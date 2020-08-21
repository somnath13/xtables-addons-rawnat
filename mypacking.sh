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

# generate a datecode
datecode() {
  date +%Y-%m-%d-%H-%M-%S
}

declare -r PACKAGE_NAME=xtables-addons-rawnat-$(datecode)
rm -f ../${PACKAGE_NAME}.tgz
git archive --format=tar --prefix=xtables-addons-rawnat/ HEAD | gzip > ../${PACKAGE_NAME}.tgz

runcmd ls -lh ../${PACKAGE_NAME}.tgz
runcmd md5sum ../${PACKAGE_NAME}.tgz

