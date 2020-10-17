#include "usage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

pthread_mutex_t m = {0};
sem_t wait_sem = {0};

#define PERCENT_CALCULATION (10000)
#define PERCENT (100)
#define BUF_SIZE (20)
#define URL_SIZE (30)

/*
 * This function creates and dispathes threads for the cpu_usage()
 * function and ultimately calculates the percentage of cpu usage
 */

double cpu() {

  pthread_t t1;
  pthread_t t2;
  pthread_mutex_init(&m, NULL);
  sem_init(&wait_sem, 0, 2);

  long *init = malloc(sizeof(double) * 2);
  assert(init != NULL);

  long *post = malloc(sizeof(long) * 2);
  assert(post != NULL);

  pthread_create(&t1, NULL, cpu_usage, (void *) &init);
  pthread_create(&t2, NULL, cpu_usage, (void *) &post);

  
  
  long total1 = init[0];
  long w_total1 = init[1];
  long total2 = post[0];
  long w_total2 = post[1];

  int total_over_period = abs(total2 - total1);
  int work_over_period = abs(w_total2 - w_total1);
  double cpu;

  if ((work_over_period == 0) || (total_over_period == 0)) {
    cpu = 0;
  }
  else {
    cpu = work_over_period / ((double) total_over_period);
  }
  cpu = cpu * PERCENT_CALCULATION * sysconf(_SC_NPROCESSORS_ONLN);
  if (cpu > PERCENT) {
    cpu = PERCENT;
  }
  if (cpu == 0) {
    cpu++;
  }

  pthread_join(t1, NULL);
  pthread_join(t2, NULL); 
  free(init);
  free(post);
  init = NULL;
  post = NULL;
  pthread_mutex_destroy(&m);
  return cpu;
}

/*
 * This function parses through /proc/stat and retrieves information needed
 * to calculate work cpu time and total cpu time
 */

void *cpu_usage(void *use) {

  sem_wait(&wait_sem);
  pthread_mutex_lock(&m);
  FILE *fp;
  
  if ((fp = fopen("/proc/stat", "r")) == NULL) {
    return 0;
  }

  long **address = (long **) use;
  int user = 0;
  int nice = 0;
  int system = 0;
  int idle = 0;
  int iowait = 0;
  int irq = 0;
  int softirq = 0;
  int steal = 0;
  int guest = 0;
  int guest_nice = 0;

  int total = 0;
  int work_total = 0;


  fscanf(fp, "cpu  %d %d %d %d %d %d %d %d %d %d\n", 
              &user, &nice, &system, &idle, &iowait, &irq, &softirq,
              &steal, &guest, &guest_nice);

  total = user + nice + system + idle + iowait + irq + softirq + steal +
           guest + guest_nice;

  work_total = user + system + nice + irq + softirq + steal + guest + guest_nice;
  (*address)[0] = total;
  
  (*address)[1] = work_total;
  use = address;
  fclose(fp);
  fp = NULL;

  usleep(1000); 
  pthread_mutex_unlock(&m);
  sem_post(&wait_sem);
  pthread_exit(NULL);

}

/* 
 * This function calculates the swap usage and memory usage percentages
 * by parsing through /proc/swaps and /proc/meminfo
 */

double *swap_mem_usage() {

  FILE *fp1;

  if ((fp1 = fopen("/proc/swaps", "r")) == NULL) {
    return NULL;
  }

  fscanf(fp1, "%*[^\n]\n");

  char url[URL_SIZE];
  char type[BUF_SIZE];
  long size = 0;
  long used = 0;
  int priority = 0;

  fscanf(fp1, "%s %s %ld %ld %d\n", url, type, &size, &used, &priority);

  double swp_percent = ((double) used / (double) size) * PERCENT;

  FILE *fp2;

  if ((fp2 = fopen("/proc/meminfo", "r")) == NULL) {
    return NULL;
  }

  char buf1[BUF_SIZE];
  char buf2[BUF_SIZE];

  long total_mem = 0;
  long free_mem = 0;

  fscanf(fp2, "%s %ld kB\n", buf1, &total_mem);
  fscanf(fp2, "%s %ld kB\n", buf2, &free_mem);

  double mem_percent = (((double) (total_mem - free_mem)) / 
                        ((double) total_mem)) * PERCENT;

  double *percentages = malloc(sizeof(double) * 2);
  assert(percentages != NULL);
  percentages[0] = swp_percent;
  percentages[1] = mem_percent;

  fclose(fp2);
  fp2 = NULL;
  fclose(fp1);
  fp1 = NULL;
  return percentages;
}

/*
 * This function calculates the network usage percentage by parsing through
 * the information located in /proc/net/dev
 */

long *network_usage() {

  FILE *fp;
  long *network_use = malloc(sizeof(long) * 2);
  assert(network_use != NULL);

  if ((fp = fopen("/proc/net/dev", "r")) == NULL) {
    return network_use;
  }

  fscanf(fp, "%*[^\n]\n"); 
  fscanf(fp, "%*[^\n]\n");

  long num_bytes_receive = 0;
  long num_bytes_transmit = 0;

  long total_received = 0;
  long total_transmit = 0;

  while ((fscanf(fp, " %*s %ld %*d %*d %*d %*d %*d %*d %*d %ld %*d %*d %*d %*d %*d %*d %*d\n",
                      &num_bytes_receive, &num_bytes_transmit)) != EOF) {

    total_received += num_bytes_receive;
    total_transmit += num_bytes_transmit;

  }

  network_use[0] = total_received;
  network_use[1] = total_transmit;

  fclose(fp);
  fp = NULL;
  return network_use;
}
