.PHONY: fat32 export-fat32 echfs export-echfs run

fat32: qloader2
	rm -f ZenithOS.hdd
	dd if=/dev/zero bs=1024M count=0 seek=8 of=ZenithOS.hdd
	parted -s ZenithOS.hdd mklabel msdos
	parted -s ZenithOS.hdd mkpart primary fat32 1 50%
	parted -s ZenithOS.hdd mkpart primary fat32 50% 100%
	sudo losetup -Pf --show ZenithOS.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkfs.fat `cat loopback_dev`p1
	mkdir -p mnt
	sudo mount `cat loopback_dev`p1 mnt
	sudo cp -r src/* mnt/
	sudo sync
	sudo umount mnt
	sudo mkfs.fat `cat loopback_dev`p2
	mkdir -p mnt
	sudo mount `cat loopback_dev`p2 mnt
	sudo cp -r src/* mnt/
	sudo sync
	sudo umount mnt
	sudo losetup -d `cat loopback_dev`
	rm -rf mnt
	rm loopback_dev
	qloader2/qloader2-install qloader2/qloader2.bin ZenithOS.hdd

export-fat32:
	sudo losetup -Pf --show ZenithOS.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	mkdir -p mnt
	sudo mount `cat loopback_dev`p1 mnt
	rm -rf src
	mkdir src
	sudo cp -r mnt/* src/
	sudo sync
	sudo umount mnt
	sudo losetup -d `cat loopback_dev`
	rm -rf mnt
	rm loopback_dev
	sudo chown -R $$USER:$$USER src

echfs:
	rm -f ZenithOS.hdd
	dd if=/dev/zero bs=1024M count=0 seek=8 of=ZenithOS.hdd
	fdisk ZenithOS.hdd < fdisk.script
	echfs-utils -v -m -p0 ZenithOS.hdd quick-format 32768
	mkdir -p mnt
	echfs-fuse --mbr -p0 ZenithOS.hdd mnt
	cp -r src/* mnt/
	sync
	fusermount -u mnt
	echfs-utils -v -m -p1 ZenithOS.hdd quick-format 32768
	mkdir -p mnt
	echfs-fuse --mbr -p1 ZenithOS.hdd mnt
	cp -r src/* mnt/
	sync
	fusermount -u mnt
	rm -rf mnt
	qloader2/qloader2-install qloader2/qloader2.bin ZenithOS.hdd

export-echfs:
	mkdir -p mnt
	echfs-fuse --mbr -p0 ZenithOS.hdd mnt
	rm -rf src
	mkdir src
	cp -r mnt/* src/
	sync
	fusermount -u mnt
	rm -rf mnt
	chmod -R 777 src

run:
	qemu-system-x86_64 -net none -m 2G -enable-kvm -cpu host -smp 4 -drive file=ZenithOS.hdd,format=raw

qloader2:
	git clone https://github.com/qloader2/qloader2.git
