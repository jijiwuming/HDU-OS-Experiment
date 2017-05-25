#!/bin/bash
make
insmod c_driver.ko
cat /proc/devices | grep my_driver
mknod /dev/mycdev c 245 0
chmod 777 /dev/mycdev
make test