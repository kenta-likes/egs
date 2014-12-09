#include "minifile.h"
#include "synch.h"
#include "disk.h"
#include "minithread.h"
#include "interrupts.h"

#define DATA_BLOCK_SIZE (DISK_BLOCK_SIZE-sizeof(int)-1)
/*
 * struct minifile:
 *     This is the structure that keeps the information about 
 *     the opened file like the position of the cursor, etc.
 */

/* TYPE DEFS */

/*
 * All the structs for our block types
 * */
typedef struct {
  union super_union {
    struct super_hdr {
      char magic_num[4];
      int block_count;
      int fib;
      int fdb;
      int root;
    } hdr;

    char padding[DISK_BLOCK_SIZE];
  } u;
} super_block;

typedef struct {
  union inode_union {
    struct inode_hdr {
      char status;
      int next;
      int byte_count;
      int d_ptrs[11];
      int i_ptr;
    } hdr;
  
    char padding[DISK_BLOCK_SIZE];
  } u;
} inode_block;

typedef struct {
  union data_union {
    struct data_hdr {
      char status;
      int next;
      char data[DATA_BLOCK_SIZE];
    } hdr;
  
    char padding[DISK_BLOCK_SIZE];
  } u;
} data_block;


struct minifile {
  /* add members here */
  int dummy;
};

typedef struct block_ctrl{
  semaphore_t block_sem;
  disk_interrupt_arg_t* block_arg;
} block_ctrl;

typedef block_ctrl* block_ctrl_t;

enum { FREE = 1, IN_USE };

/* GLOBAL VARS */
int disk_size;
const char* disk_name;
int INODE_START;
int DATA_START;
block_ctrl_t* block_array = NULL;
semaphore_t disk_op_lock;
disk_t* my_disk = NULL;

/* FUNC DEFS */

block_ctrl_t minifile_block_ctrl_create(void) {
  block_ctrl_t newb;
  
  newb = (block_ctrl_t)calloc(1, sizeof(block_ctrl));
  newb->block_sem = semaphore_create();
  
  if (!newb->block_sem) {
    free(newb);
  }
  
  semaphore_initialize(newb->block_sem, 0);
  return newb; 
}

int minifile_block_ctrl_destroy(block_ctrl_t b) {
  if (!b) {
    return -1;
  }
  if (!b->block_sem) {
    free(b);
    return -1;
  }
  semaphore_destroy(b->block_sem);
  free(b);
  return 0;
}

void minifile_ensure_exist_at(int idx) {
  if (!block_array[idx]) {
    block_array[idx] = minifile_block_ctrl_create();
  }
}

/*
 * This is the disk handler. The disk handler will take care of
 * taking a response from a disk operation for a specific block,
 * placing the disk operation result into an array, and
 * acting on the appropriate semaphore to wake up a waiting thread.
 */
void 
minifile_disk_handler(void* arg) {
  disk_interrupt_arg_t* block_arg;
  int block_num;
  interrupt_level_t l;

  l = set_interrupt_level(DISABLED);
  block_arg = (disk_interrupt_arg_t*)arg;
  block_num = block_arg->request.blocknum;

  //check if the block number is within sensible bounds
  if (block_num > disk_size || block_num < 1 ){
    set_interrupt_level(l);
    printf("error: disk response with invalid parameters\n");
    return;
  }
  block_array[block_num]->block_arg = block_arg;
  semaphore_V(block_array[block_num]->block_sem);
  set_interrupt_level(l);
  return;
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
  //disk_write_block 
  semaphore_P(block_array[0]->block_sem);
  inode->u.hdr.status = FREE;
  data->u.hdr.status = FREE; 
}


/*
 * Minifile initialize function
 * Initializes the new disk (global vars are set by application)
 * Intitializes the block array for semaphores/interrupt args to
 * be stored.
 * Installs the interrupt handler function
 * Initializes the disk operation lock
 * */
int minifile_initialize(){
  my_disk = (disk_t*)calloc(1, sizeof(disk_t));
  disk_initialize(my_disk);
  //call mkfs functions to creat the file system
 
 
  //initialize the array
  block_array = (block_ctrl_t*)calloc(disk_size, sizeof(block_ctrl_t));

  //install a handler
  install_disk_handler(minifile_disk_handler);

  disk_op_lock = semaphore_create();
  semaphore_initialize(disk_op_lock, 1);
  return 0;
}

