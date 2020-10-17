#include "basic_system_info.h"
#include "main.h"

#include <stdio.h>
#include <sys/statvfs.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <stdbool.h>

#define FILE_SYS_BUFFER (50)
#define KB (1000)
#define HEADER_SIZE (15)
#define SR_NO_SIZE (4)

/*
 * This function parses through /proc/mounts
 * and calculates the used and available disk space
 * for every mount filesystem and displays it to the GUI
 * similar to df
 */

double file_systems() {

  FILE *fp;

  if ((fp = fopen("/proc/mounts", "r")) == NULL) {
    return 0.0;
  }

  string file_sys = malloc(sizeof(char) * FILE_SYS_BUFFER);
  assert(file_sys != NULL);

  string url = malloc(sizeof(char) * FILE_SYS_BUFFER);
  assert(url != NULL);

  struct statvfs stat;

  double available_disk_space;
  int sr_no = 0;

  while (fscanf(fp, "%s %s %*[^\n]\n", file_sys, url) != EOF) {

    if (statvfs(url, &stat) != 0) {
      return available_disk_space;
    }
    long block_size = stat.f_bsize;
    long blocks = stat.f_blocks;
    long disk_size = blocks * block_size;
    long free_blocks = stat.f_bfree;
    long free_space = free_blocks * block_size;
    long used = disk_size - free_space;
    double percentage_use;
    if (disk_size == 0) {
      continue;
    }
    else {
      percentage_use = ((double)used / disk_size) * 100;
    }
    sr_no++;
    char disk_size_str[HEADER_SIZE];
    sprintf(disk_size_str, "%ld", disk_size/KB);

    char used_str[HEADER_SIZE];
    sprintf(used_str, "%ld", used/KB);

    char free_space_str[HEADER_SIZE];
    sprintf(free_space_str, "%ld", free_space/KB);

    char percentage_used_str[HEADER_SIZE];
    sprintf(percentage_used_str, "%.3f", percentage_use);

    char sr_no_str[SR_NO_SIZE];
    sprintf(sr_no_str, "%d", sr_no);

    add_file_system(sr_no_str, file_sys, disk_size_str, used_str, free_space_str, percentage_used_str, url);

    available_disk_space += free_space;
  }

  free(url);
  free(file_sys);

  url = NULL;
  file_sys = NULL;

  fclose(fp);
  fp = NULL;

  return available_disk_space;
}
