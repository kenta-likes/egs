#include "minithread.h"
#include "synch.h"
#include "disk.h"
#include "minifile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int use_existing_disk;
int disk_flags;
int disk_size;

int init(int* arg) {
  minifile_make_fs();
  return 0;
}

int main(int argc, char** argv) {

  if (argc == 1) {
    printf("Provide disk size in blocks\n");
    return -1;
  }

  use_existing_disk = 0;
  disk_flags = DISK_READWRITE;
  disk_size = atoi(argv[1]);
  disk_name = "MINIFILESYSTEM";
  
  minithread_system_initialize(init, NULL);
  // write superblock
  // def INODE_START, DATA_START
  // NULLIFY all blocks, and change the next ptr
  // 
  // 
  return 0; 
}
