#!/bin/bash
export LANG=en_US.UTF-8
export LANGUAGE=en_US.en
DATE=`date "+%Y%m%d %H:%M:%S"`
COMMIT="no version info"
type git
if [ $? == 0 ];then
	COMMIT=$(git log --oneline | head -n1 | awk '{ print $1 }')
fi
VERS=vers.h
echo "#ifndef VERS_H" >$VERS
echo "#define VERS_H" >>$VERS
echo "char *time_info = \""$DATE"\";" >>$VERS
echo "char *vers_info = \""$COMMIT"\";" >>$VERS
echo "#endif" >>$VERS
