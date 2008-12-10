/*
 *	"SYSRQ" target extension for Netfilter
 *	Copyright © Jan Engelhardt <jengelh [at] medozas de>, 2008
 *
 *	Based upon the ipt_SYSRQ idea by Marek Zalem <marek [at] terminus sk>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	version 2 or 3 as published by the Free Software Foundation.
 */
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/sysrq.h>
#include <linux/udp.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <net/ip.h>
#include "compat_xtables.h"

static bool sysrq_once;
static char sysrq_password[64];
static char sysrq_hash[16] = "sha1";
static long seqno;
static int debug;
module_param_string(password, sysrq_password, sizeof(sysrq_password),
	S_IRUSR | S_IWUSR);
module_param_string(hash, sysrq_hash, sizeof(sysrq_hash), S_IRUSR);
module_param(seqno, long, S_IRUSR | S_IWUSR);
module_param(debug, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(password, "password for remote sysrq");
MODULE_PARM_DESC(hash, "hash algorithm, default sha1");
MODULE_PARM_DESC(seqno, "sequence number for remote sysrq");
MODULE_PARM_DESC(debug, "debugging: 0=off, 1=on");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
static struct crypto_hash *tfm;
static int digestsize;
static unsigned char *digest_password;
static unsigned char *digest;
static char *hexdigest;

/*
 * The data is of the form "<requests>,<seqno>,<salt>,<hash>" where <requests>
 * is a series of sysrq requests; <seqno> is a sequence number that must be
 * greater than the last sequence number; <salt> is some random bytes; and
 * <hash> is the hash of everything up to and including the preceding ","
 * together with the password.
 *
 * For example
 *
 *   salt=$RANDOM
 *   req="s,$(date +%s),$salt"
 *   echo "$req,$(echo -n $req,secret | sha1sum | cut -c1-40)"
 *
 * You will want a better salt and password than that though :-)
 */
static unsigned int sysrq_tg(const void *pdata, uint16_t len)
{
	const char *data = pdata;
	int i, n;
	struct scatterlist sg[2];
	struct hash_desc desc;
	int ret;
	long new_seqno = 0;

	if (*sysrq_password == '\0') {
		if (!sysrq_once)
			printk(KERN_INFO KBUILD_MODNAME ": No password set\n");
		sysrq_once = true;
		return NF_DROP;
	}
	if (len == 0)
		return NF_DROP;

	for (i = 0; sysrq_password[i] != '\0' &&
	     sysrq_password[i] != '\n'; ++i)
		/* loop */;
	sysrq_password[i] = '\0';

	i = 0;
	for (n = 0; n < len - 1; ++n) {
		if (i == 1 && '0' <= data[n] && data[n] <= '9')
			new_seqno = 10L * new_seqno + data[n] - '0';
		if (data[n] == ',' && ++i == 3)
			break;
	}
	++n;
	if (i != 3) {
		if (debug)
			printk(KERN_WARNING KBUILD_MODNAME
				": badly formatted request\n");
		return NF_DROP;
	}
	if (seqno >= new_seqno) {
		if (debug)
			printk(KERN_WARNING KBUILD_MODNAME
				": old sequence number ignored\n");
		return NF_DROP;
	}

	desc.tfm = tfm;
	desc.flags = 0;
	ret = crypto_hash_init(&desc);
	if (ret != 0)
		goto hash_fail;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	sg_init_table(sg, 2);
#endif
	sg_set_buf(&sg[0], data, n);
	strcpy(digest_password, sysrq_password);
	i = strlen(digest_password);
	sg_set_buf(&sg[1], digest_password, i);
	ret = crypto_hash_digest(&desc, sg, n + i, digest);
	if (ret != 0)
		goto hash_fail;

	for (i = 0; i < digestsize; ++i) {
		hexdigest[2*i] =
			"0123456789abcdef"[(digest[i] >> 4) & 0xf];
		hexdigest[2*i+1] =
			"0123456789abcdef"[digest[i] & 0xf];
	}
	hexdigest[2*digestsize] = '\0';
	if (len - n < digestsize) {
		if (debug)
			printk(KERN_INFO KBUILD_MODNAME ": Short digest,"
			       " expected %s\n", hexdigest);
		return NF_DROP;
	}
	if (strncmp(data + n, hexdigest, digestsize) != 0) {
		if (debug)
			printk(KERN_INFO KBUILD_MODNAME ": Bad digest,"
			       " expected %s\n", hexdigest);
		return NF_DROP;
	}

	/* Now we trust the requester */
	seqno = new_seqno;
	for (i = 0; i < len && data[i] != ','; ++i) {
		printk(KERN_INFO KBUILD_MODNAME ": SysRq %c\n", data[i]);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
		handle_sysrq(data[i], NULL);
#else
		handle_sysrq(data[i], NULL, NULL);
#endif
	}
	return NF_ACCEPT;

 hash_fail:
	printk(KERN_WARNING KBUILD_MODNAME ": digest failure\n");
	return NF_DROP;
}
#else
static unsigned int sysrq_tg(const void *pdata, uint16_t len)
{
	const char *data = pdata;
	char c;

	if (*sysrq_password == '\0') {
		if (!sysrq_once)
			printk(KERN_INFO KBUILD_MODNAME "No password set\n");
		sysrq_once = true;
		return NF_DROP;
	}

	if (len == 0)
		return NF_DROP;

	c = *data;
	if (strncmp(&data[1], sysrq_password, len - 1) != 0) {
		printk(KERN_INFO KBUILD_MODNAME "Failed attempt - "
		       "password mismatch\n");
		return NF_DROP;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
	handle_sysrq(c, NULL);
#else
	handle_sysrq(c, NULL, NULL);
#endif
	return NF_ACCEPT;
}
#endif

static unsigned int
sysrq_tg4(struct sk_buff **pskb, const struct xt_target_param *par)
{
	struct sk_buff *skb = *pskb;
	const struct iphdr *iph;
	const struct udphdr *udph;
	uint16_t len;

	if (skb_linearize(skb) < 0)
		return NF_DROP;

	iph  = ip_hdr(skb);
	udph = (void *)iph + ip_hdrlen(skb);
	len  = ntohs(udph->len) - sizeof(struct udphdr);

	if (debug)
		printk(KERN_INFO KBUILD_MODNAME
		       ": " NIPQUAD_FMT ":%u -> :%u len=%u\n",
		       NIPQUAD(iph->saddr), htons(udph->source),
		       htons(udph->dest), len);
	return sysrq_tg((void *)udph + sizeof(struct udphdr), len);
}

static unsigned int
sysrq_tg6(struct sk_buff **pskb, const struct xt_target_param *par)
{
	struct sk_buff *skb = *pskb;
	const struct ipv6hdr *iph;
	const struct udphdr *udph;
	uint16_t len;

	if (skb_linearize(skb) < 0)
		return NF_DROP;

	iph  = ipv6_hdr(skb);
	udph = udp_hdr(skb);
	len  = ntohs(udph->len) - sizeof(struct udphdr);

	if (debug)
		printk(KERN_INFO KBUILD_MODNAME
		       ": " NIP6_FMT ":%hu -> :%hu len=%u\n",
		       NIP6(iph->saddr), ntohs(udph->source),
		       ntohs(udph->dest), len);
	return sysrq_tg(udph + sizeof(struct udphdr), len);
}

static bool sysrq_tg_check(const struct xt_tgchk_param *par)
{

	if (par->target->family == NFPROTO_IPV4) {
		const struct ipt_entry *entry = par->entryinfo;

		if ((entry->ip.proto != IPPROTO_UDP &&
		    entry->ip.proto != IPPROTO_UDPLITE) ||
		    entry->ip.invflags & XT_INV_PROTO)
			goto out;
	} else if (par->target->family == NFPROTO_IPV6) {
		const struct ip6t_entry *entry = par->entryinfo;

		if ((entry->ipv6.proto != IPPROTO_UDP &&
		    entry->ipv6.proto != IPPROTO_UDPLITE) ||
		    entry->ipv6.invflags & XT_INV_PROTO)
			goto out;
	}

	return true;

 out:
	printk(KERN_ERR KBUILD_MODNAME ": only available for UDP and UDP-Lite");
	return false;
}

static struct xt_target sysrq_tg_reg[] __read_mostly = {
	{
		.name       = "SYSRQ",
		.revision   = 0,
		.family     = NFPROTO_IPV4,
		.target     = sysrq_tg4,
		.checkentry = sysrq_tg_check,
		.me         = THIS_MODULE,
	},
	{
		.name       = "SYSRQ",
		.revision   = 0,
		.family     = NFPROTO_IPV6,
		.target     = sysrq_tg6,
		.checkentry = sysrq_tg_check,
		.me         = THIS_MODULE,
	},
};

static int __init sysrq_tg_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
	struct timeval now;

	tfm = crypto_alloc_hash(sysrq_hash, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		printk(KERN_WARNING KBUILD_MODNAME
			": Error: Could not find or load %s hash\n",
			sysrq_hash);
		tfm = NULL;
		goto fail;
	}
	digestsize = crypto_hash_digestsize(tfm);
	digest = kmalloc(digestsize, GFP_KERNEL);
	if (digest == NULL) {
		printk(KERN_WARNING KBUILD_MODNAME
			": Cannot allocate digest\n");
		goto fail;
	}
	hexdigest = kmalloc(2 * digestsize + 1, GFP_KERNEL);
	if (hexdigest == NULL) {
		printk(KERN_WARNING KBUILD_MODNAME
			": Cannot allocate hexdigest\n");
		goto fail;
	}
	digest_password = kmalloc(sizeof(sysrq_password), GFP_KERNEL);
	if (!digest_password) {
		printk(KERN_WARNING KBUILD_MODNAME
			": Cannot allocate password digest space\n");
		goto fail;
	}
	do_gettimeofday(&now);
	seqno = now.tv_sec;
	return xt_register_targets(sysrq_tg_reg, ARRAY_SIZE(sysrq_tg_reg));

 fail:
	if (tfm)
		crypto_free_hash(tfm);
	if (digest)
		kfree(digest);
	if (hexdigest)
		kfree(hexdigest);
	if (digest_password)
		kfree(digest_password);
	return -EINVAL;
#else
	printk(KERN_WARNING "xt_SYSRQ does not provide crypto for <= 2.6.18\n");
	return xt_register_targets(sysrq_tg_reg, ARRAY_SIZE(sysrq_tg_reg));
#endif
}

static void __exit sysrq_tg_exit(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
	crypto_free_hash(tfm);
	kfree(digest);
	kfree(hexdigest);
	kfree(digest_password);
#endif
	return xt_unregister_targets(sysrq_tg_reg, ARRAY_SIZE(sysrq_tg_reg));
}

module_init(sysrq_tg_init);
module_exit(sysrq_tg_exit);
MODULE_DESCRIPTION("Xtables: triggering SYSRQ remotely");
MODULE_AUTHOR("Jan Engelhardt <jengelh@medozas.de>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_SYSRQ");
MODULE_ALIAS("ip6t_SYSRQ");
