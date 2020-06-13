#!/bin/sh

set -e

# Save a backup of the last img just in case
[ -f ZenithOS.hdd ] && mv ZenithOS.hdd ZenithOS.hdd.bak

# 4GiB sparse file flat image
dd if=/dev/zero bs=1024M count=0 seek=4 of=ZenithOS.hdd

# 1 partition to cover the whole image
cat << EOF | fdisk ZenithOS.hdd
o
n
p
1


t
0x69
w
EOF

# Format it as FAT32
echfs-utils -v -m -p0 ZenithOS.hdd quick-format 32768

./copy-root-to-img.sh src ZenithOS.hdd 0

# Install qloader2

[ -d qloader2 ] || git clone https://github.com/qloader2/qloader2.git

qloader2/qloader2-install qloader2/qloader2.bin ZenithOS.hdd
