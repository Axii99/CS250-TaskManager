#include <pthread.h>
#include <semaphore.h>
#include <time.h>


double cpu();
double* swap_mem_usage();
void* cpu_usage(void *);
long* network_usage();
