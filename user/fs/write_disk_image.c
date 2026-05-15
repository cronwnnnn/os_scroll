#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// 在 C 语言中，可以通过声明外部函数来调用其他模块的实现，
// 这里不需要 include "sfs.h" 从而避免与 <dirent.h> 里的 struct dirent 冲突。
// 只要这个函数不需要在参数或返回类型里暴露你自定义的 struct dirent 即可：
void inject_file(FILE *disk_fp, const char *external_path, const char *internal_name);
void print_files_in_root(FILE* disk_img);

int main(int argc, char* argv[]) {
  char* dir_path = "./progs";
  if (argc > 1) {
    dir_path = argv[1];
  }

  // 存储所有文件路径和文件名
  char* file_names[256];
  char* file_paths[256];

  // List all programs.
  DIR *d = opendir(dir_path);
  struct dirent *dir;
  int num = 0;
  // 获取所有要写入的文件路径和文件名
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
        continue;
      }

      char* filename = (char*)malloc(strlen(dir->d_name) + 1);
      strcpy(filename, dir->d_name);
      file_names[num] = filename;

      char* filepath = (char*)malloc(strlen(dir->d_name) + strlen(dir_path) + 2);
      strcpy(filepath, dir_path);
      strcat(filepath, "/");
      strcat(filepath, dir->d_name);
      file_paths[num] = filepath;

      num++;
    }
    closedir(d);
  }

  // Create disk image file.
  FILE* image_file;
  if ((image_file = fopen("./file_system.img", "r+b")) == 0) {
    printf("open image file failed");
    exit(1);
  }
  
  int file_size[num];

  for(int i = 0; i < num; i++){
    char* file_name = file_names[i];
    char* file_path = file_paths[i];

    inject_file(image_file, file_path, file_name);
  }
  print_files_in_root(image_file);

}
