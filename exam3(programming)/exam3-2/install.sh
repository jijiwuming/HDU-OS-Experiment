#!/bin/bash
make
insmod blk_dev.ko
cat /proc/devices | grep my_blk
mkfs.ext2 /dev/jijiwuming
mount /dev/jijiwuming ../blk/
chmod 777 ../blk