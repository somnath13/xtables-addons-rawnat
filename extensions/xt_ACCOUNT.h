/***************************************************************************
 *   Copyright (C) 2004-2006 by Intra2net AG                               *
 *   opensource@intra2net.com                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License                  *
 *   version 2 as published by the Free Software Foundation;               *
 *                                                                         *
 ***************************************************************************/

#ifndef _IPT_ACCOUNT_H
#define _IPT_ACCOUNT_H

/*
 * Socket option interface shared between kernel (xt_ACCOUNT) and userspace
 * library (libxt_ACCOUNT_cl). Hopefully we are unique at least within our
 * kernel & xtables-addons space.
 */
#define SO_ACCOUNT_BASE_CTL 90

#define IPT_SO_SET_ACCOUNT_HANDLE_FREE (SO_ACCOUNT_BASE_CTL + 1)
#define IPT_SO_SET_ACCOUNT_HANDLE_FREE_ALL (SO_ACCOUNT_BASE_CTL + 2)
#define IPT_SO_SET_ACCOUNT_MAX		 IPT_SO_SET_ACCOUNT_HANDLE_FREE_ALL

#define IPT_SO_GET_ACCOUNT_PREPARE_READ (SO_ACCOUNT_BASE_CTL + 4)
#define IPT_SO_GET_ACCOUNT_PREPARE_READ_FLUSH (SO_ACCOUNT_BASE_CTL + 5)
#define IPT_SO_GET_ACCOUNT_GET_DATA (SO_ACCOUNT_BASE_CTL + 6)
#define IPT_SO_GET_ACCOUNT_GET_HANDLE_USAGE (SO_ACCOUNT_BASE_CTL + 7)
#define IPT_SO_GET_ACCOUNT_GET_TABLE_NAMES (SO_ACCOUNT_BASE_CTL + 8)
#define IPT_SO_GET_ACCOUNT_MAX	  IPT_SO_GET_ACCOUNT_GET_TABLE_NAMES

#define ACCOUNT_MAX_TABLES 128
#define ACCOUNT_TABLE_NAME_LEN 32
#define ACCOUNT_MAX_HANDLES 10

/* Structure for the userspace part of ipt_ACCOUNT */
struct ipt_acc_info {
	uint32_t net_ip;
	uint32_t net_mask;
	char table_name[ACCOUNT_TABLE_NAME_LEN];
	int32_t table_nr;
};

/* Internal table structure, generated by check_entry() */
struct ipt_acc_table {
	char name[ACCOUNT_TABLE_NAME_LEN];	 /* name of the table */
	uint32_t ip;						  /* base IP of network */
	uint32_t netmask;					 /* netmask of the network */
	unsigned char depth;				   /* size of network:
												 0: 8 bit, 1: 16bit, 2: 24 bit */
	uint32_t refcount;					/* refcount of this table.
												 if zero, destroy it */
	uint32_t itemcount;				   /* number of IPs in this table */
	void *data;							/* pointer to the actual data,
												 depending on netmask */
};

/* Internal handle structure */
struct ipt_acc_handle {
	uint32_t ip;						  /* base IP of network. Used for
												 caculating the final IP during
												 get_data() */
	unsigned char depth;				   /* size of network. See above for
												 details */
	uint32_t itemcount;				   /* number of IPs in this table */
	void *data;							/* pointer to the actual data,
												 depending on size */
};

/* Handle structure for communication with the userspace library */
struct ipt_acc_handle_sockopt {
	uint32_t handle_nr;				   /* Used for HANDLE_FREE */
	char name[ACCOUNT_TABLE_NAME_LEN];	 /* Used for HANDLE_PREPARE_READ/
												 HANDLE_READ_FLUSH */
	uint32_t itemcount;				   /* Used for HANDLE_PREPARE_READ/
												 HANDLE_READ_FLUSH */
};

/* Used for every IP entry
   Size is 16 bytes so that 256 (class C network) * 16
   fits in one kernel (zero) page */
struct ipt_acc_ip {
	uint32_t src_packets;
	uint32_t src_bytes;
	uint32_t dst_packets;
	uint32_t dst_bytes;
};

/*
	Used for every IP when returning data
*/
struct ipt_acc_handle_ip {
	uint32_t ip;
	uint32_t src_packets;
	uint32_t src_bytes;
	uint32_t dst_packets;
	uint32_t dst_bytes;
};

/*
	The IPs are organized as an array so that direct slot
	calculations are possible.
	Only 8 bit networks are preallocated, 16/24 bit networks
	allocate their slots when needed -> very efficent.
*/
struct ipt_acc_mask_24 {
	struct ipt_acc_ip ip[256];
};

struct ipt_acc_mask_16 {
	struct ipt_acc_mask_24 *mask_24[256];
};

struct ipt_acc_mask_8 {
	struct ipt_acc_mask_16 *mask_16[256];
};

#endif /* _IPT_ACCOUNT_H */