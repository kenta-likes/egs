#include "minifile.h"
#include "synch.h"
#include "disk.h"
#include "minithread.h"
#include "interrupts.h"

#define DATA_BLOCK_SIZE (DISK_BLOCK_SIZE - sizeof(int) - sizeof(char))
#define BLOCK_COUNT disk_size
#define NUM_DPTRS 11
#define MAX_PATH_SIZE 256 //account for null character at end
#define MAX_DIR_ENTRIES_PER_BLOCK (DATA_BLOCK_SIZE / sizeof(dir_entry)) 
#define INODE_START 2
#define DATA_START (BLOCK_COUNT/10 + 1)
#define MAX_INDIRECT_BLOCKNUM (DATA_BLOCK_SIZE / sizeof(int))
#define NUM_PTRS (NUM_DPTRS + MAX_INDIRECT_BLOCKNUM)
#define MAX_DIR_ENTRIES (MAX_DIR_ENTRIES_PER_BLOCK * NUM_PTRS)
#define MAX_FILE_SIZE (DATA_BLOCK_SIZE * (NUM_DPTRS + MAX_INDIRECT_BLOCKNUM))
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

typedef struct path_node{
  char name[MAX_PATH_SIZE];
  struct path_node* next;
  struct path_node* prev;
} path_node;

typedef struct path_list{
  int len;
  path_node* hd;
  path_node* tl;
} path_list;


typedef struct minifile {
  int inode_num;
  int byte_cursor; // only used by read and write
                   // no other functions touch this
  inode_block i_block;
  data_block indirect_block;
  data_block d_block;
  int block_cursor; // block_idx of next dblock to read
  char mode; // only used by read and write
             // no other functions touch this
} minifile;

typedef struct block_ctrl{
  semaphore_t block_sem;
  disk_interrupt_arg_t* block_arg;
} block_ctrl;

typedef block_ctrl* block_ctrl_t;

enum { FREE = 1, IN_USE };
enum { DIR_t = 1, FILE_t }; 
enum { READ = 1, WRITE, READ_WRITE, APPEND, READ_APPEND }; 

/* GLOBAL VARS */ 
int disk_size; 
const char* disk_name; 
block_ctrl_t* block_array = NULL; 
semaphore_t disk_op_lock = NULL; 
disk_t* my_disk = NULL; 
semaphore_t* inode_lock_table; 

/* FUNC DEFS */ 

void blankify(char* buff, int num);
int math_ceil(int x, int y);
minifile_t minifile_create_handle(int inode_num);
int minifile_get_curr_block_num(minifile_t handle);

/* writes num 0's starting at buff
 */
void blankify(char* buff, int num) {
  int i;
  for (i = 0; i < num; i++) buff[i] = 0;
}

/* computes a mathematical ceiling function 
*/
int math_ceil(int x, int y) {
  if (x % y) return (x / y) + 1; else return (x / y);
}

/* creates a minifile struct.
 * inode block and indirect block (if exists) are loaded in.
 */
minifile_t minifile_create_handle(int inode_num) {
  minifile_t handle;
  int block_num;

  if (inode_num < 1 || inode_num >= DATA_START) {
    printf("error: minifile_create_handle called on %d\n", inode_num);
    return NULL;
  }

  handle = (minifile_t)calloc(1, sizeof(minifile));
  if (!handle) return NULL;
  handle->inode_num = inode_num;
 
  // read in inode block 
  disk_read_block(my_disk, inode_num, (char*)&(handle->i_block));
  semaphore_P(block_array[inode_num]->block_sem);  
    
  if (handle->i_block.u.hdr.type == DIR_t) {
    printf("creating a DIR handle\n");
  }
  else if (handle->i_block.u.hdr.type == FILE_t) {
    printf("creating a FILE handle\n");
  }
  else {
    printf("creating a UNINITIALIZED handle\n");
  }

  if ((handle->i_block.u.hdr.type == DIR_t && 
      math_ceil(handle->i_block.u.hdr.count, MAX_DIR_ENTRIES_PER_BLOCK) > NUM_DPTRS) || 
      (handle->i_block.u.hdr.type == FILE_t && 
      math_ceil(handle->i_block.u.hdr.count, DATA_BLOCK_SIZE) > NUM_DPTRS)) {
    // load in indirect block
    block_num = handle->i_block.u.hdr.i_ptr;
    disk_read_block(my_disk, block_num, (char*)&(handle->indirect_block));
    semaphore_P(block_array[block_num]->block_sem);  
  }

  return handle;
}

/* returns the block num of the curr data block as pointed to by cursor.
 * assumes indirect block is loaded in if exists.
 * return -1 on failure
 */
int minifile_get_curr_block_num(minifile_t handle) {
  int block_idx;

  if (!handle) {
    printf("error: minifile_get_curr_block_num, null param\n");
    return -1;
  }

  block_idx = handle->block_cursor;
  if (block_idx < NUM_DPTRS) {
    // curr block is dptr
    return handle->i_block.u.hdr.d_ptrs[block_idx];
  }
  else {
    return handle->indirect_block.u.indirect_hdr.d_ptrs[block_idx-NUM_DPTRS]; 
  } 
}

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

void minifile_fix_fs() {
  printf("enter minifile_fix_fx\n");
  // TODO
  return;

}

void minifile_disk_error_handler(disk_interrupt_arg_t* block_arg) {
  semaphore_P(disk_op_lock);

  printf("enter minifile_disk_error_handler\n");
  switch (block_arg->reply) {
  case DISK_REPLY_FAILED:
    // gotta try again yo
    printf("disk reply failed on block %d\n", block_arg->request.blocknum);
    disk_send_request(block_arg->disk, block_arg->request.blocknum, 
        block_arg->request.buffer, block_arg->request.type);
    break;

  case DISK_REPLY_ERROR:
    printf("DISK_REPLY_ERROR wuttttt\n");
    printf("unknown error on block %d\n", block_arg->request.blocknum);
    break;

  case DISK_REPLY_CRASHED:
    printf("disk crashed\n");
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
  //read in the inode
  disk_read_block(my_disk, file_ptr->inode_num, (char*)(&file_ptr->i_block) );
  semaphore_P(block_array[file_ptr->inode_num]->block_sem);
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
    file_ptr->block_cursor++;
    return file_ptr->i_block.u.hdr.d_ptrs[index];
  }
  else {
    index -= 11; //get index in indirect block
    //read in the indirect block
    disk_read_block(my_disk, file_ptr->i_block.u.hdr.i_ptr, (char*)(&file_ptr->indirect_block) );
    semaphore_P(block_array[file_ptr->i_block.u.hdr.i_ptr]->block_sem);
    disk_read_block(my_disk, file_ptr->indirect_block.u.indirect_hdr.d_ptrs[index], (char*)(&file_ptr->d_block) );
    semaphore_P(block_array[file_ptr->indirect_block.u.indirect_hdr.d_ptrs[index]]->block_sem);
    file_ptr->block_cursor++;
    return file_ptr->indirect_block.u.indirect_hdr.d_ptrs[index];
  }
  return -1;
}

/*
 * Simplifies the path given
 * e.g. /hello/world/../.. -> /
 * Will modify whatever is handed in in-place
 * assumes length is maxed at MAX_PATH_SIZE
 * */
char* minifile_simplify_path(char* path){
  char* runner;
  char* runner_end;
  path_list* p_list;
  path_node* p_node;
  char* real_path;
  int real_path_len;
  int i;

  //construct the list structure out of the tokens
  p_list = (path_list*)calloc(1,sizeof(path_list));
  p_list->len = 1;
  //create the root node
  p_node = (path_node*)calloc(1,sizeof(path_node));
  strcpy(p_node->name, "/");
  p_list->hd = p_node;

  runner = path + 1;
  while (*runner == '/'){ //skip excessive /'s
    runner++;
  }
  runner_end = strchr(runner, '/');
  //set runner_end to location of null char
  if (runner_end == NULL){
    runner_end = runner + strlen(runner);
  }
  while (runner_end != runner){
    if ( (int)(runner_end - runner) == 1
          && (memcmp(runner, ".", (int)(runner_end - runner)) == 0) ){
      if (*runner_end != '\0'){
        runner = runner_end + 1;
      }
      else {
        runner = runner_end;
      }
    }
    else if ( (int)(runner_end - runner) >= 2
          && (memcmp(runner, "..",(int) (runner_end - runner)) == 0) ){
      if (p_list->len > 1){
        p_list->tl = p_list->tl->prev;
        p_node = p_list->tl;
        free(p_list->tl->next);
        p_list->tl->next = NULL;
        (p_list->len)--;
      }
      if (*runner_end != '\0'){
        runner = runner_end + 1;
      }
      else {
        runner = runner_end;
      }
    }
    else {
      p_node->next = (path_node*)calloc(1,sizeof(path_node));
      memcpy(p_node->next->name, runner, (int)(runner_end - runner));//store name
      p_node->next->prev = p_node;
      p_node = p_node->next;
      p_list->tl = p_node;
      (p_list->len)++;
      if (*runner_end != '\0'){
        runner = runner_end + 1;
      }
      else {
        runner = runner_end;
      }

      if (*runner == '\0'){ //ends with /
        p_node->next = (path_node*)calloc(1,sizeof(path_node));
        strcpy(p_node->next->name, "/");//store name
        p_node->next->prev = p_node;
        p_node = p_node->next;
        p_list->tl = p_node;
        (p_list->len)++;
      }
      else {
        p_node->name[strlen(p_node->name) + 1] = '\0';
        p_node->name[strlen(p_node->name)] = '/';
      }
      while (*runner == '/'){ //skip excessive /'s
        runner++;
      }
    }

    runner_end = strchr(runner, '/');
    if (runner_end == NULL){
      runner_end = runner + strlen(runner);
    }
  }

  real_path = (char*)calloc(MAX_PATH_SIZE + 1, sizeof(char));
  real_path_len = 0;
  p_node = p_list->hd;
  for (i = 0; i < p_list->len; i++){
    if (real_path_len + strlen(p_node->name) > MAX_PATH_SIZE){
      //free and move on...
      real_path_len = MAX_PATH_SIZE + 1;
      //free(real_path);
    }
    else {
      strcpy(real_path + real_path_len, p_node->name );
      real_path_len += strlen(p_node->name);
      p_node = p_node->next;
    }
    //free(p_node->prev);
  }
  //free(p_node);
  //free(p_list);
  if (real_path_len > MAX_PATH_SIZE){
    return NULL;
  }
  real_path[real_path_len] = '\0';
  return real_path;
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
  minifile_simplify_path(abs_dir); //simplify the path to avoid excessive reading
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
    else if (curr_runner[0] == '/'){ //duplicate /'s in the path name
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
            && minifile_get_next_block(tmp_file) != -1 ){
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
  printf("FOUND!!! block #%i\n", curr_block_num);
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
  int block_idx, block_num, new_block, next;

  printf("enter new_dblock\n");
  if (handle == NULL || data == NULL || add_count < 1) {
    printf("invalid params\n");
    return -1;
  }
  
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
  }
  else { // FILE_t
    block_idx = handle->i_block.u.hdr.count / DATA_BLOCK_SIZE;
  }

  if (block_idx >= NUM_PTRS) {
    printf("file size exceeded limit\n");
    free(super);
    return -1;
  }

  printf("fetching new data block at idx %d\n", block_idx);
  new_block = super->u.hdr.free_dblock_hd;

  if (new_block == 0) {
    printf("out of dblocks. abort!\n");
    free(super);
    return -1;
  }

  handle->block_cursor = block_idx;
  if (block_idx < NUM_DPTRS) {
    // the block is a direct ptr

    handle->i_block.u.hdr.d_ptrs[block_idx] = new_block;
    
    // flush new count and ptr to inode on disk
    disk_write_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
    semaphore_P(block_array[handle->inode_num]->block_sem);  
  }
  else {
    // first flush new count to disk
    disk_write_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
    semaphore_P(block_array[handle->inode_num]->block_sem);  

    if (block_idx == NUM_DPTRS) {
      printf("getting an indirect block\n");

      // need to get indirect block first
      tmp = (data_block*)calloc(1, sizeof(data_block));

      // read new dblock hd
      disk_read_block(my_disk, new_block, (char*)tmp);
      semaphore_P(block_array[new_block]->block_sem);  
      
      // store the second elt in the free dblock list
      next = tmp->u.hdr.next;

      // change ptr to free dblock hd
      handle->i_block.u.hdr.i_ptr = new_block;
      
      if (next == 0) {
        printf("out of dblocks. abort!\n");
        free(super);
        free(tmp);
        return -1;
      }

      // flush inode to disk 
      disk_write_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
      semaphore_P(block_array[handle->inode_num]->block_sem);  

      blankify((char*)&(handle->indirect_block), sizeof(data_block));
      handle->indirect_block.u.hdr.status = IN_USE;
     
      // flush changes to free dblock hd 
      disk_write_block(my_disk, new_block, (char*)&(handle->indirect_block));
      semaphore_P(block_array[new_block]->block_sem);  

      free(tmp);

      // update superblock ptr
      super->u.hdr.free_dblock_hd = next;
     
      disk_write_block(my_disk, 0, (char*)super);
      semaphore_P(block_array[0]->block_sem);  

      new_block = next;
    }

    block_num = handle->i_block.u.hdr.i_ptr; 
    handle->indirect_block.u.indirect_hdr.d_ptrs[block_idx-NUM_DPTRS] = new_block;
    disk_write_block(my_disk, block_num, (char*)&(handle->indirect_block));
    semaphore_P(block_array[block_num]->block_sem);  
    printf("modify indirect block to point to new dblock\n");
  }

  tmp = (data_block*)calloc(1, sizeof(data_block));

  // read free dlbock hd
  disk_read_block(my_disk, new_block, (char*)tmp);
  semaphore_P(block_array[new_block]->block_sem);  

  // overwrite free dlbock hd
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
  
  // flush changes to list ptrs to disk
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
  inode_block* inode;
  inode_block* tmp;
  int entry_idx, block_idx, block_num, new_block;
  
  printf("enter minifile_new_inode\n");
  if (handle == NULL || name == NULL || strlen(name) > 256) {
    printf("invalid params\n");
    return -1;
  }
 
  // note that i_block as already loaded 

  if (handle->i_block.u.hdr.type != DIR_t) {
    printf("param is not a directory\n");
    return -1;
  }

  // got a directory yo
  // read in super block
  super = (super_block*)calloc(1, sizeof(super_block));
  disk_read_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);  

  if (!(super->u.hdr.free_iblock_hd)) {
    // no free iblocks
    printf("no more free inodes\n");
    free(super);
    return -1;
  }

  if (handle->i_block.u.hdr.count >= MAX_DIR_ENTRIES) {
    printf("directory full\n");
    free(super);
    return -1;
  }

  if (handle->i_block.u.hdr.count % MAX_DIR_ENTRIES_PER_BLOCK) {
    // can write to an existing dir data block
    block_idx = handle->i_block.u.hdr.count / MAX_DIR_ENTRIES_PER_BLOCK;
    entry_idx = handle->i_block.u.hdr.count % MAX_DIR_ENTRIES_PER_BLOCK;

    // flush to count to inode
    (handle->i_block.u.hdr.count)++;
    disk_write_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
    semaphore_P(block_array[handle->inode_num]->block_sem);  

    printf("block %d at entry %d\n", block_idx, entry_idx);
    handle->block_cursor = block_idx;

    // read in dblock
    block_num = minifile_get_next_block(handle);
    if (block_num == -1) {
      printf("get next block failed. abort!\n");
      free(super);
      return -1;
    }
      
    strcpy((char*)&(handle->d_block.u.dir_hdr.data[entry_idx]), name);
    handle->d_block.u.dir_hdr.data[entry_idx].block_num 
        = super->u.hdr.free_iblock_hd; 
    handle->d_block.u.dir_hdr.data[entry_idx].type = type;  

    // flush data to disk
    disk_write_block(my_disk, block_num, (char*)&(handle->d_block));
    semaphore_P(block_array[block_num]->block_sem);  
  }
  else {
    printf("curr data block is full\n");
    // gotta get a new block

    // blankify dblock
    blankify((char*)&(handle->d_block), sizeof(data_block));

    // write into first entry
    strcpy((char*)(handle->d_block.u.dir_hdr.data), name);
    handle->d_block.u.dir_hdr.data[0].block_num = super->u.hdr.free_iblock_hd;
    handle->d_block.u.dir_hdr.data[0].type = type;  

    minifile_new_dblock(handle, &(handle->d_block), 1);
    // change new_block to update hdr stuff 
    disk_read_block(my_disk, 0, (char*)super);
    semaphore_P(block_array[0]->block_sem);  
  }

  inode = (inode_block*)calloc(1, sizeof(inode_block));
  tmp = (inode_block*)calloc(1, sizeof(inode_block));

  inode->u.hdr.status = IN_USE;
  inode->u.hdr.type = type;
  
  // get free iblock hd so data isn't lost
  new_block = super->u.hdr.free_iblock_hd;
  disk_read_block(my_disk, new_block, (char*)tmp);
  semaphore_P(block_array[new_block]->block_sem);  
  
  // flush new inode stuff to iblock hd
  disk_write_block(my_disk, new_block, (char*)inode);
  semaphore_P(block_array[new_block]->block_sem);  
   
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

/*
 *  Splits dirname into parent and child components, and
 *  stores them into a variable the caller uses
 * */
int minifile_get_parent_child_paths(char** parent_dir, char** new_dir_name, char* dirname){
  int name_len;
  int i;
  *parent_dir = (char*)calloc(MAX_PATH_SIZE + 1, sizeof(char)); //allocate path holder
  *new_dir_name = (char*)calloc(MAX_PATH_SIZE + 1, sizeof(char)); //allocate dir name holder
  strcpy(*parent_dir, dirname);

  //clip off trailing /'s
  name_len = strlen(dirname);
  i = name_len - 1;
  while ((*parent_dir)[i] == '/' && i >= 0){
    (*parent_dir)[i] = '\0'; //nullify
    i--;
  }
  if (i < 0){ //if name was only /'s
    free(*parent_dir);
    free(*new_dir_name);
    return -1;
  }

  //get the name of the new directory
  while ((*parent_dir)[i] != '/'){
    if (i == 0){
      strcpy(*new_dir_name, *parent_dir);
      (*parent_dir)[0] = '.';
      (*parent_dir)[1] = '\0';
      break;
    }
    i--;
  }
  if (i != 0){
    strcpy(*new_dir_name, (*parent_dir) + i + 1);
    (*parent_dir)[i+1] = '\0'; //nullify
  }
  else if ((*parent_dir)[0] == '/') {
    strcpy(*new_dir_name, (*parent_dir) + 1);
    (*parent_dir)[1] = '\0';
  }

  return 0;

}

minifile_t minifile_creat(char *filename){
  char* parent_dir;
  char* new_dir_name;
  inode_block* parent_block;
  int parent_block_num;
  int child_block_num;
  minifile* new_dir;
  minifile* child_file_ptr;

  printf("enter minifile_creat, directory at %s\n", filename);
  
  if (!filename || filename[0] == '\0'){ //NULL string or empty string
    return NULL;
  }
  if (filename[0] == '/'){ //check path length for absolute path
    if (strlen(filename) > MAX_PATH_SIZE)
      return NULL; 
  }
  else { //check path length for relative path
    if (strlen(filename) + 1 + strlen(minithread_get_curr_dir()) > MAX_PATH_SIZE )
      return NULL;
  }

  if (minifile_get_parent_child_paths(&parent_dir, &new_dir_name, filename) == -1){
    return NULL;
  }

  printf("New directory: %s\n", new_dir_name);
  printf("Parent directory: %s\n", parent_dir);
  parent_block = (inode_block*)calloc(1, sizeof(inode_block));

  semaphore_P(disk_op_lock);
  
  printf("calling get block on %s\n", filename);
  if (minifile_get_block_from_path(filename) != -1){
    semaphore_V(disk_op_lock);
    printf("error. file exists\n");
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    return NULL;
  }
  parent_block_num = minifile_get_block_from_path(parent_dir);
  if (parent_block_num == -1){
    semaphore_V(disk_op_lock);
    printf("something went horribly wrong and we can't find the block\n");
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    return NULL;
  }

  disk_read_block(my_disk, parent_block_num, (char*)parent_block );
  semaphore_P(block_array[parent_block_num]->block_sem);
  printf("got the parent block! Now I just need to get a free inode and add it as an entry\n");
  
  //grab a free inode!
  new_dir = minifile_create_handle(parent_block_num);
  if (!new_dir){
    printf("NULL ON MINIFILE_CREATE\n");
    semaphore_V(disk_op_lock);
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    return NULL;
  }
  child_block_num = minifile_new_inode(new_dir, new_dir_name, FILE_t);
  if (child_block_num == -1) {
    semaphore_V(disk_op_lock);
    printf("failed on getting new inode! aaaahhhhhh\n");
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    free(new_dir);
    return NULL;
  }
  child_file_ptr = (minifile*)calloc(1, sizeof(minifile));
  child_file_ptr->inode_num = child_block_num;
  child_file_ptr->byte_cursor = 0;
  child_file_ptr->block_cursor = 0;
  
  semaphore_V(disk_op_lock);
  free(parent_dir);
  free(new_dir_name);
  free(parent_block);
  free(new_dir);
  return child_file_ptr;
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
  int bytes_read;

  bytes_read = 0;
  if (maxlen > MAX_FILE_SIZE){
    printf("file too large to read\n");
    return -1;
  }
  semaphore_P(disk_op_lock);
  printf("enter minifile_read\n");
  while (bytes_read < maxlen && minifile_get_next_block(file) != -1){
    if (maxlen - bytes_read > DATA_BLOCK_SIZE){ //more space in buffer
      if (file->i_block.u.hdr.count - file->byte_cursor > DATA_BLOCK_SIZE){ //more left to read in file
        memcpy(data + bytes_read, file->d_block.u.file_hdr.data, DATA_BLOCK_SIZE);
        bytes_read += DATA_BLOCK_SIZE;
      }
      else { //less than block size left to read in file
        memcpy(data + bytes_read, file->d_block.u.file_hdr.data, file->i_block.u.hdr.count-file->byte_cursor);
        bytes_read += file->i_block.u.hdr.count-file->byte_cursor;
      }
      //keep reading
    }
    else { //no more space in buffer, this should be last block
      if (file->i_block.u.hdr.count - file->byte_cursor > maxlen - bytes_read){ //buffer size smaller
        memcpy(data + bytes_read, file->d_block.u.file_hdr.data, maxlen - bytes_read);
        bytes_read += maxlen - bytes_read;
      }
      else { // file content left smaller
        memcpy(data + bytes_read, file->d_block.u.file_hdr.data, file->i_block.u.hdr.count - file->byte_cursor);
        bytes_read += file->i_block.u.hdr.count - file->byte_cursor;
      }
    }
  }
  semaphore_V(disk_op_lock);
  return 0;
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

/* return -1 on failure and 0 on success
 */
int minifile_rm_dblock(minifile_t handle, int block_idx) {
  super_block* super;
  data_block* tmp;
  int tl, block_num, max_count;

  printf("enter minifile_rm_dblock\n");

  if (!handle || block_idx < 0 || block_idx >= NUM_PTRS) {
    printf("invalid params\n");
    return -1;
  }

  disk_read_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
  semaphore_P(block_array[handle->inode_num]->block_sem);  

  if (handle->i_block.u.hdr.type != FILE_t) {
    printf("error: input not file\n");
  }

  handle->block_cursor = block_idx;
  if (block_idx < 11) {
    // the block is a direct ptr
    block_num = handle->i_block.u.hdr.d_ptrs[block_idx];
  }
  else {
    printf("reading indirect block\n");
    block_num = handle->i_block.u.hdr.i_ptr;
    disk_read_block(my_disk, block_num, (char*)&(handle->indirect_block));
    semaphore_P(block_array[block_num]->block_sem);  
    block_num = handle->indirect_block.u.indirect_hdr.d_ptrs[block_idx-11];
  }

  if (block_num == 0) {
    printf("error: data block ptr is not initialized\n");
    return -1;
  }

  super = (super_block*)calloc(1, sizeof(super_block));
  disk_read_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);  

  tl = super->u.hdr.free_dblock_tl;

  if (tl) {
    // list contains at least one elt
    tmp = (data_block*)calloc(1, sizeof(data_block));
    disk_read_block(my_disk, tl, (char*)tmp);
    semaphore_P(block_array[tl]->block_sem);  
    tmp->u.hdr.next = block_num;
    disk_write_block(my_disk, tl, (char*)tmp);
    semaphore_P(block_array[tl]->block_sem);  
    free(tmp);
    super->u.hdr.free_dblock_tl = block_num;
    // tmp contains old tail of free dblock list 
  }
  else {
    super->u.hdr.free_dblock_tl = block_num;
    super->u.hdr.free_dblock_hd = block_num;
  }
  
  // update count in handle inode 
  if (handle->i_block.u.hdr.type == FILE_t) {
    max_count = block_idx * DATA_BLOCK_SIZE;
  }
  else { // type is DIR_t
    max_count = block_idx * MAX_DIR_ENTRIES_PER_BLOCK;
  }   
  if (handle->i_block.u.hdr.count > max_count) {
    handle->i_block.u.hdr.count = max_count;
  }

  // Nullify the ptr
  if (block_idx < 11) {
    // the block is a direct ptr
    handle->i_block.u.hdr.d_ptrs[block_idx] = 0;
    disk_write_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
    semaphore_P(block_array[handle->inode_num]->block_sem);  
    printf("Nullified ptr...and updated count\n");
  }
  else {
    handle->indirect_block.u.indirect_hdr.d_ptrs[block_idx-11] = 0;
    disk_write_block(my_disk, handle->i_block.u.hdr.i_ptr, 
        (char*)&(handle->indirect_block));
    semaphore_P(block_array[handle->i_block.u.hdr.i_ptr]->block_sem);  
    printf("Nullified ptr...");

    disk_write_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
    semaphore_P(block_array[handle->inode_num]->block_sem);  
    printf("and updated count\n");
  }

  tmp = (data_block*)calloc(1, sizeof(data_block));
  tmp->u.hdr.status = FREE;
  disk_write_block(my_disk, block_num, (char*)tmp);
  semaphore_P(block_array[block_num]->block_sem);  
  free(tmp);
  
  disk_write_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);  
    
  free(super);
  printf("exit minifile_rm_dblock on success\n");
  return 0;
}

int minifile_unlink(char *filename){
  /*
  int parent_block_num, i, j, name_len; 
  int tl, dblock_num, dir_entry_num;
  int found_entry;
  char* parent_dir;
  char* del_file;
  char* tmp_str;
  minifile_t parent;
  inode_block* tmp;
  super_block* super;
  minifile_t handle;

  if (!filename) {
    printf("invalid params\n");
    return -1;
  }

  semaphore_P(disk_op_lock);
  printf("enter minifile_unlink\n");

  handle = (minifile_t)calloc(1, sizeof(minifile));
  handle->inode_num = minifile_get_block_from_path(filename);
  if (handle->inode_num == -1) {
    printf("error: %s not a file or directory\n", filename);
    free(handle);
    semaphore_V(disk_op_lock);
    return -1;
  }

  // grab the file lock
  semaphore_P(inode_lock_table[handle->inode_num]);

  // read in the inode
  disk_read_block(my_disk, handle->inode_num, (char*)&(handle->i_block));
  semaphore_P(block_array[handle->inode_num]->block_sem);  

  // check that file type is FILE_t
  if (handle->i_block.u.hdr.type != FILE_t) {
    printf("unlink called on non-file type\n");
    free(handle);
    semaphore_V(inode_lock_table[handle->inode_num]);
    semaphore_V(disk_op_lock);
    return -1;
  }

  parent_dir = (char*)calloc(MAX_PATH_SIZE + 1, sizeof(char));
  del_file = (char*)calloc(MAX_PATH_SIZE + 1, sizeof(char));
  strcpy(parent_dir, filename);

  //clip off trailing /'s
  name_len = strlen(filename);
  i = name_len - 1;
  while (parent_dir[i] == '/' && i >= 0){
    parent_dir[i] = '\0'; //nullify
    i--;
  }
  if (i < 0){ //if name was only /'s
    printf("error: name only contains /'s\n");
    free(handle);
    semaphore_V(inode_lock_table[handle->inode_num]);
    semaphore_V(disk_op_lock);
    return -1;
  }

  //get the name of the new directory
  while (parent_dir[i] != '/'){
    if (i == 0){
      if (parent_dir[i] == '/'){ //root dir
        strcpy(del_file, parent_dir + 1);
      }
      else {
        strcpy(del_file, parent_dir);
        parent_dir[0] = '.';
      }
      parent_dir[1] = '\0';
      break;
    }
    i--;
  }
  if (i != 0){
    strcpy(del_file, parent_dir + i + 1);
    parent_dir[i+1] = '\0'; //nullify
  }
  printf("file to delete: %s\n", del_file);
  printf("Parent directory: %s\n", parent_dir);

  parent_block_num = minifile_get_block_from_path(parent_dir);

  // load the parent
  parent = (minifile_t)calloc(1, sizeof(minifile));
  disk_read_block(my_disk, parent_block_num, (char*)&(parent->i_block));
  semaphore_P(block_array[parent_block_num]->block_sem);  

  // sanity check: parent is a dir
  if (parent->i_block.u.hdr.type != DIR_t) {
    printf("huh? parent is not a directory\n");
    free(parent);
    free(handle);
    semaphore_V(inode_lock_table[handle->inode_num]);
    semaphore_V(disk_op_lock);
    return -1;
  }
 
  // rm all the dblocks
  dblock_num = handle->i_block.u.hdr.count / DATA_BLOCK_SIZE - 1; 
  if (handle->i_block.u.hdr.count % DATA_BLOCK_SIZE) {
    // part of next block used
    dblock_num++; 
  }
   
  for (i = dblock_num; i >= 0; i--) {
    if (minifile_rm_dblock(handle, i)) {
      // failed
      printf("rm a dblock failed. abort!\n");
      free(parent);
      free(handle);
      semaphore_V(inode_lock_table[handle->inode_num]);
      semaphore_V(disk_op_lock);
      return -1;
    }
  }
  
  printf("all data blocks freed\n");
       
  super = (super_block*)calloc(1, sizeof(super_block));
  disk_read_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);  

  tl = super->u.hdr.free_iblock_tl;

  if (tl) {
    // list contains at least one elt
    tmp = (inode_block*)calloc(1, sizeof(inode_block));
    disk_read_block(my_disk, tl, (char*)tmp);
    semaphore_P(block_array[tl]->block_sem);  
    // put myself end of free list
    tmp->u.hdr.next = handle->inode_num;
    // flush change to disk
    disk_write_block(my_disk, tl, (char*)tmp);
    semaphore_P(block_array[tl]->block_sem);  
    free(tmp);
    super->u.hdr.free_iblock_tl = handle->inode_num;
    // modify tl ptr, not flushed yet
  }
  else {
    super->u.hdr.free_iblock_tl = handle->inode_num;
    super->u.hdr.free_iblock_hd = handle->inode_num;
    // modify ptrs, not flushed yet
  }
 
  // nullify and label free 
  tmp = (inode_block*)calloc(1, sizeof(inode_block));
  tmp->u.hdr.status = FREE;
  // next is already null
  disk_write_block(my_disk, handle->inode_num, (char*)tmp);
  semaphore_P(block_array[tl]->block_sem);  
  free(tmp);

  // update parent dir
  dir_entry_num = parent->i_block.u.hdr.count;

  found_entry = 0;
  tmp_str = (char*)calloc(257, sizeof(char));
  for (i = 0; i < dir_entry; i++) {
    for (j = 0; ((j+i) < dir_entry_num) 
          &&(j < MAX_DIR_ENTRIES_PER_BLOCK); j++) {
      printf("reading the number %d entry\n", j+i);
      if (found_entry) {
        // swap this one and next
          
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
  free(tmp_str);

  semaphore_V(inode_lock_table[handle->inode_num]);
  semaphore_V(disk_op_lock);
  printf("exit minifile_unlink on success\n");
  free(handle);
  */
  return 0;
}

int minifile_mkdir(char *dirname){
  char* parent_dir;
  char* new_dir_name;
  inode_block* parent_block;
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

  if (minifile_get_parent_child_paths(&parent_dir, &new_dir_name, dirname) == -1){
    return -1;
  }

  printf("New directory: %s\n", new_dir_name);
  printf("Parent directory: %s\n", parent_dir);
  parent_block = (inode_block*)calloc(1, sizeof(inode_block));

  semaphore_P(disk_op_lock);
  
  if (minifile_get_block_from_path(dirname) != -1){
    semaphore_V(disk_op_lock);
    printf("error. file exists\n");
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    return -1;
  }
  parent_block_num = minifile_get_block_from_path(parent_dir);
  if (parent_block_num == -1){
    semaphore_V(disk_op_lock);
    printf("something went horribly wrong and we can't find the block\n");
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    return -1;
  }

  disk_read_block(my_disk, parent_block_num, (char*)parent_block );
  semaphore_P(block_array[parent_block_num]->block_sem);
  printf("got the parent block! Now I just need to get a free inode and add it as an entry\n");
  
  //grab a free inode!
  new_dir = minifile_create_handle(parent_block_num);
  if (!new_dir) {
    printf("create handle failed. abort!\n");
    semaphore_V(disk_op_lock);
    free(parent_dir);
    free(new_dir_name);
    free(parent_block);
    return -1;
  }    
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
  child_file_ptr = minifile_create_handle(child_block_num);

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
  return 0;
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
  printf("==============================YERRR=====================\n");
  printf("%s",minifile_simplify_path(path));
  printf("\n");
  
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
    minifile_simplify_path(curr_dir);
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
    minifile_simplify_path(curr_dir);
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

  if (handle->inode_num == -1) {
    free(handle);
    printf("%s", path);
    printf(": No such file or directory\n");
    semaphore_V(disk_op_lock);
    return NULL;
  }  

  if (minifile_get_next_block(handle) == -1) {
    printf("get next block failed\n");
    free(handle);
    semaphore_V(disk_op_lock);
    return NULL;
  }
    
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
  for (i = 0; i < handle->i_block.u.hdr.count; i+=j) {
    for (j = 0; ((j+i) < handle->i_block.u.hdr.count) &&
        (j < MAX_DIR_ENTRIES_PER_BLOCK); j++) {
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
  } 

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

