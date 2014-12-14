/* filetest3.c
 * nested mkdir.
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
  int i;
  char name[257]; 

  for (i = 0; i < 50; i++) {
    sprintf(name, "%d", i);
    if (minifile_mkdir(name) == -1) {
      printf("mkdir failed on %dth entry. abort!\n", i);
      return -1;
    }
    if (minifile_cd(name) == -1) {
      printf("cd failed on %dth entry. abort!\n", i);
      return -1;
    }
  }

  for (i = 49; i >= 0; i--) {
    sprintf(name, "%d", i);
    if (minifile_cd("..") == -1) {
      printf("cd failed on %dth entry. abort!\n", i);
      return -1;
    }
    printf("curr dir: %s\n", minifile_pwd());
    if (minifile_rmdir(name) == -1) {
      printf("rmdir failed on %dth entry. abort!\n", i);
      return -1;
    }
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
  disk_size = 1000;
  
  system("rm MINIFILESYSTEM");

  minithread_system_initialize(init, NULL);
  return 0; 
}
