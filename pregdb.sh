#!/usr/bin/env bash

ulimit -c unlimited

YFSDIR1=$PWD/yfs1

rm -rf $YFSDIR1
mkdir $YFSDIR1 || exit 1
sleep 1
echo "starting ./yfs_client $YFSDIR1  > yfs_client1.log 2>&1 &"
sleep 2
