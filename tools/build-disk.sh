#!/bin/bash

function build_disk() {

  DISK=$1
  MBR=$2
  BOOTLOADER=$3

  dd if=/dev/zero of="$DISK" bs=1M count=1 seek=99 || exit 1

  dd if="$MBR" of="$DISK" conv=notrunc

  /sbin/sfdisk "$DISK" <<EOF
# partition table of disk.img
unit: sectors

test.img1 : start=        2048, size=       40960, type=83
test.img2 : start=       43008, size=       61440, type=83
test.img3 : start=      104448, size=       81920, type=81, bootable
test.img4 : start=      186368, size=       18432, type=83
EOF
  test $? = 0 || { echo "Could not create partitions." && exit 1; }

  # Get the line with partition info.
  PSTART=`/sbin/sfdisk -l "$DISK" | \
          awk '/.*\*.*81 Minix.*/ { match($0,/[^*]+[^[:digit:]]+([[:digit:]]+)\s+[[:digit:]]+\s+([[:digit:]]+).*/,a); print a[1]; }'`
  test x$PSTART = x && { echo "Could not determine partition starting sector" && exit 1; }

  PSIZE=`/sbin/sfdisk -l "$DISK" | \
          awk '/.*\*.*81 Minix.*/ { match($0,/[^*]+[^[:digit:]]+([[:digit:]]+)\s+[[:digit:]]+\s+([[:digit:]]+).*/,a); print a[2]; }'`
  test x$PSIZE = x && { echo "Could not determine partition size" && exit 1; }

  # Create a temporary file.
  TMPFILE=`tempfile` || exit 1
  dd if=/dev/zero of="$TMPFILE" bs=512 count=$PSIZE || exit 1

  # Format it.
  /sbin/mkfs.minix "$TMPFILE" || exit 1

  # Copy formatted partition to disk.
  dd if="$TMPFILE" of="$DISK" bs=512 seek=$PSTART conv=notrunc || exit 1

  # Remove temporary file.
  rm "$TMPFILE" || exit 1

  tools/install-buhos-bootloader "$DISK" "$BOOTLOADER" $PSTART || exit 1

  exit 0
}

function install_kernel() {
  DISK=$1
  KERNEL=$2

  # Get the line with partition info.
  PARTNO=`/sbin/sfdisk -l "$DISK" | \
          awk '/.*\*.*81 Minix.*/ { match($1,/[^[:digit:]]+([[:digit:]]+)/,a); print a[1]; }'`
  test x$PARTNO = x && { echo "Could not determine which one is the bootable partition." && exit 1; }
  echo "Bootable partition: $PARTNO"

  # Create the loop device.
  LOOPDEV=`udisksctl loop-setup -f "$DISK" | awk '{ print $NF }'`
  test x$LOOPDEV = x && { echo "Could not create loop device." && exit 1; }
  # Strip the ending point udisksctl add to the sentence - if there.
  LOOPDEV=${LOOPDEV%.}
  echo "Loop device: $LOOPDEV"

  # Mount the partition.
  PARTDEV=${LOOPDEV}p${PARTNO}
  echo "Partition in loop format: $PARTDEV"

  # Check if it was automounted.
  if test `mount | grep "^${PARTDEV}" | wc -l` = 1; then
    MOUNTPOINT=`mount | grep "^${PARTDEV}" | awk '{ print $3 }'`
    echo "Partition is automounted at $MOUNTPOINT"
  else
    MOUNTPOINT=`udisksctl mount -b ${PARTDEV} | awk '{ print $NF }'`
    test x$MOUNTPOINT = x && { echo "Could not mount partition." && exit 1; }
    MOUNTPOINT=${MOUNTPOINT%.}
    echo "Mount point: $MOUNTPOINT"
  fi

  # Copy the kernel.
  cp -v "$KERNEL" "${MOUNTPOINT}/kernel" || { echo "Could not copy kernel." && exit 1; }

  sync -f "${MOUNTPOINT}/kernel"

  # Unmount partition.
  ( udisksctl unmount --force -b ${PARTDEV} ) || { echo "Could not unmount partition." && exit 1; }

  # Delete loop device.
  ( udisksctl loop-delete -b ${LOOPDEV} ) || { echo "Could not remove loop device." && exit 1; }

  exit 0
}

case $1 in
  build) build_disk $2 $3 $4;;
  kernel) install_kernel $2 $3;;
  *) echo "usage: $0 build|kernel DISK [ARGS ...]";
     echo "  build: Builds the image. ARGS must be:";
     echo "           Path to MBR.";
     echo "           Path to buhos bootloader.";
     echo "  kernel: Installs the kernel. ARGS must be:";
     echo "           Path to the kernel.";
    ;;
esac
