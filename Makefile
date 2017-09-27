
TARGET = x86_64-elf
CC = $(TARGET)-gcc
LD = $(TARGET)-ld
AR = $(TARGET)-ar
RANLIB = $(TARGET)-ranlib
STRIP = $(TARGET)-strip

PROG = rtv

BUILD_DIR = ./build
SRC_DIR = .

SRCS := $(shell find $(SRC_DIR) -name *.c -or -name *.s)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

CFLAGS = -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2
LDFLAGS = -nostdlib -lgcc

all: $(PROG)

$(PROG): 

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)
