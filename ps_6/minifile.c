#include "minifile.h"
#include "disk.h"
#include "synch.h"

/*
 * struct minifile:
 *     This is the structure that keeps the information about 
 *     the opened file like the position of the cursor, etc.
 */

/* TYPE DEFS */
struct minifile {
  /* add members here */
  int dummy;
};

typedef block_ctrl* block_ctrl_t;

/* GLOBAL VARS */
int disk_size;
int INODE_START;
int DATA_START;

/* FUNC DEFS */

void minifile_ensure_exist_at(int idx) {
  if (!block_array[idx]) {
    block_array[idx] = block_ctrl_create();
  }
}

block_ctrl_t minifile_block_ctrl_create() {
  block_ctrl_t newb;
  
  newb = (block_ctrl_t)calloc(1, sizeof(block_ctrl));
  newb->block_sem = semaphore_create();
  
  if (!newb->block_sem) {
    free(newb);
  }
  
  sempahore_initialize(newb->block_sem, 0);
  return newb; 
}

int minifile_block_ctrl_destroy(block_ctrl_t b) {
  if (!b) {
    return -1;
  }
  if (semaphore_destroy(b->block_sem)) {
    return -1;
  }
  free(b);
  return 0;
}

minifile_t minifile_creat(char *filename){
  return NULL;
}

minifile_t minifile_open(char *filename, char *mode){
  return NULL;
}

int minifile_read(minifile_t file, char *data, int maxlen){
  return -1;
}

int minifile_write(minifile_t file, char *data, int len){
  return -1;
}

int minifile_close(minifile_t file){
  return -1;
}

int minifile_unlink(char *filename){
  return -1;
}

int minifile_mkdir(char *dirname){
  return -1;
}

int minifile_rmdir(char *dirname){
  return -1;
}

int minifile_stat(char *path){
  return -1;
} 

int minifile_cd(char *path){
  return -1;
}

char **minifile_ls(char *path){
  return NULL;
}

char* minifile_pwd(void){
  return NULL;
}

void minifile_make_fs(void) {
  super_block* super;
  inode_block* inode;
  data_block* data;
  char* magic = "4411";

  super = (super_block*)calloc(1, sizeof(super_block));
  inode = (inode_block*)calloc(1, sizeof(inode_block));
  data = (data_block*)calloc(1, sizeof(inode_block));
 
  semaphore_P(disk_op_lock); 
  INODE_START = 3;
  DATA_START = disk_size / 10 + 1;
  memcpy(super->u.hdr.magic_num, magic, 4);
  super->u.hdr.block_count = disk_size;
  super->u.hdr.fib = INODE_START;
  super->u.hdr.fdb = DATA_START; 
  super->u.hdr.root = 2;
  
  minifile_ensure_exist_at(0);
  disk_write_block 
  semaphore_P(block_array[0]->block_sem);
  inode->u.hdr.status = FREE;
  data->u.hdr.status = FREE; 
}

