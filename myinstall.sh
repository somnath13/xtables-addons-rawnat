#!/bin/bash

declare -r MYNAME="$( basename "$0" )"
declare -r MYDIR="$( cd "$( dirname "$0" )" && pwd )"
cd "${MYDIR}"


runcmd() {
  echo; echo "= $@"; "$@"
}

output() {
  echo; echo "    $*"; echo
}

# find an installed Software Collection
find_devtoolset() {
  local devtoolset=''
  if which scl &>/dev/null; then
    for devtoolset in "$@"; do
      if scl -l | grep ${devtoolset}; then
        return 0
      fi
    done
  fi
  return 1
}

declare retval=0

# verify running kernel version against pre-built kernel module
declare -r RAWNAT_MODULE_PATH=./extensions/xt_RAWNAT.ko
if [[ -f ${RAWNAT_MODULE_PATH} ]]; then
  declare -r EXPECTED_KERNEL_VER=$(/sbin/modinfo -F vermagic ${RAWNAT_MODULE_PATH} | awk '{ print $1 }')
  if [[ ${EXPECTED_KERNEL_VER} != "$(uname -r)" ]]; then
    output "STOP: expected kernel ${EXPECTED_KERNEL_VER}, but found $(uname -r)"
    exit 1
  fi
else
  output "STOP: cannot find pre-built kernel module ${RAWNAT_MODULE_PATH}"
  exit 1
fi

# look for devtoolset-6
declare -r DEVTOOLSET="$(find_devtoolset 'devtoolset-6')"
if [[ -n ${DEVTOOLSET} ]]; then
  # use full path for sudo because devtoolset environment clobbers sudo -- som
  runcmd scl enable devtoolset-6 '/usr/bin/sudo -n make install' || exit 13
else
  output "WARN: Software Collections devtoolset-6 package is not available"

  # GCC version AFTER upgrdading to CentOS 6.10 -- som
  declare -r EXPECTED_GCC_VER="gcc-4.4.7-23.el6.x86_64"
  if [[ ${EXPECTED_GCC_VER} != "$(rpm -q gcc)" ]]; then
    output "STOP: expected GCC ${EXPECTED_GCC_VER}, but found $(rpm -q gcc)"
    exit 1
  fi

  runcmd sudo -n make install || exit 13
fi

declare -r IPTABLES_SCRIPT='/home/mystic/work/mystic/script/iptables.sh'
[[ -x ${IPTABLES_SCRIPT} ]] && runcmd ${IPTABLES_SCRIPT} setip

