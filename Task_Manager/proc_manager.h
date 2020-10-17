#include <stdio.h>
#include <sys/types.h>

typedef struct process {
  char* name;
  char* stat;
  int pid;
  int ppid;
  int tgid;
  int uid;
  int flag;
  double cpu_usage;
} process_t;


int get_header_num();
process_t** get_headers();
void free_all();
void print_process(process_t*);
int is_proc(char*);
void free_proc(process_t*);
int list(char*);
void init_headers();
process_t* find_proc(pid_t);
int kill_by_pid(pid_t);
int stop_by_pid(pid_t);
int term_by_pid(pid_t);
int cont_by_pid(pid_t);
