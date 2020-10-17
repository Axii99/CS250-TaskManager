#include "basic_system_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <sys/statvfs.h>

#define STR_LENGTH (30)
#define LIMIT (5)
#define MB (1000000)
#define GB (1000000000)

/*
 * This function extracts the kernel version info from
 *  /proc/version and returns said string
 */

string kernel_version() {

  FILE *fp;

  if ((fp = fopen("/proc/version", "r")) == NULL) {
    return "";
  }

  string kernel_version = malloc(sizeof(char) * STR_LENGTH);
  assert(kernel_version != NULL);

  fscanf(fp, "%[^(\n]\n", kernel_version);

  fclose(fp);
  fp = NULL;
  return kernel_version;
}

/*
 * This function extracts the processor version info from
 *  /proc/cpuinfo and returns said string
 */

string processor_version() {

  FILE *fp;

  if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
    return "";
  }

  string processor_version;
  processor_version = malloc(sizeof(char) * 2 * STR_LENGTH);
  assert(processor_version != NULL);

  int count = 1;
  while ((fscanf(fp, "%[^\n]\n", processor_version)) != EOF) {
    if (count == LIMIT) {
      break;
    }
    count++;
  }

  int len = strlen(processor_version);
  int index = 0;

  for (int i = 0; i < len; i++) {
    if (processor_version[i] == ':') {
      index = i;
      break;      
    }
  }

  //setting index to next word
  index = index + 2;
  int new_len = len - (index - 1);
  string proc_ver = malloc(sizeof(char) * new_len);
  assert(proc_ver != NULL);

  count = 0;

  for (int i = index; i < len; i++) {
    proc_ver[count] = processor_version[i];
    count++;
  }
  free(processor_version);
  processor_version = NULL;
  fclose(fp);
  fp = NULL;

  return proc_ver;
}

/*
 * This function extracts the memory from /proc/meminfo
 * and returns the memory in giB
 */

double meminfo() {

  FILE *fp;

  if ((fp = fopen("/proc/meminfo", "r")) == NULL) {
    return -1;
  }

  double num = 0;
  fscanf(fp, "%*s %lf\n", &num);

  fclose(fp);
  fp = NULL;

  //memory in Kb, so dividing by MB=1000000 returns result in GB
  return num/MB;

}

/*
 * Calculates total space in GB
 */

double total_space() {
  
  return file_systems()/GB;
}
