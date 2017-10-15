
TARGET := x86_64-elf
CC := $(TARGET)-gcc
LD := $(TARGET)-ld

PROG := rtv

CSRCS := $(shell find . -name *.c)
ASRCS := $(shell find . -name *.s)
OBJS := $(CSRCS:%.c=%.o)
OBJS += $(ASRCS:%.s=%.o)
DEPS := $(OBJS:.o=.d)

INC_DIR := ./include

#-z max-page-size may not be needed
CFLAGS := -ffreestanding -mcmodel=large -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -g -Wall -MMD -lgcc
LDFLAGS := -nostdlib -z max-page-size=4096

.PHONY: all clean

all: $(PROG)

$(PROG): $(OBJS)
	$(LD) -T link.ld -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG) $(DEPS)

-include $(DEPS)

