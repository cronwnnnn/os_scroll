SRC_DIR=./src
LIB_DIR=./lib
BIN_DIR=./progs

SYS_INC_DIR=../src/include
SYS_LIB_DIR=../build/src

DEP_FLAGS = -MMD -MP

CC=gcc
CFLAGS=-m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-pic -no-pie -Wall -Werror -g $(DEP_FLAGS)
IFLAGS=-I${SRC_DIR} -I${SYS_INC_DIR}
LDFLAGS=-m elf_i386 -T link.ld

# 在这里列出确实存在的系统库文件（复用内核编译出来的 .o）
SYS_OBJS = \
	$(SYS_LIB_DIR)/syscall/syscall.o \
	$(SYS_LIB_DIR)/syscall/syscall_trigger.o \
	$(LIB_DIR)/user_sys/common.o

# 我们暂时只测试存在的程序
PROGS = \
  ${BIN_DIR}/hello

all: prepare image

prepare:
	mkdir -p $(BIN_DIR)
	mkdir -p $(LIB_DIR)/user_sys

image: disk_image_writer progs greeting
	./disk_image_writer ./progs

disk_image_writer: write_disk_image.c
	gcc -o disk_image_writer write_disk_image.c

progs: ${PROGS}

greeting:
	cp greeting.txt ./progs/

$(BIN_DIR)/%: $(LIB_DIR)/%.o ${SYS_OBJS}
	ld $(LDFLAGS) $< ${SYS_OBJS} -o $@

$(LIB_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

$(LIB_DIR)/user_sys/%.o: $(SRC_DIR)/user_sys/%.c $(SRC_DIR)/user_sys/%.h
	@mkdir -p $(LIB_DIR)/user_sys
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

clean:
	rm -rf ${BIN_DIR} ${LIB_DIR} user_disk_image disk_image_writer greeting.txt
