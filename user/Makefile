SRC_DIR=./src
LIB_DIR=./lib
BIN_DIR=./progs

SYS_INC_DIR=../src/include
USER_INC_DIR=$(SRC_DIR)/include

DEP_FLAGS = -MMD -MP

CC=gcc
AS=nasm
CFLAGS=-m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-pic -no-pie -Wall -Werror -g $(DEP_FLAGS)
IFLAGS=-I${USER_INC_DIR} -I${SYS_INC_DIR}
LDFLAGS=-m elf_i386 -T link.ld

# 在这里列出确实存在的系统库文件
SYS_OBJS = \
	$(LIB_DIR)/user_sys/syscall.o \
	$(LIB_DIR)/user_sys/syscall_trigger.o \
	$(LIB_DIR)/user_sys/common.o \
	$(LIB_DIR)/user_sys/stdlib.o \
	$(LIB_DIR)/user_sys/user_io.o

# 我们暂时只测试存在的程序
PROGS = \
  ${BIN_DIR}/hello \
  ${BIN_DIR}/shell \
  ${BIN_DIR}/ls \
  ${BIN_DIR}/os_test \
  ${BIN_DIR}/video_test \
  ${BIN_DIR}/init 

all: prepare image

prepare:
	mkdir -p $(BIN_DIR)
	mkdir -p $(LIB_DIR)/user_sys
	mkdir -p $(LIB_DIR)/user_file

image: disk_image_writer progs greeting file_system_img
	./fs/disk_image_writer ./progs

disk_image_writer: ./fs/write_disk_image.c ./fs/fs_inject.c
	gcc -I../src/include -o ./fs/disk_image_writer ./fs/write_disk_image.c ./fs/fs_inject.c

file_system_img:
	rm -f file_system.img
	dd if=/dev/zero of=file_system.img bs=1M count=100
	gcc -I../src/include -o ./fs/mkfs ./fs/mkfs.c
	./fs/mkfs

progs: ${PROGS}

greeting:
	cp greeting.txt ./progs/

$(BIN_DIR)/%: $(LIB_DIR)/user_file/%.o ${SYS_OBJS}
	ld $(LDFLAGS) $< ${SYS_OBJS} -o $@

$(LIB_DIR)/user_file/%.o: $(SRC_DIR)/user_file/%.c
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

$(LIB_DIR)/user_sys/%.o: $(SRC_DIR)/user_sys/%.c
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

$(LIB_DIR)/user_sys/%.o: $(SRC_DIR)/user_sys/%.asm
	$(AS) -f elf32 $< -o $@

clean:
	rm -rf ${BIN_DIR} ${LIB_DIR} user_disk_image disk.img ./fs/disk_image_writer ./fs/mkfs
