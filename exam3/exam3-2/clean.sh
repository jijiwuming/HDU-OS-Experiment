#!/bin/bash
umount /dev/jijiwuming
rmmod blk_dev
lsmod | grep blk_dev
make clean