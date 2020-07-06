OS := $(shell uname)

.PHONY: fat32 export-fat32 echfs export-echfs run deps clean

fat32: deps
	mkdir -p mnt_c mnt_d
	rm -f ZenithOS.hdd
	dd if=/dev/zero bs=1M count=0 seek=8192 of=ZenithOS.hdd
ifeq ($(OS), Linux)
	parted -s ZenithOS.hdd mklabel msdos
	parted -s ZenithOS.hdd mkpart primary fat32 1 50%
	parted -s ZenithOS.hdd mkpart primary fat32 50% 100%
	sudo losetup -Pf --show ZenithOS.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat `cat loopback_dev`p1
	sudo mount `cat loopback_dev`p1 mnt_c
	sudo mkfs.fat `cat loopback_dev`p2
	sudo mount `cat loopback_dev`p2 mnt_d
else ifeq ($(OS), FreeBSD)
	sudo mdconfig -a -t vnode -f ZenithOS.hdd -u md9
	sudo gpart create -s mbr md9
	sudo gpart add -a 4k -t '!12' -s 4096M md9
	sudo gpart add -a 4k -t '!12' md9
	sudo newfs_msdos -F 32 /dev/md9s1
	sudo mount -t msdosfs /dev/md9s1 mnt_c
	sudo newfs_msdos -F 32 /dev/md9s2
	sudo mount -t msdosfs /dev/md9s2 mnt_d
endif
	sudo cp -r src/* mnt_c/
	sudo cp -r src/* mnt_d/
	sudo sync
	sudo umount mnt_c
	sudo umount mnt_d
ifeq ($(OS), Linux)
	sudo losetup -d `cat loopback_dev`
	rm loopback_dev
else ifeq ($(OS), FreeBSD)
	sudo mdconfig -d -u md9
endif
	rm -rf mnt
	qloader2/qloader2-install qloader2/qloader2.bin ZenithOS.hdd

export-fat32:
	mkdir -p mnt
ifeq ($(OS), Linux)
	sudo losetup -Pf --show ZenithOS.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mount `cat loopback_dev`p1 mnt
else ifeq ($(OS), FreeBSD)
	sudo mdconfig -a -t vnode -f ZenithOS.hdd -u md9
	sudo mount -t msdosfs /dev/md9s1 mnt
endif
	rm -rf src
	mkdir src
	sudo cp -r mnt/* src/
	sudo sync
	sudo umount mnt
ifeq ($(OS), Linux)
	sudo losetup -d `cat loopback_dev`
	rm loopback_dev
else ifeq ($(OS), FreeBSD)
	sudo mdconfig -d -u md9
endif
	rm -rf mnt
	sudo chown -R $$USER:$$USER src

echfs: deps
	rm -f ZenithOS.hdd
	dd if=/dev/zero bs=1024M count=0 seek=8 of=ZenithOS.hdd
ifeq ($(OS), Linux)
	fdisk ZenithOS.hdd < fdisk.script
	echfs/echfs-utils -v -m -p0 ZenithOS.hdd quick-format 32768
	echfs/echfs-utils -v -m -p1 ZenithOS.hdd quick-format 32768
	mkdir -p mnt
	echfs/echfs-fuse --mbr -p0 ZenithOS.hdd mnt
	cp -r src/* mnt/
	sync
	fusermount -u mnt
	echfs/echfs-fuse --mbr -p1 ZenithOS.hdd mnt
	cp -r src/* mnt/
	sync
	fusermount -u mnt
	rm -rf mnt
else ifeq ($(OS), FreeBSD)
	sudo mdconfig -a -t vnode -f ZenithOS.hdd -u md9
	sudo gpart create -s mbr md9
	sudo gpart add -a 4k -t '!105' -s 512M md9
	sudo gpart add -a 4k -t '!105' md9
	sudo mdconfig -d -u md9
	echfs/echfs-utils -v -m -p0 ZenithOS.hdd quick-format 32768
	echfs/echfs-utils -v -m -p1 ZenithOS.hdd quick-format 32768
	./copy-root-to-img.sh src ZenithOS.hdd 0
	./copy-root-to-img.sh src ZenithOS.hdd 1
endif
	qloader2/qloader2-install qloader2/qloader2.bin ZenithOS.hdd

export-echfs:
	mkdir -p mnt
	echfs/echfs-fuse --mbr -p0 ZenithOS.hdd mnt
	rm -rf src
	mkdir src
	cp -r mnt/* src/
	sync
	fusermount -u mnt
	rm -rf mnt
	chmod -R 777 src

run:
	qemu-system-x86_64 -net none -m 2G -enable-kvm -cpu host -smp 4 -drive file=ZenithOS.hdd,format=raw
run-nokvm:
	qemu-system-x86_64 -net none -m 2G -smp 4 -drive file=ZenithOS.hdd,format=raw

clean:
	rm -f ZenithOS.hdd

deps:
	git submodule init
	git submodule update
	$(MAKE) -C echfs
