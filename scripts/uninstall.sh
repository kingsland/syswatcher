systemctl stop syswatcher
systemctl disable syswatcher
lib_path=/usr/local/lib/syswatcher
rm -rf $lib_path
rm -f /usr/bin/syswatcher
rm -f /usr/bin/syswatcher_ldplg
rm -f /usr/bin/ldsyswatcher_plg
rm -f /lib/systemd/system/syswatcher.service
