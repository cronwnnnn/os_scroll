# ================= 1. 变量定义 =================
CC = gcc
ASM = nasm
LD = ld

# 目录定义
SRC_DIR = src
BUILD_DIR = build

DEP_FLAGS = -MMD -MP

# 编译选项
CFLAGS = -m32 -Isrc/include -fno-builtin -fno-stack-protector -nostdlib -fno-pic -no-pie -Wall -Werror -g -c $(DEP_FLAGS)
ASMFLAGS = -f elf32 -I src/include
LDFLAGS = -m elf_i386 -T link.ld

KERNEL = $(BUILD_DIR)/kernel.bin
MBR = $(BUILD_DIR)/mbr.bin
LOADER = $(BUILD_DIR)/loader.bin

# ================= 2. 寻找源文件与计算目标文件 =================

# 使用 shell find 命令递归查找所有 .c 和 .asm 文件
# 结果类似: src/kernel/main.c src/drivers/vga.c
C_SOURCES = $(shell find $(SRC_DIR) -name '*.c')

# 1. 先找到所有的 .asm 文件
ALL_ASM_SOURCES = $(shell find $(SRC_DIR) -name '*.asm')

# 2. 定义不需要链接进内核的引导程序汇编文件
# 注意：路径必须和 find 查出来的路径完全一致
BOOT_ASM_SOURCES = src/boot/mbr.asm src/boot/loader.asm

# 3. 使用 filter-out 从列表中剔除引导程序文件
# 现在的 ASM_SOURCES 只包含内核需要的汇编文件，不再包含 mbr 和 loader
ASM_SOURCES = $(filter-out $(BOOT_ASM_SOURCES), $(ALL_ASM_SOURCES))

# 计算对应的 .o 文件路径
# 逻辑：将 src/xxx.c 替换为 build/src/xxx.o
C_OBJECTS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst %.asm, $(BUILD_DIR)/%.o, $(ASM_SOURCES))

# 所有需要链接的 .o 文件
OBJ = $(C_OBJECTS) $(ASM_OBJECTS)

DEPS = $(OBJ:.o=.d)

# ================= 3. 编译规则 =================

all: image

image:$(KERNEL) $(MBR) $(LOADER)
	dd if=$(BUILD_DIR)/mbr.bin of=hardware/scroll.img bs=512 count=1 seek=0 conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=hardware/scroll.img bs=512 count=8 seek=1 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=hardware/scroll.img bs=512 count=2048 seek=9 conv=notrunc

$(MBR): $(SRC_DIR)/boot/mbr.asm
	@echo "Assembling MBR..."
	@mkdir -p $(dir $@)
	$(ASM) -I src/include -f bin $< -o $@

$(LOADER): $(SRC_DIR)/boot/loader.asm
	@echo "Assembling loader..."
	@mkdir -p $(dir $@)
	$(ASM) -I src/include -f bin $< -o $@

# 链接规则
$(KERNEL): $(OBJ)
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) -o $@ $^

# C 文件编译规则
# 意思是：当需要 build/xxx.o 时，去依赖 xxx.c
$(BUILD_DIR)/%.o: %.c
	@# 关键一步：创建目标文件所在的目录（如果不存在的话）
	@mkdir -p $(dir $@)
	@echo "Compiling C: $< -> $@"
	$(CC) $(CFLAGS) $< -o $@

# 汇编文件编译规则
$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "Assembling: $< -> $@"
	$(ASM) $(ASMFLAGS) $< -o $@

# ================= 4. 辅助功能 =================

-include $(DEPS)

clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

# 运行 (注意这里指向 build 目录下的 kernel)

.PHONY: all clean