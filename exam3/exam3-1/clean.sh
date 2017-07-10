#!/bin/bash
rmmod c_driver
lsmod | grep c_driver
make clean
rm -f /dev/mycdev