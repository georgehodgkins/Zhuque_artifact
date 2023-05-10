#!/bin/bash
PMEM_DEV=/dev/pmem1
if [ -f 'pmem.dev' ]; then
    PMEM_DEV=$(cat pmem.dev)
fi

{
#    umount -f /mnt/ramdisk
	umount -f /mnt/pmem1
    mkdir -p /mnt/ram
    mkfs.ext4 -F $PMEM_DEV
    mount -t ext4 -o dax $PMEM_DEV /mnt/ram
    chmod -R 777 /mnt/ram
}

