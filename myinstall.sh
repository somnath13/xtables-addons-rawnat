#!/bin/bash

declare -r SUDO_CMD='sudo -n' # don't prompt for password but exit with error if password is required
declare -r AT_CMD='/usr/bin/at'

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

# build and install from inside devtoolset environment
build_and_install() {
  # check GCC version
  declare -r EXPECTED_GCC_VER="6.3.1"
  if [[ ${EXPECTED_GCC_VER} != "$(gcc -dumpversion)" ]]; then
    output "STOP: expected GCC ${EXPECTED_GCC_VER}, but found $(gcc -dumpversion)"
    return 1
  fi

  runcmd ./autogen.sh || return 3

  runcmd ./configure || return 5

  runcmd make clean || return 7

  runcmd make install || return 9

  return 0
}

# NOTE unused reference code -- som
verify_kernel_version() {
  # verify running kernel version against pre-built kernel module
  declare -r RAWNAT_MODULE_PATH=./extensions/xt_RAWNAT.ko
  if [[ -f ${RAWNAT_MODULE_PATH} ]]; then
    declare -r EXPECTED_KERNEL_VER=$(/sbin/modinfo -F vermagic ${RAWNAT_MODULE_PATH} | awk '{ print $1 }')
    if [[ ${EXPECTED_KERNEL_VER} != "$(uname -r)" ]]; then
      output "STOP: expected kernel ${EXPECTED_KERNEL_VER}, but found $(uname -r)"
      exit 1
    fi
  else
    output "INFO: cannot find pre-built kernel module ${RAWNAT_MODULE_PATH}"
  fi
}


declare -i retval=0

# invoked from devtoolset environment?
if [[ ${1:-unset} == 'build_and_install' ]]; then
  runcmd build_and_install; retval=$?
  exit ${retval}
fi


# look for devtoolset-6
declare -r DEVTOOLSET="$(find_devtoolset 'devtoolset-6')"
if [[ -z ${DEVTOOLSET} ]]; then
  output "STOP: Software Collections devtoolset-6 package is not available"
  exit 13
fi

runcmd ${SUDO_CMD} scl enable devtoolset-6 "./${MYNAME} build_and_install"; retval=$?
if [[ ${retval} -eq 0 ]]; then
  declare -r IPTABLES_SCRIPT='/home/mystic/work/mystic/script/iptables.sh'
  [[ -x ${IPTABLES_SCRIPT} ]] && runcmd ${IPTABLES_SCRIPT} setip
fi

exit ${retval}


