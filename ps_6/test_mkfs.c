#include "minithread.h"
#include "synch.h"
#include "minifile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int use_existing_disk;
int disk_flags;
int disk_size;

int test(int* arg) {
  minifile_test_make_fs();
  return 0;
}

int main(int argc, char** argv) {
  use_existing_disk = 1;
  disk_name = "MINIFILESYSTEM";
  
  minithread_system_initialize(test, NULL);
  return 0; 
}
