#!/bin/sh

set -e

# Might as well ask for password up-front, right?
sudo -v

# Keep-alive: update existing sudo time stamp if set, otherwise do nothing.
while true; do sudo -n true; sleep 60; kill -0 "$$" || exit; done 2>/dev/null &

# Save a backup of the last img just in case
[ -f ZenithOS.hdd ] && mv ZenithOS.hdd ZenithOS.hdd.bak

# 512GiB sparse file flat image
dd if=/dev/zero bs=1024M count=0 seek=8 of=ZenithOS.hdd

# 1 partition to cover the whole image
parted -s ZenithOS.hdd mklabel msdos
parted -s ZenithOS.hdd mkpart primary fat32 1 100%

sudo losetup -Pf --show ZenithOS.hdd > loopback_dev
sudo partprobe `cat loopback_dev`

# Format it as FAT32
sudo mkfs.fat `cat loopback_dev`p1

mkdir -p mnt
sudo mount `cat loopback_dev`p1 mnt

sudo cp -rv src/* mnt/

sudo sync

sudo umount mnt
sudo losetup -d `cat loopback_dev`

rm -rf mnt
rm loopback_dev

# Install qloader2

[ -d qloader2 ] || git clone https://github.com/qloader2/qloader2.git

qloader2/qloader2-install qloader2/qloader2.bin ZenithOS.hdd
