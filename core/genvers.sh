#!/bin/bash
export LANG=en_US.UTF-8
export LANGUAGE=en_US.en
DATE=`date "+%Y%m%d %H:%M:%S"`
COMMIT="no version info"
type git
if [ $? == 0 ];then
	COMMIT=$(git log --oneline | head -n1 | awk '{ print $1 }')
fi
VERS_H=vers.h
VERS_C=vers.c
echo "#ifndef VERS_H" >$VERS_H
echo "#define VERS_H" >>$VERS_H
echo "extern char *time_info;" >>$VERS_H
echo "extern char *vers_info;" >>$VERS_H
echo "#endif" >>$VERS_H
echo "#include \"vers.h\"" >$VERS_C
echo "char *time_info = \""$DATE"\";" >>$VERS_C
echo "char *vers_info = \""$COMMIT"\";" >>$VERS_C
