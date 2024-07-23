#!/bin/sh
host=$1
scp -oHostkeyalgorithms=+ssh-rsa -r luasrc/* root@$host:/usr/lib/lua/luci/
scp -oHostkeyalgorithms=+ssh-rsa -r root/usr/* root@$host:/usr/
scp -oHostkeyalgorithms=+ssh-rsa -r root/etc/init.d/* root@$host:/etc/init.d/
ssh -oHostkeyalgorithms=+ssh-rsa root@$host rm -rf /tmp/luci-indexcache
