/* filetest1.c
 * ls on root directory to make sure it is empty
 * make a directory
 * ls to make sure new dir was added
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
  char** file_list;

  printf("testing ls on root\n");   
  file_list = minifile_ls("/");
  if (!file_list) {
    printf("ls failed. abort!\n");
    return -1;
  }
  

  assert(list_count(file_list) == 2);
  assert(!strcmp(file_list[0], "."));
  free(file_list[0]);
  assert(!strcmp(file_list[1], ".."));
  free(file_list[1]);
  free(file_list);

  printf("root test passed\n");

  if (minifile_mkdir("foo") == -1) {
    printf("mkdir failed. abort!\n");
    return -1;
  }

  file_list = minifile_ls("/");
  if (!file_list) {
    printf("ls failed. abort!\n");
    return -1;
  }
  
  assert(list_count(file_list) == 3);
  assert(!strcmp(file_list[0], "."));
  free(file_list[0]);
  assert(!strcmp(file_list[1], ".."));
  free(file_list[1]);
  assert(!strcmp(file_list[2], "foo"));
  free(file_list[2]);
  free(file_list);
  
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
  disk_size = 1000;
  
  system("rm MINIFILESYSTEM");

  minithread_system_initialize(init, NULL);
  return 0; 
}
