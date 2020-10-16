#!/bin/bash
RELEASE=`git log --oneline -n1 HEAD | cut -d ' ' -f 1`
BUILDDATE=$(date "+%Y%m%d")
DIR=syswatcher_install-${RELEASE}-${BUILDDATE}-`uname -m`
TAR=${DIR}.tar
echo $DIR
make
mkdir -p ${DIR}
cp scripts/* ${DIR}
cp core/syswatcher ${DIR}
cp tools/ldsyswatcher_plg ${DIR}
cp `find ./plugins | grep .so$` ${DIR}
tar cf ${DIR}.tar ${DIR}
make clean
