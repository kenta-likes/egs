/* filetest4.c
 * create, write, read and rm a file
 */


#include "minithread.h"
#include "synch.h"
#include "minifile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int use_existing_disk;
int disk_flags;
int disk_size;

int test(int* arg) {
  minifile_t file;
  char** file_list;
  char* data;
  char buff[10];

  data = "hello"; 

  file = minifile_creat("foo.txt");
  if (!file) {
    printf("minifile_creat failed\n");
    return -1;
  }

  file_list = minifile_ls("/foo.txt");
  if (!file_list) {
    printf("ls failed\n");
    return -1;
  }

  if (!strcmp(file_list[0], "foo.txt")) {
    printf("ls failed\n");
    return -1;
  }

  if (minifile_write(file, data, 6) == -1) {
    printf("write failed\n");
    return -1;
  }
 
  if (minifile_close(file) == -1) {
    printf("minifile_close failed\n");
    return -1;
  }

  file = minifile_open("foo.txt", "w+");
  if (!file) {
    printf("minifile_open failed\n");
    return -1;
  }
    
  if (minifile_read(file, buff, 6) == -1) {
    printf("minifile_read failed\n");
    return -1;
  }

  if (strcmp(buff, data)) {
    printf("minifile_read failed\n");
    return -1;
  }

  if (minifile_close(file) == -1) {
    printf("minifile_close failed\n");
    return -1;
  }

  if (minifile_unlink("foo.txt") == -1) {
    printf("minifile_unlink failed\n");
    return -1;
  }

  printf("all tests pass\n");
  return 0;
}

int init(int* arg) {
  minifile_make_fs();
  minithread_fork(test, NULL); 
  return 0;
}

int main(int argc, char** argv) {
  use_existing_disk = 0;
  disk_name = "MINIFILESYSTEM";
  disk_flags = DISK_READWRITE;
  disk_size = 100;
  
  system("rm MINIFILESYSTEM");

  minithread_system_initialize(init, NULL);
  return 0; 
}
