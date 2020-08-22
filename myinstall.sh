#!/bin/bash

declare -r SUDO_CMD='sudo -n' # don't prompt for password but exit with error if password is required
declare -r AT_CMD='/usr/bin/at'
declare -r IPTABLES_SCRIPT='/home/mystic/work/mystic/script/iptables.sh'
declare -r MIPSOS_FILE='/home/mystic/work/MIPSOS'

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

# build from inside devtoolset environment
devtoolset_build() {
  # save environment variables
  declare -r SAVED_PATH=${PATH}
  declare -r SAVED_LD_LIBRARY_PATH=${LD_LIBRARY_PATH}

  # setup devtoolset environment
  source scl_source enable ${DEVTOOLSET}

  # check GCC version
  declare -r EXPECTED_GCC_VER="6.3.1"
  if [[ ${EXPECTED_GCC_VER} != "$(gcc -dumpversion)" ]]; then
    output "STOP: expected GCC ${EXPECTED_GCC_VER}, but found $(gcc -dumpversion)"
    exit 1
  fi

  runcmd ./autogen.sh || exit 3

  runcmd ./configure || exit 5

  runcmd make clean || exit 7

  runcmd make || exit 9

  # restore environment variables
  export PATH=${SAVED_PATH}
  export LD_LIBRARY_PATH=${SAVED_LD_LIBRARY_PATH}
  return 0
}

# install from inside devtoolset environment
devtoolset_install() {
  # NOTE: run 'scl' via 'sudo' because installation requires root privilege AND devtoolset
  runcmd ${SUDO_CMD} scl enable ${DEVTOOLSET} 'make install' || exit 13
}

# verify running kernel version against pre-built kernel module
verify_kernel_version() {
  declare -r RAWNAT_MODULE_PATH=./extensions/xt_RAWNAT.ko
  if [[ -f ${RAWNAT_MODULE_PATH} ]]; then
    declare -r EXPECTED_KERNEL_VER=$(/sbin/modinfo -F vermagic ${RAWNAT_MODULE_PATH} | awk '{ print $1 }')
    if [[ ${EXPECTED_KERNEL_VER} == "$(uname -r)" ]]; then
      return 0
    else
      output "NOTE: kernel modules were built against ${EXPECTED_KERNEL_VER}, but running kernel is $(uname -r)"
    fi
  else
    output "INFO: cannot find pre-built kernel module ${RAWNAT_MODULE_PATH}"
  fi

  return 1
}


# look for devtoolset package
declare -r DEVTOOLSET="$(find_devtoolset 'devtoolset-6')"
if [[ -z ${DEVTOOLSET} ]]; then
  output "STOP: Software Collections devtoolset package is not available"
  exit 13
fi

# build with devtoolset
runcmd verify_kernel_version || runcmd devtoolset_build

# install with devtoolset
runcmd devtoolset_install

# setup source IP for multicast
[[ -f ${MIPSOS_FILE} ]] && [[ -x ${IPTABLES_SCRIPT} ]] && runcmd ${IPTABLES_SCRIPT} setip

