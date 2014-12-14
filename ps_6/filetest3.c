/* filetest1.c
 * ls on root directory to make sure it is empty
 * make max_num directories
 * ls to make sure new dirs were added
 * make one more directory
 * make sure that operation fails
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
int max;

int test(int* arg) {
  char** file_list;
  int i;
  char name[257]; 

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

  for (i = 0; i < (max - 2); i++) {
    sprintf(name, "%d", i);
    if (minifile_mkdir(name) == -1) {
      printf("mkdir failed on %dth entry. abort!\n", i+1);
      return -1;
    }
  }

  file_list = minifile_ls("/");
  if (!file_list) {
    printf("ls failed. abort!\n");
    return -1;
  }
  
  assert(list_count(file_list) == max);
  assert(!strcmp(file_list[0], "."));
  free(file_list[0]);
  assert(!strcmp(file_list[1], ".."));
  free(file_list[1]);

  for (i = 2; i < max; i++) {
    sprintf(name, "%d", i-2);
    assert(!strcmp(file_list[i], name));
    free(file_list[i]);
  }
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
  disk_size = 15000;
  
  system("rm MINIFILESYSTEM");

  max = 1000;

  minithread_system_initialize(init, NULL);
  return 0; 
}
