/* filetest7.c
 * test different modes for the opening the file.
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

int test(int* arg) {
  minifile_t file;
  char* data1;
  char* data2;
  char buff[20];

  data1 = "data1"; 
  data2 = "data2"; 

  file = minifile_creat("foo.txt");
  if (!file) {
    printf("minifile_creat failed\n");
    return -1;
  }

  // mode should be read+write
  if (minifile_write(file, data1, 6) == -1) {
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

  if (strcmp(buff, data1)) {
    printf("minifile_read failed\n");
    return -1;
  }

  if (minifile_close(file) == -1) {
    printf("minifile_close failed\n");
    return -1;
  }

  file = minifile_open("foo.txt", "w");
  if (!file) {
    printf("minifile_open failed\n");
    return -1;
  }

  if (minifile_close(file) == -1) {
    printf("minifile_close failed\n");
    return -1;
  }

  if (minifile_stat("/") != -2) {
    printf("stat didn't return -2 for root dir\n");
    return -1;
  }

  if (minifile_stat("foo.txt")) {
    printf("stat returned non-zero byte len for truncated file\n");
    return -1;
  }

  if (minifile_stat("bar.txt") != -1) {
    printf("stat didn't return -1 for non-existing file\n");
    return -1;
  }  

  file = minifile_open("foo.txt", "w");
  if (!file) {
    printf("minifile_open failed\n");
    return -1;
  }

  if (minifile_write(file, data1, 6) == -1) {
    printf("write failed\n");
    return -1;
  }
 
  if (minifile_close(file) == -1) {
    printf("minifile_close failed\n");
    return -1;
  }

  file = minifile_open("foo.txt", "a");
  if (!file) {
    printf("minifile_open failed\n");
    return -1;
  }

  if (minifile_write(file, data2, 6) == -1) {
    printf("write failed\n");
    return -1;
  }

  if (minifile_close(file) == -1) {
    printf("minifile_close failed\n");
    return -1;
  }

  if (minifile_stat("foo.txt") != 12) {
    printf("12 bytes written to foo.txt, but stat returned %d\n", minifile_stat("foo.txt"));
    return -1;
  }  
  
  file = minifile_open("foo.txt", "r");
  if (!file) {
    printf("minifile_open failed\n");
    return -1;
  }
    
  if (minifile_read(file, buff, 12) == -1) {
    printf("minifile_read failed\n");
    return -1;
  }

  if (strcmp(buff, data1)) {
    printf("minifile_read failed\n");
    return -1;
  }

  
  if (strcmp(buff+6, data2)) {
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

  if (minifile_stat("foo.txt") != -1) {
    printf("stat didn't return -1 for non-existing file\n");
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
