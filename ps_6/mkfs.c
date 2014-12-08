#include "minithread.h"
#include "synch.h"
#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int use_existing_disk;
const char* disk_name;
int disk_flags;
int disk_size;

int main(int argc, char** argv) {
  disk_t* disk;

  if (argc == 0) {
    printf("Provide disk size in blocks\n");
    return -1;
  }

  use_existing_disk = 0;
  disk_name = "minidisk";
  disk_flags = DISK_READWRITE;
  disk_size = atoi(argv[1]);

  disk = (disk_t*)calloc(1, sizeof(disk_t));
  if (disk_initialize(disk)) {
    free(disk);
    return -1;
  } 
  
   
  // write superblock
  // def INODE_START, DATA_START
  // NULLIFY all blocks, and change the next ptr
  // 
  // 
  return 0; 
}
