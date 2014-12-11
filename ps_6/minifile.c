#include "minifile.h"
#include "synch.h"
#include "disk.h"
#include "minithread.h"
#include "interrupts.h"

#define DATA_BLOCK_SIZE (DISK_BLOCK_SIZE-sizeof(int)-sizeof(char))
#define BLOCK_COUNT disk_size
#define NUM_DPTRS 11
#define MAX_PATH_SIZE 256 //account for null character at end
#define MAX_DIR_ENTRIES_PER_BLOCK (DATA_BLOCK_SIZE/sizeof(dir_entry)) 
#define INODE_START 2
#define DATA_START (BLOCK_COUNT/10+1)
#define MAX_INDIRECT_BLOCKNUM DATA_BLOCK_SIZE/sizeof(int)
#define NUM_PTRS NUM_DPTRS+MAX_INDIRECT_BLOCKNUM
#define MAX_DIR_ENTRIES MAX_DIR_ENTRIES_PER_BLOCK*NUM_PTRS
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
      char type;
      int count;
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
    } hdr;

    struct file_hdr {
      char status;
      int next;
      char data[DATA_BLOCK_SIZE];
    } file_hdr;

    struct dir_hdr {
      char status;
      int next;
      dir_entry data[MAX_DIR_ENTRIES_PER_BLOCK];
    } dir_hdr;

    struct indirect_hdr {
      char status;
      int next;
      int d_ptrs[MAX_INDIRECT_BLOCKNUM];
    } indirect_hdr;
  
    char padding[DISK_BLOCK_SIZE];
  } u;
} data_block;


/*TODO create create/destroy funcs for minifile and set block ptrs to NULL*/
typedef struct minifile {
  int inode_num;
  int byte_cursor;
  inode_block i_block;
  data_block indirect_block;
  data_block d_block;
  int block_cursor;
  char mode;
} minifile;

typedef struct block_ctrl{
  semaphore_t block_sem;
  disk_interrupt_arg_t* block_arg;
  
  char* buff;
} block_ctrl;

typedef block_ctrl* block_ctrl_t;

enum { FREE = 1, IN_USE };
enum { DIR_t = 1, FILE_t }; 
enum { READ = 0, WRITE, READ_WRITE, APPEND, READ_APPEND }; 

/* GLOBAL VARS */ 
int disk_size; 
const char* disk_name; 
block_ctrl_t* block_array = NULL; 
semaphore_t disk_op_lock = NULL; 
disk_t* my_disk = NULL; 
semaphore_t* inode_lock_table; 

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

void minifile_fix_fs(){
  // TODO
  return;

}

void minifile_disk_error_handler(disk_interrupt_arg_t* block_arg) {
  semaphore_P(disk_op_lock);

  switch (block_arg->reply) {
  case DISK_REPLY_FAILED:
    // gotta try again yo
    disk_send_request(block_arg->disk, block_arg->request.blocknum, 
        block_arg->request.buffer, block_arg->request.type);
    break;

  case DISK_REPLY_ERROR:
    printf("DISK_REPLY_ERROR wuttttt\n");
    break;

  case DISK_REPLY_CRASHED:
    minifile_fix_fs();
    break;
  
  default:
    break;
  }
  free(block_arg);
  semaphore_V(disk_op_lock);
}

/*
 * This is the disk handler. The disk handler will take care of
 * taking a response from a disk operation for a specific block,
 * placing the disk operation result into an array, and
 * acting on the appropriate semaphore to wake up a waiting thread.
 */
void minifile_disk_handler(void* arg) {
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
  if (block_arg->reply == DISK_REPLY_OK) {
    block_array[block_num]->block_arg = block_arg;
    semaphore_V(block_array[block_num]->block_sem);
    set_interrupt_level(l);
    return;
  }
  set_interrupt_level(l);
  minifile_disk_error_handler(block_arg);
}

/*
 * Reads the next block of data from the file
 * Returns 0 on success, -1 on failure
 * */
int minifile_get_next_block(minifile_t file_ptr){
  int block_total;
  int index;

  block_total = 0; //initialize to 0
  if (file_ptr->block_cursor == 0){ //if this is the first block read, read in the inode
    disk_read_block(my_disk, file_ptr->inode_num, (char*)(&file_ptr->i_block) );
    semaphore_P(block_array[file_ptr->inode_num]->block_sem);
  }
  if (file_ptr->i_block.u.hdr.type == FILE_t){
    block_total = file_ptr->i_block.u.hdr.count / DATA_BLOCK_SIZE;
    //if not divisible, add 1 to get ceiling
    if (file_ptr->i_block.u.hdr.count % DATA_BLOCK_SIZE != 0){
      block_total++;
    }
  }
  else {
    block_total = file_ptr->i_block.u.hdr.count / MAX_DIR_ENTRIES_PER_BLOCK;
    if (file_ptr->i_block.u.hdr.count % MAX_DIR_ENTRIES_PER_BLOCK != 0){
      block_total++;
    }
  }

  if (file_ptr->block_cursor >= block_total){
    return -1;
  }

  index = file_ptr->block_cursor;

  if (index < 11){
    //still direct block
    disk_read_block(my_disk, file_ptr->i_block.u.hdr.d_ptrs[index], (char*)(&file_ptr->d_block) );
    semaphore_P(block_array[file_ptr->i_block.u.hdr.d_ptrs[index]]->block_sem);
  }
  else {
    index -= 11; //get index in indirect block
    if (index == 0){
      //first block in indirect block, so read in the indirect block
      disk_read_block(my_disk, file_ptr->i_block.u.hdr.i_ptr, (char*)(&file_ptr->indirect_block) );
      semaphore_P(block_array[file_ptr->i_block.u.hdr.i_ptr]->block_sem);
    }
    disk_read_block(my_disk, file_ptr->indirect_block.u.indirect_hdr.d_ptrs[index], (char*)(&file_ptr->d_block) );
    semaphore_P(block_array[file_ptr->indirect_block.u.indirect_hdr.d_ptrs[index]]->block_sem);
  }
  file_ptr->block_cursor++;
  return 0;
}

/*
 * Helper function to get block number from a directory/file path
 * Returns: block number, -1 if path DNE
 *
 * Assumes: 
 * -nonempty path input
 * */
int minifile_get_block_from_path(char* path){
  minifile_t tmp_file;
  super_block* s_block;
  inode_block* i_block;
  char* abs_dir; //store absolute path here
  char* curr_dir_name; //use for holding current directory name
  char* curr_runner; //use for going through path
  int name_len;
  int i;
  int j;
  int curr_block_num;
  int entries_read;
  int entries_total;
  int is_dir;

  printf("Entering minifile_get_block_from_path()\n"); //TODO: Check output
  curr_dir_name = (char*)calloc(MAX_PATH_SIZE + 1, sizeof(char));
  abs_dir = (char*)calloc(MAX_PATH_SIZE, sizeof(char));
  tmp_file = (minifile_t)calloc(1, sizeof(minifile));
  //this is a relative path, so construct absolute path
  if (path[0] != '/'){
    i = strlen(minithread_get_curr_dir()); //use i as temp variable
    j = strlen(path);
    strcpy(abs_dir, minithread_get_curr_dir());
    if (abs_dir[i-1] == '/'){
      strcpy(abs_dir + i, path); //copy path passed in after '/'
    }
    else {
      abs_dir[i] = '/'; //replace null char with '/' to continue path
      strcpy(abs_dir + i + 1, path); //copy path passed in after '/'
    }
  }
  else { //otherwise it's an absolute path
    strcpy(abs_dir, path);
  }
  s_block = (super_block*)calloc(1, sizeof(super_block)); //make space for block container
  i_block = (inode_block*)calloc(1, sizeof(inode_block)); //make space for block container


  curr_runner = abs_dir; //point to beginning
  printf("Absolute path is %s\n", abs_dir);

 
  //read the super block
  disk_read_block(my_disk, 0, (char*)s_block);
  semaphore_P(block_array[0]->block_sem);

  curr_block_num = s_block->u.hdr.root; //grab the root block number

  curr_runner += 1; //move curr_runner past '/'
  is_dir = 1;
  printf("Found root at %i\n", curr_block_num);
  while (curr_runner[0] != '\0'){
    if (strchr(curr_runner, '/') == NULL){ //check if '/' is not in path
      //last name in path, can be file or a directory
      name_len = strlen(curr_runner);
      memcpy(curr_dir_name, curr_runner, name_len+1); //copy everything including null char
      is_dir = 0;
    }
    else if (strchr(curr_runner, '/') == 0){ //duplicate /'s in the path name
      curr_runner++;
      continue;
    }
    else {
      name_len = (int)(strchr(curr_runner, '/') - curr_runner);
      memcpy(curr_dir_name, curr_runner, name_len);
      curr_dir_name[name_len] = '\0'; //null terminate to make string
      is_dir = 1;
    }
    disk_read_block(my_disk, curr_block_num, (char*)i_block); //get the root inode
    semaphore_P(block_array[curr_block_num]->block_sem);

    tmp_file->inode_num = curr_block_num; //init tmp_file before iterator
    tmp_file->block_cursor = 0;
    entries_total = i_block->u.hdr.count;
    curr_block_num = -1; //set to -1 to check at end of loop
    entries_read = 0; //set to 0 before reading
    printf("looking for %s\n", curr_dir_name);
    while ( entries_read < entries_total
            && curr_block_num == -1
            && minifile_get_next_block(tmp_file) == 0 ){
      //search through the directory
      for (i = 0; i < MAX_DIR_ENTRIES_PER_BLOCK && entries_read < entries_total; i++){
        //name match
        if (strcmp(curr_dir_name, tmp_file->d_block.u.dir_hdr.data[i].name) == 0){
          //check if it has to be a directory & if so, if inode is directory type
          if (!is_dir || (is_dir && tmp_file->d_block.u.dir_hdr.data[i].type == DIR_t)) {
            curr_block_num = tmp_file->d_block.u.dir_hdr.data[i].block_num;
            if (is_dir){
              curr_runner += name_len + 1; //get past the next '/' char
            }
            else {
              curr_runner += name_len; //get to null character
            }
            printf("Found directory %s\n", curr_dir_name);
            break;
          }
          else {
            free(curr_dir_name);//make sure to free before return
            free(abs_dir);
            free(s_block);
            free(i_block);
            free(tmp_file);
            printf("Found name but not matching type\n");
            return -1; //could not find directory with the right name
          }
        }
        entries_read++;
      }
    }
    if ( curr_block_num == -1 ) { //not found
      free(curr_dir_name);//make sure to free before return
      free(abs_dir);
      free(s_block);
      free(i_block);
      free(tmp_file);
      printf("Entries read %d Entries total %d\n", entries_read, entries_total);
      printf("Exhausted search\n");
      return -1; //error case
    }
  }
  free(curr_dir_name);//make sure to free before return
  free(abs_dir);
  free(tmp_file);
  free(s_block);
  printf("FOUND!!!\n");
  return curr_block_num;
}

/* Does things in this order:
 * 1) updates count in inode
 * 2) sets ptr to new block
 * 3) writes data to new dblock 
 * 4) updates free_dblock pointers in superblock 
 * return 0 on success, -1 on failure
 * the handle is updated
 * return new_dblock block_num on success, -1 on failure
 */
int minifile_new_dblock(minifile_t handle, data_block* data, int add_count) {
  super_block* super;
  data_block* tmp;
  int block_idx, block_num, new_block;

  printf("enter new_dblock\n");
  if (handle == NULL || data == NULL || add_count < 1) {
    printf("invalid params\n");
    return -1;
  }
  
  disk_read_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
  semaphore_P(block_array[handle->inode_num]->block_sem);  
  
  super = (super_block*)calloc(1, sizeof(super_block));
  disk_read_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);  

  if (!(super->u.hdr.free_dblock_hd)) {
    // no free dblocks
    printf("no more free dblocks\n");
    free(super);
    return -1;
  }

  // inc count
  handle->i_block.u.hdr.count += add_count;

  // find out which dblock to alloc
  if (handle->i_block.u.hdr.type == DIR_t) {
    block_idx = handle->i_block.u.hdr.count / MAX_DIR_ENTRIES_PER_BLOCK;
    if ((handle->i_block.u.hdr.count) % MAX_DIR_ENTRIES_PER_BLOCK) {
      block_idx++;
    } 
  }
  else { // FILE_t
    block_idx = handle->i_block.u.hdr.count / DATA_BLOCK_SIZE;
    if ((handle->i_block.u.hdr.count) % DATA_BLOCK_SIZE) {
      block_idx++;
    }
  }

  if (block_idx >= NUM_PTRS) {
    printf("file size exceeded limit\n");
    free(super);
    return -1;
  }

  printf("fetching new data block at idx %d\n", block_idx);
  new_block = super->u.hdr.free_dblock_hd;

  if (block_idx < 11) {
    // the block is a direct ptr
    handle->block_cursor = block_idx;
    handle->i_block.u.hdr.d_ptrs[block_idx] = new_block;
    
    disk_write_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
    semaphore_P(block_array[handle->inode_num]->block_sem);  
    printf("modify inode to point to new dblock\n");
  }
  else {
    // first flush new count to disk
    disk_write_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
    semaphore_P(block_array[handle->inode_num]->block_sem);  

    block_idx -= 11;
    printf("reading indirect block\n");
    block_num = handle->i_block.u.hdr.i_ptr;
    disk_read_block(my_disk, block_num, (char*)&(handle->indirect_block));
    semaphore_P(block_array[block_num]->block_sem);  
    handle->indirect_block.u.indirect_hdr.d_ptrs[block_idx] = new_block;
    disk_write_block(my_disk, block_num, (char*)&(handle->indirect_block));
    semaphore_P(block_array[block_num]->block_sem);  
    printf("modify indirect block to point to new dblock\n");
  }

  tmp = (data_block*)calloc(1, sizeof(data_block));

  disk_read_block(my_disk, new_block, (char*)tmp);
  semaphore_P(block_array[new_block]->block_sem);  

  disk_write_block(my_disk, new_block, (char*)data);
  semaphore_P(block_array[new_block]->block_sem);  
    
  if (tmp->u.hdr.next == 0) {
    // empty list
    super->u.hdr.free_dblock_hd = 0;
    super->u.hdr.free_dblock_tl = 0;
  }
  else {
    super->u.hdr.free_dblock_hd = tmp->u.hdr.next;
  } 
  
  disk_write_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);  
  
  free(super);
  free(tmp);
  printf("exit new_dblock on success\n");
  return new_block;
}

/* Does things in this order:
 * 1) updates count in directory
 * 2) adds mapping to directory
 * 3) writes data to new inode
 * 4) updates free_iblock pointers in superblock 
 * return new_inode block_num on success, -1 on failure
 */
int minifile_new_inode(minifile_t handle, char* name, char type) {
  super_block* super;
  data_block* data;
  inode_block* inode;
  inode_block* tmp;
  int entry_idx, block_idx, block_num, new_block;
  
  printf("enter new_inode\n");
  if (handle == NULL || name == NULL || strlen(name) > 256) {
    printf("invalid params\n");
    return -1;
  }
  
  disk_read_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
  semaphore_P(block_array[handle->inode_num]->block_sem);  
  
  if (handle->i_block.u.hdr.type != DIR_t) {
    printf("param is not a directory\n");
    return -1;
  }

  // got a directory yo
  super = (super_block*)calloc(1, sizeof(super_block));
  disk_read_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);  

  if (!(super->u.hdr.free_iblock_hd)) {
    // no free iblocks
    printf("no more free inodes\n");
    free(super);
    return -1;
  }

  if ((handle->i_block.u.hdr.count)++ >= MAX_DIR_ENTRIES) {
    printf("directory full\n");
    free(super);
    return -1;
  }

  // find next spot to write to
  if ((handle->i_block.u.hdr.count % MAX_DIR_ENTRIES_PER_BLOCK) == 0) {
    // gotta get a new block
    // do what you gotta do....that's yo job!
    printf("curr data block is full\n");
    data = (data_block*)calloc(1, sizeof(data_block));
    data->u.dir_hdr.status = IN_USE;
    strcpy((char*)(data->u.dir_hdr.data), name);
    data->u.dir_hdr.data[0].block_num = super->u.hdr.free_iblock_hd;
    data->u.dir_hdr.data[0].type = type;  
    disk_write_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
    semaphore_P(block_array[handle->inode_num]->block_sem);  
    minifile_new_dblock(handle, data, 1);
    free(data); 
  }
  else {
    // fetch the inode
    // fetch the last data block and add to it
    block_idx = handle->i_block.u.hdr.count / MAX_DIR_ENTRIES_PER_BLOCK;
    entry_idx = handle->i_block.u.hdr.count % MAX_DIR_ENTRIES_PER_BLOCK - 1;

    disk_write_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
    semaphore_P(block_array[handle->inode_num]->block_sem);  

    printf("fetching data block to add dir entry\n");
    printf("block %d at entry %d\n", block_idx, entry_idx);
    // read the inode
    if (block_idx < 11) {
      // the block is a direct ptr
      handle->block_cursor = block_idx;
      block_num = handle->i_block.u.hdr.d_ptrs[block_idx];
    }
    else {
      // the block is a indirect ptr
      handle->block_cursor = block_idx;
      block_idx -= 11;
      printf("reading indirect ptr\n");
      block_num = handle->i_block.u.hdr.i_ptr;
      disk_read_block(my_disk, block_num, (char*)&(handle->indirect_block));
      semaphore_P(block_array[block_num]->block_sem);  
      block_num = handle->indirect_block.u.indirect_hdr.d_ptrs[block_idx]; 
    }
    printf("reading direct ptr\n");
    disk_read_block(my_disk, block_num, (char*)&(handle->d_block));
    semaphore_P(block_array[block_num]->block_sem);  
    strcpy((char*)&(handle->d_block.u.dir_hdr.data[entry_idx]), name);
    handle->d_block.u.dir_hdr.data[entry_idx].block_num 
        = super->u.hdr.free_iblock_hd; 
    handle->d_block.u.dir_hdr.data[entry_idx].type = type;  
    // done modifying da dir
    disk_write_block(my_disk, block_num, (char*)&(handle->d_block));
    semaphore_P(block_array[block_num]->block_sem);  
  }
  // writing data to new inode
  inode = (inode_block*)calloc(1, sizeof(inode_block));
  tmp = (inode_block*)calloc(1, sizeof(inode_block));
  inode->u.hdr.status = IN_USE;
  inode->u.hdr.type = type;
  
  new_block = super->u.hdr.free_iblock_hd;
  disk_read_block(my_disk, super->u.hdr.free_iblock_hd, (char*)tmp);
  semaphore_P(block_array[super->u.hdr.free_iblock_hd]->block_sem);  
  
  disk_write_block(my_disk, super->u.hdr.free_iblock_hd, (char*)inode);
  semaphore_P(block_array[super->u.hdr.free_iblock_hd]->block_sem);  
   
  if (tmp->u.hdr.next == 0) {
    // empty list
    super->u.hdr.free_iblock_hd = 0;
    super->u.hdr.free_iblock_tl = 0;
  }
  else {
    super->u.hdr.free_iblock_hd = tmp->u.hdr.next;
  } 
  
  disk_write_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);  
  
  free(super);
  free(inode);
  free(tmp);
  printf("exit new_inode on success\n");
  return new_block;
}

minifile_t minifile_creat(char *filename){
  semaphore_P(disk_op_lock);
  printf("enter minifile_creat\n");
  semaphore_V(disk_op_lock);
  return NULL;
}

minifile_t minifile_open(char *filename, char *mode){
  minifile_t handle;

  if (filename == NULL || mode == NULL) {
    return NULL;
  }

  semaphore_P(disk_op_lock);
  printf("enter minifile_open\n");

  handle = (minifile_t)calloc(1, sizeof(struct minifile));

  if (!strcmp(mode, "r")) {  
    handle->mode = READ;
  }
  else if (!strcmp(mode, "r+") || !strcmp(mode, "w+")) {  
    handle->mode = READ_WRITE;
  }
  else if (!strcmp(mode, "w")) {  
    handle->mode = WRITE;
  }
  else if (!strcmp(mode, "a")) {  
    handle->mode = APPEND;
  }
  else if (!strcmp(mode, "a+")) {  
    handle->mode = READ_APPEND;
  }
  else {
    printf("mode not recognized\n");
    free(handle);
    return NULL;
  }

  handle->inode_num = minifile_get_block_from_path(filename);

  if (handle->inode_num == -1) {
    free(handle);
    printf("%s", filename);
    printf(": No such file or directory\n");
    semaphore_V(disk_op_lock);
    return NULL;
  }  

  // grab file lock yo
  semaphore_P(inode_lock_table[handle->inode_num]);

  if (minifile_get_next_block(handle) == -1) {
    free(handle);
    semaphore_V(inode_lock_table[handle->inode_num]);
    semaphore_V(disk_op_lock);
    return NULL;
  }
    
  if (handle->i_block.u.hdr.type == DIR_t) {
    free(handle);
    printf("open called on a directory\n");
    semaphore_V(inode_lock_table[handle->inode_num]);
    semaphore_V(disk_op_lock);
    return NULL;
  }

  semaphore_V(disk_op_lock);
  return handle;
}

int minifile_read(minifile_t file, char *data, int maxlen){
  semaphore_P(disk_op_lock);
  printf("enter minifile_read\n");
  semaphore_V(disk_op_lock);
  return -1;
}

int minifile_write(minifile_t file, char *data, int len){
  semaphore_P(disk_op_lock);
  printf("enter minifile_write\n");
  semaphore_V(disk_op_lock);
  return -1;
}

int minifile_close(minifile_t file){
  printf("enter minifile_close\n");

  if (!file) {
    return -1;
  }

  semaphore_P(disk_op_lock);
  semaphore_V(inode_lock_table[file->inode_num]);
  free(file);
  semaphore_V(disk_op_lock);
  return -1;
}

int minifile_unlink(char *filename){
  semaphore_P(disk_op_lock);
  printf("enter minifile_unlink\n");
  semaphore_V(disk_op_lock);
  return -1;
}

int minifile_mkdir(char *dirname){
  int name_len;
  char* parent_dir;
  char* new_dir_name;
  inode_block* parent_block;
  int i;
  int parent_block_num;
  int child_block_num;
  minifile* new_dir;
  minifile* child_file_ptr;
  data_block* new_block;

  printf("enter minifile_mkdir, directory at %s\n", dirname);
  
  if (!dirname || dirname[0] == '\0'){ //NULL string or empty string
    return -1;
  }
  if (dirname[0] == '/'){ //check path length for absolute path
    if (strlen(dirname) > MAX_PATH_SIZE)
      return -1;
  }
  else { //check path length for relative path
    if (strlen(dirname) + 1 + strlen(minithread_get_curr_dir()) > MAX_PATH_SIZE )
      return -1;
  }

  parent_dir = (char*)calloc(MAX_PATH_SIZE + 1, sizeof(char)); //allocate path holder
  new_dir_name = (char*)calloc(MAX_PATH_SIZE + 1, sizeof(char)); //allocate dir name holder
  strcpy(parent_dir, dirname);

  //clip off trailing /'s
  name_len = strlen(dirname);
  i = name_len - 1;
  while (parent_dir[i] == '/' && i >= 0){
    parent_dir[i] = '\0'; //nullify
    i--;
  }
  if (i < 0){ //if name was only /'s
    free(parent_dir);
    free(new_dir_name);
    return -1;
  }

  //get the name of the new directory
  while (parent_dir[i] != '/'){
    if (i == 0){
      if (parent_dir[i] == '/'){ //root dir
        strcpy(new_dir_name, parent_dir + 1);
      }
      else {
        strcpy(new_dir_name, parent_dir);
        parent_dir[0] = '.';
      }
      parent_dir[1] = '\0';
      break;
    }
    i--;
  }
  if (i != 0){
    strcpy(new_dir_name, parent_dir + i + 1);
    parent_dir[i+1] = '\0'; //nullify
  }
  printf("New directory: %s\n", new_dir_name);
  printf("Parent directory: %s\n", parent_dir);
  parent_block = (inode_block*)calloc(1, sizeof(inode_block));
  new_dir = (minifile*)calloc(1, sizeof(minifile));

  semaphore_P(disk_op_lock);
  
  printf("calling get block on %s\n", dirname);
  if (minifile_get_block_from_path(dirname) != -1){
    semaphore_V(disk_op_lock);
    printf("error. file exists\n");
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    free(new_dir);
    return -1;
  }
  parent_block_num = minifile_get_block_from_path(parent_dir);
  if (parent_block_num == -1){
    semaphore_V(disk_op_lock);
    printf("something went horribly wrong and we can't find the block\n");
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    free(new_dir);
    return -1;
  }

  disk_read_block(my_disk, parent_block_num, (char*)parent_block );
  semaphore_P(block_array[parent_block_num]->block_sem);
  printf("got the parent block! Now I just need to get a free inode and add it as an entry\n");
  
  //grab a free inode!
  new_dir->inode_num = parent_block_num;
  child_block_num = minifile_new_inode(new_dir, new_dir_name, DIR_t);
  if (child_block_num == -1) {
    semaphore_V(disk_op_lock);
    printf("failed on getting new inode! aaaahhhhhh\n");
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    free(new_dir);
    return -1;
  }
  //write in the . and .. directories!!
  new_block = (data_block*)calloc(1,sizeof(data_block));
  child_file_ptr = (minifile*)calloc(1, sizeof(minifile));
  child_file_ptr->inode_num = child_block_num;

  new_block->u.dir_hdr.status = IN_USE;
  strcpy(new_block->u.dir_hdr.data[0].name, ".");
  new_block->u.dir_hdr.data[0].block_num = child_block_num;
  new_block->u.dir_hdr.data[0].type = DIR_t;
  strcpy(new_block->u.dir_hdr.data[1].name, "..");
  new_block->u.dir_hdr.data[1].block_num = parent_block_num;
  new_block->u.dir_hdr.data[1].type = DIR_t;
  printf("calling get new block to store . and ..\n");
  if ( minifile_new_dblock(child_file_ptr, new_block, 2) == -1){
    semaphore_V(disk_op_lock);
    printf("failed on getting new d_block! aaaahhhhhh\n");
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    free(new_dir);
    free(new_block); 
    free(child_file_ptr); 
    return -1;
  }
  
  semaphore_V(disk_op_lock);
  free(parent_dir);
  free(new_dir_name);
  free(parent_block);
  free(new_dir);
  free(new_block); 
  free(child_file_ptr); 
  return -1;
}

int minifile_rmdir(char *dirname){
  semaphore_P(disk_op_lock);
  printf("enter minifile_rmdir\n");
  semaphore_V(disk_op_lock);
  return -1;
}

int minifile_stat(char *path){
  semaphore_P(disk_op_lock);
  printf("enter minifile_stat\n");
  semaphore_V(disk_op_lock);
  return -1;
} 

int minifile_cd(char *path){
  char* curr_dir;
  int len;

  printf("enter minifile_cd\n");
  
  if (!path || path[0] == '\0'){ //NULL string or empty string
    return -1;
  }
  if (path[0] == '/'){ //check path length for absolute path
    if (strlen(path) > MAX_PATH_SIZE)
      return -1;
  }
  else { //check path length for relative path
    if (strlen(path) + 1 + strlen(minithread_get_curr_dir()) > MAX_PATH_SIZE )
      return -1;
  }

  semaphore_P(disk_op_lock);
  if (minifile_get_block_from_path(path) == -1){
    semaphore_V(disk_op_lock);
    printf("Directory not found\n");
    return -1;
  }
  //update currdir
  curr_dir = (char*)calloc(MAX_PATH_SIZE, sizeof(char));

  if (path[0] == '/'){
    strcpy(curr_dir,path);
    //just set the path to the absolute path passed in
    minithread_set_curr_dir(curr_dir);
  }
  else {
    strcpy(curr_dir, minithread_get_curr_dir());
    len = strlen(curr_dir);
    if (curr_dir[len-1] == '/'){ //ends with /
      strcpy(curr_dir + len, path); //copy relative path
    }
    else { //doesn't end with /
      curr_dir[len] = '/';
      strcpy(curr_dir + len + 1, path); //copy relative path
    }
    minithread_set_curr_dir(curr_dir);
  }
  semaphore_V(disk_op_lock);
  return 0;
}

char **minifile_ls(char *path){
  minifile_t handle;
  char** file_list;
  int i,j;
  char* tmp;

  if (!path) {
    return NULL;
  }

  semaphore_P(disk_op_lock);
  printf("enter minifile_ls\n");

  handle = (minifile_t)calloc(1, sizeof(struct minifile)); 
  handle->inode_num = minifile_get_block_from_path(path);
  printf("got past get_block\n");

  if (handle->inode_num == -1) {
    free(handle);
    printf("%s", path);
    printf(": No such file or directory\n");
    semaphore_V(disk_op_lock);
    return NULL;
  }  

  printf("calling get net block\n");
  if (minifile_get_next_block(handle) == -1) {
    printf("get next block failed\n");
    free(handle);
    semaphore_V(disk_op_lock);
    return NULL;
  }
  printf("get next block success\n");
    
  if (handle->i_block.u.hdr.type == FILE_t) {
    printf("ls called on a file type\n");
    free(handle);
    file_list = (char**)calloc(2, sizeof(char*));
    file_list[0] = path; 
    semaphore_V(disk_op_lock);
    return file_list;
  }
  
  file_list = (char**)calloc(handle->i_block.u.hdr.count + 1, sizeof(char*));
 
  printf("dir count is %d\n", handle->i_block.u.hdr.count);
  // we got a directory yo
  for (i = 0; i < handle->i_block.u.hdr.count; i++) {
    for (j = 0; ((j+i) < handle->i_block.u.hdr.count) &&
        (j < MAX_DIR_ENTRIES_PER_BLOCK); j++) {
      printf("reading the number %d entry\n", j+i);
      tmp = (char*)calloc(257, sizeof(char));
      strcpy(tmp, handle->d_block.u.dir_hdr.data[j].name);
      file_list[j+i] = tmp;
    }
    // copied an entire data block yo
    // get the next one, if exists
    if (((j+i) < handle->i_block.u.hdr.count) 
         && minifile_get_next_block(handle) == -1) {
      free(handle);
      semaphore_V(disk_op_lock);
      return file_list; 
    }
    i += j;
  }; 

  free(handle);
  semaphore_V(disk_op_lock);
  printf("exit ls on success\n");
  return file_list; 
}

/*
 * returns the current directory by strcpy-ing the curr_dir
 * */
char* minifile_pwd(void){
  char* user_curr_dir;

  semaphore_P(disk_op_lock); 
  user_curr_dir = (char*)calloc(strlen(minithread_get_curr_dir()) + 1, sizeof(char));
  strcpy(user_curr_dir, minithread_get_curr_dir());
  semaphore_V(disk_op_lock); 
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
  assert(inode->u.hdr.count == 2);

  // get dir entries
  block_num = inode->u.hdr.d_ptrs[0]; 
  disk_read_block(my_disk, block_num, out);
  semaphore_P(block_array[block_num]->block_sem);

  assert(((data_block*)out)->u.dir_hdr.status == IN_USE);
  assert(!strcmp(((data_block*)out)->u.dir_hdr.data[0].name,"."));
  assert(((data_block*)out)->u.dir_hdr.data[0].block_num == 1);
  assert(((data_block*)out)->u.dir_hdr.data[0].type == DIR_t);
  assert(!strcmp(((data_block*)out)->u.dir_hdr.data[1].name,".."));
  assert(((data_block*)out)->u.dir_hdr.data[1].block_num == 1);
  assert(((data_block*)out)->u.dir_hdr.data[1].type == DIR_t);
  
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
  
  semaphore_V(disk_op_lock); 
  free(out);
  printf("File System creation tested\n");
}

void minifile_make_fs(void) {
  super_block* super;
  inode_block* inode;
  data_block* data;
  int i;
  dir_entry path;
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
  super->u.hdr.free_dblock_hd = DATA_START + 1; 
  super->u.hdr.free_dblock_tl = BLOCK_COUNT - 1; 
  super->u.hdr.root = 1;
  
  disk_write_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);
 
  // root 
  inode->u.hdr.status = IN_USE;
  inode->u.hdr.type = DIR_t;
  inode->u.hdr.count = 2; 
  inode->u.hdr.d_ptrs[0] = DATA_START;
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

  data->u.dir_hdr.status = IN_USE;
  path.name[0] = '.';
  path.name[1] = '\0';
  path.block_num = 1;
  path.type = DIR_t;

  memcpy((char*)(data->u.dir_hdr.data), (char*)&path, sizeof(dir_entry));
  
  path.name[1] = '.';
  path.name[2] = '\0';
  
  memcpy((char*)&(data->u.dir_hdr.data[1]), (char*)&path, sizeof(dir_entry));

  disk_write_block(my_disk, DATA_START, (char*)data);
  semaphore_P(block_array[DATA_START]->block_sem);

  data->u.file_hdr.status = FREE;
  // make linked list of free data blocks
  for (i = DATA_START + 1; i < BLOCK_COUNT - 1; i++) {
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

  inode_lock_table = (semaphore_t*)calloc(DATA_START, sizeof(semaphore_t));
  for (i = 0; i < DATA_START; i++) {
    inode_lock_table[i] = semaphore_create();
    semaphore_initialize(inode_lock_table[i], 1);
  }

  //install a handler
  install_disk_handler(minifile_disk_handler);

  disk_op_lock = semaphore_create();
  semaphore_initialize(disk_op_lock, 1);
  return 0;
}

