/* filetest8.c
 * three threads doing unrelated things
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

int C(int* arg) {
  minifile_t file;

  file = minifile_creat("foo.txt");
  if (!file) {
    printf("C: minifile_creat failed\n");
    return -1;
  }

  if (minifile_close(file) == -1) {
    printf("C: minifile_close failed\n");
    return -1;
  }

  if (minifile_unlink("foo.txt") == -1) {
    printf("C: minifile_unlink failed\n");
    return -1;
  }

  printf("C: exiting\n");
  return 0;
}

int B(int* arg) {
  int i;
  char name[257]; 

  minithread_fork(B, NULL);

  for (i = 0; i < 10; i++) {
    sprintf(name, "%d", i);
    if (minifile_mkdir(name) == -1) {
      printf("B: mkdir failed on %dth entry. abort!\n", i);
      return -1;
    }
    if (minifile_cd(name) == -1) {
      printf("B: cd failed on %dth entry. abort!\n", i);
      return -1;
    }
  }

  for (i = 9; i >= 0; i--) {
    sprintf(name, "%d", i);
    if (minifile_cd("..") == -1) {
      printf("B: cd failed on %dth entry. abort!\n", i);
      return -1;
    }
    if (minifile_rmdir(name) == -1) {
      printf("B: rmdir failed on %dth entry. abort!\n", i);
      return -1;
    }
  }
  printf("B: exiting\n");
  return 0;
}

int A(int* arg) {
  char** file_list;

  minithread_fork(B, NULL);
  file_list = minifile_ls("/");
  if (!file_list) {
    printf("A: ls failed. abort!\n");
    return -1;
  }
  

  assert(list_count(file_list) == 2);
  assert(!strcmp(file_list[0], "."));
  free(file_list[0]);
  assert(!strcmp(file_list[1], ".."));
  free(file_list[1]);
  free(file_list);

  printf("A: root test passed\n");

  if (minifile_mkdir("A") == -1) {
    printf("A: mkdir failed. abort!\n");
    return -1;
  }

  file_list = minifile_ls("/");
  if (!file_list) {
    printf("A: ls failed. abort!\n");
    return -1;
  }
  
  assert(list_count(file_list) == 3);
  assert(!strcmp(file_list[0], "."));
  free(file_list[0]);
  assert(!strcmp(file_list[1], ".."));
  free(file_list[1]);
  assert(!strcmp(file_list[2], "A"));
  free(file_list[2]);
  free(file_list);

  printf("A: exiting\n");
  return 0;
}

int test(int* arg) {
  minifile_t file;
  char** file_list;
  char* data;
  char buff[10];

  data = "hello"; 

  file = minifile_creat("foo.txt");
  if (!file) {
    printf("minifile_creat failed\n");
    return -1;
  }

  file_list = minifile_ls("/foo.txt");
  if (!file_list) {
    printf("ls failed\n");
    return -1;
  }

  if (!strcmp(file_list[0], "foo.txt")) {
    printf("ls failed\n");
    return -1;
  }

  if (minifile_write(file, data, 6) == -1) {
    printf("write failed\n");
    return -1;
  }
 
  if (minifile_close(file) == -1) {
    printf("minifile_close failed\n");
    return -1;
  }

  file = minifile_open("foo.txt", "r");
  if (!file) {
    printf("minifile_open failed\n");
    return -1;
  }
    
  if (minifile_read(file, buff, 6) == -1) {
    printf("minifile_read failed\n");
    return -1;
  }

  if (strcmp(buff, data)) {
    printf("minifile_read failed\n");
    return -1;
  }

  if (minifile_close(file) == -1) {
    printf("minifile_close failed\n");
    return -1;
  }

  if (minifile_unlink("foo.txt") == -1) {
    printf("minifile_unlink failed\n");
    return -1;
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
  disk_size = 100;
  
  system("rm MINIFILESYSTEM");

  minithread_system_initialize(init, NULL);
  return 0; 
}
