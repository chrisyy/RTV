include config.mk

TARGET := x86_64-elf
ARCH := x86_64
CC := $(TARGET)-gcc
LD := $(TARGET)-ld

PROG := rtv

CSRCS := $(shell find . -name *.c)
ASRCS := $(shell find . -name *.S)
OBJS := $(CSRCS:%.c=%.o)
OBJS += $(ASRCS:%.S=%.o)
DEPS := $(OBJS:.o=.d)

INC_FLAGS := -Iinclude -Iarch/$(ARCH)/include
ISO_DIR := iso

CFLAGS := -ffreestanding -mcmodel=large -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -g -Wall -MMD -lgcc $(INC_FLAGS) $(CFG)
LDFLAGS := -nostdlib -z max-page-size=4096

.PHONY: all iso clean

all: $(PROG) iso

$(PROG): $(OBJS)
	$(LD) -T arch/$(ARCH)/link.ld -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJS): config.mk

config.mk:
	cp -f default_config.mk config.mk

iso: 
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(PROG) $(ISO_DIR)/boot/
	cp grub.cfg $(ISO_DIR)/boot/grub/
	grub-mkrescue -o $(PROG).iso $(ISO_DIR)

run:
	qemu-system-x86_64 -cdrom $(PROG).iso -m 4G -smp 2 &

debug:
	qemu-system-x86_64 -s -S -d int,guest_errors,mmu -D qemu.log -cdrom $(PROG).iso -m 4G -smp 2 &

clean:
	rm -rf $(OBJS) $(PROG) $(DEPS) $(PROG).iso $(ISO_DIR)

-include $(DEPS)
