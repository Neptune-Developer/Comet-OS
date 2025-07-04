ARCH = aarch64
CROSS_COMPILE = $(ARCH)-linux-gnu-

AS = $(CROSS_COMPILE)as
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

BUILD_DIR = build
SRC_DIR = src

C_SRCS = $(wildcard $(SRC_DIR)/*.c)
S_SRCS = $(wildcard $(SRC_DIR)/*.s)

C_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SRCS))
S_OBJS = $(patsubst $(SRC_DIR)/%.s, $(BUILD_DIR)/%.o, $(S_SRCS))

OBJS = $(C_OBJS) $(S_OBJS)

TARGET = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
BOOTLOADER_BIN = $(BUILD_DIR)/bootloader.bin

.PHONY: all clean run

all: $(TARGET) $(KERNEL_BIN) $(BOOTLOADER_BIN)

$(BUILD_DIR): 
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) -Wall -Wextra -nostdlib -nostdinc -ffreestanding -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	$(AS) $< -o $@

$(TARGET): $(OBJS)
	$(LD) -nostdlib -T linker.ld $(OBJS) -o $@

$(KERNEL_BIN): $(TARGET)
	$(OBJCOPY) -O binary $(TARGET) $@

$(BOOTLOADER_BIN): $(BUILD_DIR)/bootloader.o
	$(LD) -nostdlib -Ttext=0x80000 $(BUILD_DIR)/bootloader.o -o $(BUILD_DIR)/bootloader.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/bootloader.elf $@

clean:
	rm -rf $(BUILD_DIR)

run:
	qemu-system-aarch64 -M virt -cpu cortex-a53 -kernel $(BOOTLOADER_BIN)

