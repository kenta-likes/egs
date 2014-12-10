#include "minifile.h"
#include "synch.h"
#include "disk.h"
#include "minithread.h"
#include "interrupts.h"

#define DATA_BLOCK_SIZE (DISK_BLOCK_SIZE-sizeof(int)-1)
#define BLOCK_COUNT disk_size
#define MAX_PATH_SIZE 256 //account for null character at end
#define DIR_MAX_ENTRIES_PER_BLOCK (DATA_BLOCK_SIZE/sizeof(dir_entry)) 
#define INODE_START 2
#define DATA_START (BLOCK_COUNT/10)


/* TYPE DEFS */

/*
 * All the structs for our block types
 * */
typedef struct {
  char name[MAX_PATH_SIZE + 1];
  int block_num;
  char type;
} dir_entry;

typedef struct {
  union super_union {
    struct super_hdr {
      char magic_num[4];
      int block_count;
      int free_iblock_hd;
      int free_iblock_tl;
      int free_dblock_hd;
      int free_dblock_tl;
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
      int count;
      int d_ptrs[11];
      int i_ptr;
    } hdr;
  
    char padding[DISK_BLOCK_SIZE];
  } u;
} inode_block;

typedef struct {
  union data_union {
    struct file_hdr {
      char status;
      int next;
      char data[DATA_BLOCK_SIZE];
    } file_hdr;

    struct dir_hdr {
      char status;
      int next;
      dir_entry data[DIR_MAX_ENTRIES_PER_BLOCK];
    } dir_hdr;
  
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
enum { DIR_t = 1, FILE_t };

/* GLOBAL VARS */
int disk_size;
const char* disk_name;
block_ctrl_t* block_array = NULL;
semaphore_t disk_op_lock = NULL;
disk_t* my_disk = NULL;
semaphore_t* inode_table;

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
  if (block_num > disk_size || block_num < 0){
    set_interrupt_level(l);
    printf("error: disk response with invalid parameters\n");
    return;
  }
  block_array[block_num]->block_arg = block_arg;
  semaphore_V(block_array[block_num]->block_sem);
  set_interrupt_level(l);
  return;
}

/*
 * Helper function to get block number from
 * a directory/file path
 * Returns: block number, -1 if path DNE
 * */
int minifile_get_block_from_path(char* dir_path){
  super_block* s_block;
  inode_block* i_block;
  data_block* d_block;
  char* abs_dir; //store absolute path here
  char* curr_dir_name; //use for holding current directory name
  char* curr_block;
  int name_len;
  int i;
  int j;
  int curr_block_num;

  if (dir_path[0] == '\0'){
    printf("ERROR: looking up empty string");
    return -1;
  }

  semaphore_P(disk_op_lock);
  
  //this is a relative path, so construct absolute path
  if (dir_path[0] != '/'){
    //add two to buffer size for extra '/' character and '\0' character at the end
    abs_dir = (char*)calloc(strlen(minithread_get_curr_dir()) + strlen(dir_path) + 2, sizeof(char));
    strcpy(abs_dir, minithread_get_curr_dir());
    abs_dir[strlen(minithread_get_curr_dir())] = '/';
    strcpy(abs_dir + strlen(minithread_get_curr_dir()) + 1, dir_path);
  }
  else { //otherwise it's an absolute path
    abs_dir = (char*)calloc(strlen(dir_path) + 1, sizeof(char));
    strcpy(abs_dir, dir_path);
  }
  curr_dir_name = abs_dir; //point to beginning
  printf("Absolute path is %s\n", abs_dir); //TODO: Check output

  curr_block = (char*)calloc(1, sizeof(super_block));
 
  //read the super block
  disk_read_block(my_disk, 0, (char*)curr_block);
  semaphore_P(block_array[0]->block_sem);
  s_block = (super_block*)curr_block;
  
  curr_block_num = s_block->u.hdr.root; //grab the root block number
  curr_dir_name += 1; //points to the name after the '/' sign
  while (curr_dir_name[0] != '\0'){
    if (strchr(curr_dir_name, '/') == NULL){
      //this is the last one on the plate, can be a file or a directory
    }
    else {
      name_len = (int)(strchr(curr_dir_name, '/') - curr_dir_name);
      disk_read_block(my_disk, curr_block_num, curr_block); //read in the block from the current block number
      semaphore_P(block_array[curr_block_num]->block_sem);
      i_block = (inode_block*)curr_block; //next directory to explore
      curr_block_num = -1; //set to -1 to check at end of loop
      for (i = 0; i < 11; i++){ //iterate over direct entries...
        for (j = 0; j < DIR_MAX_ENTRIES_PER_BLOCK; j++){
          disk_read_block(my_disk, i_block->u.hdr.d_ptrs[i], curr_block);
          semaphore_P(block_array[i_block->u.hdr.d_ptrs[i]]->block_sem);
          d_block = (data_block*)curr_block; //data block to check
          if (memcmp(curr_dir_name, d_block->u.dir_hdr.data[j].name, name_len) == 0){
            // if directory
              // read in inode, break loop
                //semaphore_P(disk_op_lock);
            // if not directory, return error: file exists
          }
          // else continue with for loop
        }
      }
      // if i >= total number of entries
           //semaphore_P(disk_op_lock);
        // could not find; return error
      // else continue 
    }
  }
  
  semaphore_V(disk_op_lock);
  return -1;

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

/*
 * returns the current directory by strcpy-ing the curr_dir
 * */
char* minifile_pwd(void){
  char* user_curr_dir;

  user_curr_dir = (char*)calloc(strlen(minithread_get_curr_dir()) + 1, sizeof(char));
  strcpy(user_curr_dir, minithread_get_curr_dir());
  return user_curr_dir;
}

void minifile_test_make_fs() {
  super_block* super;
  inode_block* inode;
  data_block* data;
  int block_num;
  int free_iblock;
  int free_dblock;
  char* out;
  
  out = calloc(DISK_BLOCK_SIZE, sizeof(char));
 
  semaphore_P(disk_op_lock); 
  printf("enter minifile_test_make_fs\n");
  
  disk_read_block(my_disk, 0, out);
  semaphore_P(block_array[0]->block_sem);
  super = (super_block*)out;

  assert(super->u.hdr.block_count == BLOCK_COUNT);
  assert(super->u.hdr.root == 1);
  free_iblock = super->u.hdr.free_iblock_hd;
  free_dblock = super->u.hdr.free_dblock_hd;

  block_num = super->u.hdr.root; 
  disk_read_block(my_disk, block_num, out);
  semaphore_P(block_array[block_num]->block_sem);

  inode = (inode_block*)out;
  assert(inode->u.hdr.status == IN_USE);
  assert(inode->u.hdr.count == 0);
  
  block_num = free_iblock;
  while (block_num != 0) {
    printf("free inode at %d\n", block_num);
    disk_read_block(my_disk, block_num, out);
    semaphore_P(block_array[block_num]->block_sem);
    assert(inode->u.hdr.status == FREE);
    block_num = inode->u.hdr.next; 
  }

  data = (data_block*)out;
  block_num = free_dblock;
  while (block_num != 0) {
    printf("free data block at %d\n", block_num);
    disk_read_block(my_disk, block_num, out);
    semaphore_P(block_array[block_num]->block_sem);
    assert(data->u.file_hdr.status == FREE);
    block_num = data->u.file_hdr.next; 
  }
  
  free(out);
  printf("File System creation tested\n");
}

void minifile_make_fs(void) {
  super_block* super;
  inode_block* inode;
  data_block* data;
  int i;
  char* magic = "4411";
  
  super = (super_block*)calloc(1, sizeof(super_block));
  inode = (inode_block*)calloc(1, sizeof(inode_block));
  data = (data_block*)calloc(1, sizeof(inode_block));
 
  semaphore_P(disk_op_lock); 
  printf("enter minifile_make_fs\n");
  memcpy(super->u.hdr.magic_num, magic, 4);
  super->u.hdr.block_count = BLOCK_COUNT;
  super->u.hdr.free_iblock_hd = INODE_START;
  super->u.hdr.free_iblock_tl = DATA_START - 1;
  super->u.hdr.free_dblock_hd = DATA_START; 
  super->u.hdr.free_dblock_tl = BLOCK_COUNT - 1; 
  super->u.hdr.root = 1;
  
  disk_write_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);
 
  // root 
  inode->u.hdr.status = IN_USE;
  inode->u.hdr.count = 0; 
  disk_write_block(my_disk, 1, (char*)inode);
  semaphore_P(block_array[1]->block_sem);

  inode->u.hdr.status = FREE;
  inode->u.hdr.count = 0; 

  // make linked list of free inodes
  for (i = INODE_START; i < DATA_START - 1; i++) {
    inode->u.hdr.next = i+1;
    disk_write_block(my_disk, i, (char*)inode);
    semaphore_P(block_array[i]->block_sem);
  }

  // the last one is null terminated
  inode->u.hdr.next = 0; 
  disk_write_block(my_disk, DATA_START - 1, (char*)inode);
  semaphore_P(block_array[DATA_START - 1]->block_sem);

  data->u.file_hdr.status = FREE;

  // make linked list of free data blocks
  for (i = DATA_START; i < BLOCK_COUNT - 1; i++) {
    data->u.file_hdr.next = i+1;
    disk_write_block(my_disk, i, (char*)data);
    semaphore_P(block_array[i]->block_sem);
  }
 
  // the last one is null terminated
  data->u.file_hdr.next = 0; 
  disk_write_block(my_disk, BLOCK_COUNT - 1, (char*)data);
  semaphore_P(block_array[BLOCK_COUNT - 1]->block_sem);

  free(super);
  free(inode);
  free(data);
  printf("File System created.\n");
  semaphore_V(disk_op_lock); 
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
  int i;

  my_disk = (disk_t*)calloc(1, sizeof(disk_t));
  disk_name = "MINIFILESYSTEM";
  disk_initialize(my_disk);
  //call mkfs functions to creat the file system
 
 
  //initialize the array
  block_array = (block_ctrl_t*)calloc(disk_size, sizeof(block_ctrl_t));
  for (i = 0; i < disk_size; i++) {
    block_array[i] = minifile_block_ctrl_create();
  }

  inode_table = (semaphore_t*)calloc(DATA_START, sizeof(semaphore_t));
  for (i = 0; i < DATA_START; i++) {
    inode_table[i] = semaphore_create();
    semaphore_initialize(inode_table[i], 1);
  }

  //install a handler
  install_disk_handler(minifile_disk_handler);

  disk_op_lock = semaphore_create();
  semaphore_initialize(disk_op_lock, 1);
  return 0;
}

