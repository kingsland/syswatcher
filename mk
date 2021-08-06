#!/bin/bash
CONF_PREFIX=conf
COMPILE_CONF=config
MODULES=$(cat $COMPILE_CONF | grep -n "^\[.*\]$" | awk -F '[":"\\[\\]]' '{print $1":"$3}')
MODULES_TO_LOAD=
gen_conf() {
	ENDL=`cat $COMPILE_CONF | wc -l`
	for MODULE in ${MODULES}
	do
		CONF="${MODULE#*:}.conf"
		SESSION_START=`echo $MODULE | cut -d ":" -f 1`
		SESSION_END=`cat ${COMPILE_CONF} | awk -v i=${SESSION_START} '/^\[.*\]$/ {if(NR>i)print NR}' |head -n1`
		SESSION_END=`echo $SESSION_END-1 | bc`
		if [ $SESSION_END -eq -1 ];then
			SESSION_END=$ENDL
		fi
		if [ $SESSION_END -gt $SESSION_START ];then
			SESSION_START=`echo $SESSION_START+1 | bc`
			sed -n "${SESSION_START},${SESSION_END}p" $COMPILE_CONF >$CONF_PREFIX/plugins/$CONF
			else
			echo "" >$CONF_PREFIX/plugins/$CONF
		fi
        MODULES_TO_LOAD="$MODULES_TO_LOAD lib${MODULE#*:}.so"
	done
    echo "MODULES_TO_LOAD =$MODULES_TO_LOAD" >$CONF_PREFIX/syswatcher.conf
}
RELEASE=`git log --oneline -n1 HEAD | cut -d ' ' -f 1`
BUILDDATE=$(date "+%Y%m%d")
DIR=syswatcher_install-${RELEASE}-${BUILDDATE}-`uname -m`
TAR=${DIR}.tar
gen_conf
make

mkdir -p ${DIR}
cp scripts/* ${DIR}
cp -r conf ${DIR}
cp core/syswatcher ${DIR}
cp tools/ldsyswatcher_plg ${DIR}
for MODULE in $MODULES
do
	cp plugins/mod_${MODULE#*:}/lib${MODULE#*:}.so ${DIR}
done
tar cf ${DIR}.tar ${DIR}
make clean
rm -rf ${DIR}
rm conf/plugins/*

