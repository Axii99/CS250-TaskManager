#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


char* lsof(pid_t);
double get_mem_usage(pid_t);
char* get_map(pid_t);
double get_cpu_usage(pid_t);
char* get_details(pid_t);
