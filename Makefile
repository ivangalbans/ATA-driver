AS = nasm
CC = gcc
CC_FLAGS = -Wall -c -m32 -ffreestanding -I src/kernel/include -nostdinc -ggdb
LD = ld

### Bootloader ###

build/mbr.bin: src/boot/mbr.asm
	${AS} -f bin -o build/mbr.bin src/boot/mbr.asm

build/vbr.bin: src/boot/vbr.asm
	${AS} -f bin -o build/vbr.bin src/boot/vbr.asm

### Kernel ###

build/kernel: build/kernel.elf build/kernel_init.bin
	objcopy -O binary build/kernel.elf build/kernel.bin
	cat build/kernel_init.bin build/kernel.bin > build/kernel

build/kernel_init.bin: src/kernel/kernel_init.asm
	${AS} -f bin -o build/kernel_init.bin src/kernel/kernel_init.asm

build/kernel.elf: build/kernel.o \
								  build/kernel_entry.o \
									build/string.o \
									build/io.o \
									build/hw.o \
									build/fb.o \
									build/mem.o \
									build/mem_asm.o
	${LD} -m elf_i386 -T src/kernel/kernel.ld -nostdlib -static \
				-o build/kernel.elf \
				build/kernel_entry.o \
				build/kernel.o \
				build/fb.o \
				build/string.o \
				build/io.o \
				build/hw.o \
				build/mem.o \
				build/mem_asm.o

build/kernel_entry.o: src/kernel/kernel_entry.asm
	${AS} -f elf -o build/kernel_entry.o src/kernel/kernel_entry.asm

build/kernel.o: src/kernel/kernel.c src/kernel/include/*.h
	${CC} ${CC_FLAGS} -o build/kernel.o src/kernel/kernel.c

build/string.o: src/kernel/string.c src/kernel/include/string.h
	${CC} ${CC_FLAGS} -o build/string.o src/kernel/string.c

build/io.o: src/kernel/io.asm src/kernel/include/io.h
	${AS} -f elf -o build/io.o src/kernel/io.asm

build/hw.o: src/kernel/hw.asm src/kernel/include/hw.h
	${AS} -f elf -o build/hw.o src/kernel/hw.asm

build/fb.o: src/kernel/drivers/fb.c src/kernel/include/fb.h
	${CC} ${CC_FLAGS} -o build/fb.o src/kernel/drivers/fb.c

build/mem.o: src/kernel/drivers/mem.c src/kernel/include/mem.h
	${CC} ${CC_FLAGS} -o build/mem.o src/kernel/drivers/mem.c

build/mem_asm.o: src/kernel/drivers/mem.asm src/kernel/include/mem.h
	${AS} -f elf -o build/mem_asm.o src/kernel/drivers/mem.asm

### Clean ###

.PHONY: clean
clean:
	rm build/*
	rm tests/images/*

### Tools ###

tools/install-buhos-bootloader: tools/src/install-buhos-bootloader.c
	${CC} -o tools/install-buhos-bootloader tools/src/install-buhos-bootloader.c


### One shot rules ###

# Run this once in the beginning.
tests/images/disk.img: build/mbr.bin build/vbr.bin tools/install-buhos-bootloader
	./tools/build-disk.sh build tests/images/disk.img build/mbr.bin build/vbr.bin

### Tests ###

tests/.last-build: build/kernel
	./tools/build-disk.sh kernel tests/images/disk.img build/kernel
	touch tests/.last-build

.PHONY: qemu
qemu: tests/.last-build
	qemu-system-i386 -drive index=0,media=disk,file=tests/images/disk.img,if=ide,format=raw -m 16

qemu-debug: tests/.last-build
	qemu-system-i386 -drive index=0,media=disk,file=tests/images/disk.img,if=ide,format=raw -m 16 -s -S &
	gdb --command=tests/gdb.txt

bochs: tests/.last-build
	bochs -f tests/bochsrc.txt



# vim: noexpandtab
