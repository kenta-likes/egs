/* filetest3.c
 * make and remove 4800 directories.
 * make sure blocks don't get lost.
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
  int i,j;
  char name[257]; 

  for (i = 0; i < 100; i++) { 
    printf("iteration: %d\n", i);

    for (j = 0; j < 48; j++) {
      sprintf(name, "%d", j);
      if (minifile_mkdir(name) == -1) {
        printf("mkdir failed on %dth entry. abort!\n", j+1);
        return -1;
      }
    }

    for (j = 0; j < 48; j++) {
      sprintf(name, "%d", j);
      if (minifile_rmdir(name) == -1) {
        printf("rmdir failed on %dth entry. abort!\n", j+1);
        return -1;
      }
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
  disk_size = 750;
  
  system("rm MINIFILESYSTEM");

  minithread_system_initialize(init, NULL);
  return 0; 
}
