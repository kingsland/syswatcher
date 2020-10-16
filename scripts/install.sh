systemctl stop syswatcher
lib_path=/usr/local/lib/syswatcher
cp ./syswatcher /usr/bin
cp ./syswatcher_ldplg /usr/bin
cp ./ldsyswatcher_plg /usr/bin
mkdir $lib_path -p
cp *.so $lib_path
cp syswatcher.service /lib/systemd/system/
systemctl daemon-reload

