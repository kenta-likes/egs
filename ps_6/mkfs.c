#include "minithread.h"
#include "synch.h"
#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int use_existing_disk;
int disk_flags;
int disk_size;
disk_t* disk;

int init(int* arg) {
  /*super_block super;
  char* out;

  out = (char*)calloc(DISK_BLOCK_SIZE, sizeof(char));
  printf("disk initialized\n");  
  printf("write status: %d\n", disk_write_block(disk, 1, buff));    
  minithread_sleep_with_timeout(1000);
  printf("read status: %d\n", disk_read_block(disk, 1, out));
  minithread_sleep_with_timeout(1000);
  printf("%s", out); */
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
  
  minithread_system_initialize(init, NULL);
  // write superblock
  // def INODE_START, DATA_START
  // NULLIFY all blocks, and change the next ptr
  // 
  // 
  return 0; 
}
