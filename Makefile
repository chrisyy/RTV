
TARGET := x86_64-elf
ARCH := x86_64
CC := $(TARGET)-gcc
LD := $(TARGET)-ld

PROG := rtv

CSRCS := $(shell find . -name *.c)
ASRCS := $(shell find . -name *.s)
OBJS := $(CSRCS:%.c=%.o)
OBJS += $(ASRCS:%.s=%.o)
DEPS := $(OBJS:.o=.d)

INC_FLAGS := -Iinclude -Iarch/$(ARCH)/include
ISO_DIR := iso

#-z max-page-size may not be needed
CFLAGS := -ffreestanding -mcmodel=large -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -g -Wall -MMD -lgcc $(INC_FLAGS)
LDFLAGS := -nostdlib -z max-page-size=4096

.PHONY: all iso clean

all: $(PROG) iso

$(PROG): $(OBJS)
	$(LD) -T arch/$(ARCH)/link.ld -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

iso: 
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(PROG) $(ISO_DIR)/boot/
	cp grub.cfg $(ISO_DIR)/boot/grub/
	grub-mkrescue -o $(PROG).iso $(ISO_DIR)

run:
	qemu-system-x86_64 -cdrom $(PROG).iso -m 4G &

debug:
	qemu-system-x86_64 -s -S -cdrom $(PROG).iso -m 4G &

clean:
	rm -rf $(OBJS) $(PROG) $(DEPS) $(PROG).iso $(ISO_DIR)


-include $(DEPS)

