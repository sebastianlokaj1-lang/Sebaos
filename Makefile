CC      = gcc
CFLAGS  = -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-delete-null-pointer-checks \
          -fno-tree-loop-distribute-patterns -mno-sse -mno-mmx -O2 -Wall -Wno-array-bounds -Ikernel
SRC     = kernel/sys.c kernel/gfx.c kernel/gui.c kernel/apps.c kernel/main.c
OBJ     = $(SRC:.c=.o) kernel/entry.o

all: sebaos.img

kernel/%.o: kernel/%.c kernel/kernel.h
	$(CC) $(CFLAGS) -c $< -o $@
kernel/entry.o: kernel/entry.asm
	nasm -f elf32 $< -o $@
kernel.elf: $(OBJ) kernel/linker.ld
	ld -m elf_i386 -T kernel/linker.ld -nostdlib -o $@ $(OBJ)
kernel.bin: kernel.elf
	objcopy -O binary $< $@
sebaos.img: kernel.bin boot/boot.asm boot/stage2.asm
	nasm -f bin boot/stage2.asm -o stage2.bin
	nasm -f bin boot/boot.asm -DKSECT=$$(( ($$(stat -c %s kernel.bin) + 511) / 512 )) -o boot.bin
	cat boot.bin stage2.bin kernel.bin > $@
	truncate -s 1M $@
	@echo "Hotovo: $@  (jadro: $$(stat -c %s kernel.bin) B)"
run: sebaos.img
	qemu-system-i386 -drive file=sebaos.img,format=raw,if=ide -m 64 -rtc base=localtime
clean:
	rm -f kernel/*.o *.elf *.bin sebaos.img
