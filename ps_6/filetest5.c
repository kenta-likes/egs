/* filetest4.c
 * create and rm an empty file
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

  file = minifile_creat("foo.txt");
  if (!file) {
    printf("minifile_creat failed\n");
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
