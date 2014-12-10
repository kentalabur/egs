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
  int inode_num;
  int offset;
};

typedef struct block_ctrl{
  semaphore_t block_sem;
  disk_interrupt_arg_t* block_arg;
  
  char* buff;
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
 * Helper function to get block number from
 * a directory/file path
 * Returns: block number, -1 if path DNE
 * */
int minifile_get_block_from_path(char* dir_path){
  super_block* s_block;
  inode_block* i_block;
  char* abs_dir; //store absolute path here
  char* curr_dir_name; //use for holding current directory name
  char* curr_block;
  //int read_end;
  //int i;
  int curr_block_num;

  if (dir_path[0] == '\0'){
    printf("ERROR: looking up empty string");
    return -1;
  }
  
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

  curr_block = (char*)calloc(1, sizeof(super_block));
 
  //semaphore_P(disk_op_lock);........only reading, so maybe not necessary?
  //read the super block
  disk_read_block(my_disk, 0, (char*)curr_block);
  semaphore_P(block_array[0]->block_sem);
  s_block = (super_block*)curr_block;
  
  /* Do we need to check this?
  if (memchk(s_block->u.hdr.magic_num, magic, 4) != 0){
    printf("ERROR: Magic number does not match.\n");
    return -1;
  }
  */
  curr_block_num = s_block->u.hdr.root;
  while (curr_dir_name[0] != '\0'){
    disk_read_block(my_disk, curr_block_num, curr_block);
    semaphore_P(block_array[curr_block_num]->block_sem);
    i_block = (inode_block*)curr_block;
    i_block--;
    i_block++;
  }
  
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
  int block_num;
  inode_block* inode;

  semaphore_P(disk_op_lock);
  block_num = minifile_get_block_from_path(path);
  if (block_num == -1) {
    semaphore_V(disk_op_lock);
    return NULL;
  } 
  inode = (inode_block*)calloc(1, sizeof(inode_block));
  disk_read_block(my_disk, block_num, (char*)inode);
  semaphore_P(block_array[block_num]->block_sem);
  semaphore_V(disk_op_lock);
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
  super->u.hdr.free_dblock_hd = DATA_START; 
  super->u.hdr.free_dblock_tl = BLOCK_COUNT - 1; 
  super->u.hdr.root = 1;
  
  disk_write_block(my_disk, 0, (char*)super);
  semaphore_P(block_array[0]->block_sem);
 
  // root 
  inode->u.hdr.status = IN_USE;
  inode->u.hdr.type = DIR_t;
  inode->u.hdr.count = 2; 
  inode->d_ptrs[0] = DATA_START;
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

