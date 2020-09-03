SRCDIR := $(dir $(lastword $(MAKEFILE_LIST)))
VPATH := $(SRCDIR)

CROSS_COMPILE ?= riscv64-unknown-elf-
CFLAGS = -O2 -march=rv32ic -mabi=ilp32 -I $(SRCDIR)
LDFLAGS = $(CFLAGS) -Wl,-Bstatic,-T,$(SRCDIR)/animation.lds

OBJS  = entry.o
OBJS += $(patsubst %.c,%.o,$(wildcard *.c))
OBJS += $(patsubst %.s,%.o,$(wildcard *.s))
TARGET = $(shell basename $(shell pwd))

# Pattern Rules
%.o: %.s
	@echo "   Assembling [$<]"
	@$(CROSS_COMPILE)gcc $(CFLAGS) -c -o $@ $<

%.o: %.c
	@echo "   Compiling  [$<]"
	@$(CROSS_COMPILE)gcc $(CFLAGS) -c -o $@ $<

%.bin: %.elf
	@echo "   Generating [$@]"
	@$(CROSS_COMPILE)objcopy -O binary $^ $@

# Build the animation.
$(TARGET).elf: $(OBJS)
	@echo "   Linking    [$@]"
	@$(CROSS_COMPILE)gcc $(LDFLAGS) -o $@ $^

# Get the animation name from the directory name.
all: $(TARGET).elf $(TARGET).bin

# Cleanup after ourselves.
clean:
	rm -f *.o *.elf *.bin *.dfu

.DEFAULT_GOAL = all
.PHONY: all clean
