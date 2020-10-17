#ifndef PROC_MANAGER_H
#define PROC_MANAGER_H

#include "proc_manager.h"
#include "proc_detail.h"

#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

process_t** g_proc_headers = NULL;
int g_max_headers = 4096;
int g_header_num = 0;

process_t** get_headers() {
  return g_proc_headers;
}

int get_header_num() {
  return g_header_num;
}



/*
 * Free the proc structure
 */

void free_proc(process_t *proc) {
  if (!proc) {
    return;
  }
  free(proc->name);
  proc->name = NULL;
  free(proc->stat);
  proc->stat = NULL;
  free(proc);
  proc = NULL;
} /* free_proc() */

/* 
 * free a process group
 */

void free_all() {
  for (int i = 0; i < g_header_num; i++) {
    process_t *head = g_proc_headers[i];    
    free(head->name);
    head->name = NULL;
    free(head->stat);
    head->stat = NULL;
    free(head); 
  }
  free(g_proc_headers);
  g_proc_headers = NULL;
  g_max_headers = 4096;
  g_header_num = 0;
} /* free_proc_group() */

/*
 * init the process headers array
 */

void init_headers() {
  if (g_proc_headers) {
    free_all();
    g_header_num = 0;
    g_max_headers = 4096;
  }
  else {
    g_proc_headers = malloc(sizeof(process_t*) * g_max_headers);
  }
} /* init_headers() */

process_t* create_process() {
  process_t* proc = malloc(sizeof(process_t));
  proc->name = NULL;
  proc->stat = NULL;
  proc->pid = - 1;
  proc->ppid = - 1;
  proc->tgid = - 1;
  proc->uid = - 1;
  proc->flag = 0;
  proc->cpu_usage = 0.0;
  return proc;
}

/*
 * set cpu usage to a process
 */

void set_cpu_usage(process_t *proc) {
  double usage = get_cpu_usage(proc->pid);
  if (usage > 0) {
    proc->cpu_usage = usage;
  }
} /* set_cpu_usage() */

/*
 * add a process to linked list
 */

void add_proc(process_t* proc) {
  if (g_header_num == g_max_headers) {
    g_max_headers *= 2;
    g_proc_headers = realloc(g_proc_headers, sizeof(process_t*) * g_max_headers);
  }
  g_proc_headers[g_header_num++] = proc; 
} /* add_proc() */

/*
 *Check if the dir name is number
 */

int is_proc(char* d_name) {
  if (d_name == NULL) {
    return -1;
  }
  for (int i = 0; i < strlen(d_name); i++) {
    if (!isdigit((int) d_name[i])) {
      return 0;
    }
  }
  return 1;
} /* is_proc() */

/*
 * Print proc Info
 */

void print_process(process_t* proc) {
  if (proc == NULL) {
    printf("proc is NULL\n");
    return;
  }
  printf("P:%d ", proc->pid);
  printf("PP:%d ", proc->ppid);
  printf("T:%d ", proc->tgid);
  printf("U:%d ", proc->uid);
  printf("%s ", proc->stat);
  printf("%s\n", proc->name);
} /* print_process() */

/*
 * open the pid directory and read the proc information
 */

int read_proc_info(char* dir_name, process_t *proc) {
  if ((!dir_name) || (!proc)) {
    return 0;
  }
  char filename[1024] = {0};
  sprintf(filename, "/proc/%s/status", dir_name);
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    printf("fopen failed");
    return -1;
  }
  int len = 2048;
  char line[2048];
  while (fgets(line, len, fp)) {
    //printf("%s", line);
    char* tmp = NULL;
    if ((tmp = strstr(line, "Name:")) != NULL) {
      proc->name = strdup(tmp + 6);
      proc->name[strlen(proc->name) - 1] = '\0';
      //printf("N:%s ", proc->name);
    }
    else if ((tmp = strstr(line, "State:")) != NULL) {
      proc->stat = strdup(tmp + 7);
      proc->stat[strlen(proc->stat) - 1] = '\0';
      //printf("S:%s ", proc->stat);
    }
    else if (!strncmp(line, "Tgid:", 5)) {
      proc->tgid = atoi(line + 6);
      //printf("tgid:%d\n", proc->pid);
    }
    else if (!strncmp(line, "Pid:", 4)) {
      proc->pid = atoi(line + 5);
      //printf("Pid:%d\n", proc->pid);
    }
    else if (!strncmp(line, "PPid:", 5)) {
      proc->ppid = atoi(line + 6);
      //printf("PPid:%d\n", proc->pid);
    }
    else if (!strncmp(line, "Uid:", 4)) {
      int i = 0;
      for (i = 0; i < strlen(line); i++) {
        if (line[i] == ' ') {
          break;
        }
      }
      char tmp[i - 4];
      strncpy(tmp, line + 5, i - 4);
      tmp[i - 5] = '\0';
      proc->uid = atoi(tmp);
      //printf("U:%d\n", proc->pid);
    }
    else {
    }
  }
  fclose(fp);
  return 1;
} /* read_proc_info() */

/*
 * check if the process is a desired type
 */

int check_type(process_t* proc, char* type) {
  if (!strcmp(type, "A")) {
    if (proc->ppid > 2) {
      return 1;
    }
  }
  else if (!strcmp(type, "R")) {
    if (strstr(proc->stat, "R")) {
      return 1;
    }
  }
  else {
    int uid = (int) getuid();
    if (uid == proc->uid) {
      return 1;
    }
  }
  return 0;
}

/*
 * open /proc and list process
 */

int list(char* type) {
  DIR *proc_dir = NULL;
  struct dirent *dir = NULL;
  proc_dir = opendir("/proc");
  if (!proc_dir) {
    perror("opendir() failed");
    return -1;
  }

  while ((dir = readdir(proc_dir)) != NULL) {
    if (is_proc(dir->d_name)) {
      process_t *process = create_process();
      read_proc_info(dir->d_name, process);
      if (check_type(process, type)) {
        add_proc(process);
        set_cpu_usage(process);
      }
      else {
        free_proc(process);
      }
    }
  }
  closedir(proc_dir);
  return 0;
} /* list_all() */

/* 
 * find a process by pid, return NULL if not found
 */

process_t* find_proc(pid_t pid) {
  DIR *proc_dir = NULL;
  struct dirent *dir = NULL;
  proc_dir = opendir("/proc");
  if (!proc_dir) {
    perror("opendir() failed\n");
    return NULL;
  }
  process_t *process = NULL;
  while ((dir = readdir(proc_dir)) != NULL) {
    if (is_proc(dir->d_name)) {
      int tmp_pid = atoi(dir->d_name);
      if ((int) pid == tmp_pid) {
        process = create_process();
        read_proc_info(dir->d_name, process);
        break;
      }
    }
  }
  closedir(proc_dir);

  return process;
} /* find_proc() */

/*
 * kill a process by pid
 */

int kill_by_pid(pid_t pid) {
  if (pid < 1) {
    printf("invalid pid\n");
    return -1;
  }
  process_t* process = find_proc(pid);
  if (process) {
    printf("Killing %d %s\n", (int) pid, process->name);
    kill(pid, SIGKILL);
    free_proc(process);
    return 0;
  }
  printf("Process not found");
  return -1;
} /* kill_by_pid() */

/*
 * terminate a process by pid
 */

int term_by_pid(pid_t pid) {
  if (pid < 1) {
    printf("invalid pid\n");
    return -1;
  }
  process_t* process = find_proc(pid);
  if (process) {
    printf("Terminating %d %s\n", (int) pid, process->name);
    kill(pid, SIGTERM);
    free_proc(process);
    return 0;
  }
  printf("Process not found");
  return -1;
} /* term_by_pid() */

/*
 * Stop a process by pid
 */

int stop_by_pid(pid_t pid) {
  if (pid < 1) {
    printf("invalid pid\n");
    return -1;
  }
  process_t* process = find_proc(pid);
  if (process) {
    printf("Stopping %d %s\n", (int) pid, process->name);
    kill(pid, SIGSTOP);
    free_proc(process);
    return 0;
  }
  printf("Process not found");
  return -1;
} /* kill_by_pid() */

/*
 * Continue a process by pid
 */

int cont_by_pid(pid_t pid) {
  if (pid < 1) {
    printf("invalid pid\n");
    return -1;
  }
  process_t* process = find_proc(pid);
  if (process) {
    printf("Continue %d %s\n", (int) pid, process->name);
    kill(pid, SIGCONT);
    free_proc(process);
    return 0;
  }
  printf("Process not found");
  return -1;
} /* kill_by_pid() */

#endif
