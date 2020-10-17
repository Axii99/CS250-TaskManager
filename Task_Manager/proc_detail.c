#include "proc_detail.h"


#include "proc_manager.h"
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

#define MAX (4096)

/*
 * list open files
 */

char* lsof(pid_t pid) {
  if (pid < 1) {
    printf("invalid pid\n");
    return NULL;
  }

  process_t *proc = find_proc(pid);
  char* open_files = NULL;
  if (proc) {
    open_files = malloc(sizeof(char) * MAX * 10);
    strcpy(open_files, "");
    char path[1024];
    sprintf(path, "/proc/%d/fd/", proc->pid);
    DIR *fd_dir = NULL;
    fd_dir = opendir(path);
    if (!fd_dir) {
      printf("Permission Denied\n");
      return NULL; 
    }
    struct dirent *dir = NULL;
    while ((dir = readdir(fd_dir)) != NULL) {
      if (isdigit(dir->d_name[0])) {
        char link[1024] = {0};
        char type[20] = "file";
        sprintf(path, "/proc/%d/fd/%s", proc->pid, dir->d_name);
        readlink(path, link, 2048);
        char* p = NULL;
        p = strstr(link, ":");
        if (p != NULL) {
          strncpy(type, link, (int) (p - link));
          type[(int) (p - link)] = '\0';
        }
        char tmp[1024] = {0};
        sprintf(tmp, "%s  %s  %s\n", dir->d_name, type, link);
        strcat(open_files, tmp); 
      }
    }
  }
  //printf("%s", open_files);
  free_proc(proc);
  return open_files;
} /* lsof() */

/*
 * open smaps file and find the total memory usuage
 */

double get_mem_usage(pid_t pid) {
  if (pid < 0) {
    printf("invalid pid\n");
    return 0;
  }

  char path[1024];
  sprintf(path, "/proc/%d/smaps", (int) pid);
  FILE* fp = fopen(path, "rb");
  if (fp == NULL) {
    //printf("Permission Denied\n");
    return 0;
  }
  char buf[1024];
  double total_mem = 0;
  strcpy(buf, "test");
  while (fgets(buf, 1024, fp) != NULL) {
    int mem = 0;
    if (!strncmp(buf, "Size:", 5)) {
      sscanf(buf, "Size: %d kB", &mem);
    }
    total_mem += mem;
  }
  fclose(fp);
  return total_mem / 1024.0; 
} /* get_mem_usage() */

/*
 * get maps
 */

char* get_map(pid_t pid) {
  if (pid < 0) {
    printf("invalid pid\n");
    return NULL;
  }

  char path[1024];
  sprintf(path, "/proc/%d/smaps", (int) pid);
  FILE* fp = fopen(path, "rb");
  if (fp == NULL) {
    printf("Permission Denied\n");
    return NULL;
  }
  char buf[1024];
  char* maps = malloc(sizeof(char) * 30 * MAX);
  strcpy(maps, "");
  while (fgets(buf, 1024, fp) != NULL) {

    if (!strncmp(buf, "VmFlags:", 8)) {
      continue;
    }
    char vmstart[20];
    char vmend[20];
    char vmsize[20];
    char flags[16];
    char offset[20];
    char dev[16];
    char inode[16];
    char filename[1024];
    char sclean[16];
    char sdirty[16];
    char pclean[16];
    char pdirty[16];
    int check = 0;
    if (strlen(buf) > 75) {
      sscanf(buf, "%[^-]-%s %s %s %s %s %s", vmstart, vmend, flags,
            offset, dev, inode, filename);
    }
    else if (strlen(buf) > 30) {
      sscanf(buf, "%[^-]-%s %s %s %s %s", vmstart, vmend, flags,
            offset, dev, inode);
      strcpy(filename, "Unknown");
      //printf("%s\n", filename);
    }
    else {
      if (!strncmp(buf, "Size:", 5)) {
        sscanf(buf, "Size: %s kB", vmsize);
      }
      else if (!strncmp(buf, "Shared_Clean:", 13)) {
        sscanf(buf, "Shared_Clean: %s kB", sclean);
      }
      else if (!strncmp(buf, "Shared_Dirty:", 13)) {
        sscanf(buf, "Shared_Dirty: %s kB", sdirty);
      }
      else if (!strncmp(buf, "Private_Clean:", 14)) {
        sscanf(buf, "Private_Clean: %s kB", pclean);
      }
      else if (!strncmp(buf, "Private_Dirty:", 14)) {
        sscanf(buf, "Private_Dirty: %s kB", pdirty);
        check = 1;
      }
    }
    if (check) {
      char tmp[2048];
      sprintf(tmp, "%s %s %s %s %s %s %s %s %s %s\n",
              filename, vmstart, vmend, vmsize, flags, offset, pclean, pdirty,
              sclean, sdirty);
      strcat(maps, tmp);
    }
  
  }
  fclose(fp);
  return maps; 
} /* get_map() */

/*
 * get cpu usage
 */

double get_cpu_usage(pid_t pid) {
  if (pid < 0) {
    printf("invalid pid\n");
    return 0;
  }

  char path[1024];
  sprintf(path, "/proc/%d/stat", (int) pid);
  FILE* pstat = fopen(path, "rb");
  FILE* sstat = fopen("/proc/stat", "rb");
  if (pstat == NULL) {
    //fclose(pstat);
    return -1;
  }
  if (sstat == NULL) {
    fclose(sstat);
    return -1;
  }

  unsigned long long cpu_time[10];
  unsigned long long total_cpu1 = 0;
  int ret = fscanf(sstat, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        &cpu_time[0], &cpu_time[1], &cpu_time[2], &cpu_time[3], &cpu_time[4],
        &cpu_time[5], &cpu_time[6], &cpu_time[7], &cpu_time[8], &cpu_time[9]);
  if (ret == 10) {
    for (int i = 0; i < 4; i ++) {
      total_cpu1 += cpu_time[i];
    }
  }
  fclose(sstat);
  unsigned long long utime = 0;
  unsigned long long stime = 0;
  unsigned long long cutime = 0;
  unsigned long long cstime = 0;
  unsigned long long total_proc1 = 0;
  char tmp[2048];
  fgets(tmp, 2048, pstat);
  ret = sscanf(tmp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u"
                      "%*u %*u %llu %llu %llu %llu %*[^\n]",
                      &utime, &stime, &cutime, &cstime);
  if (ret == 4) {
    total_proc1 = utime + stime + cutime + cstime;
  }
  fclose(pstat);

  usleep(4000);
  
  pstat = fopen(path, "rb");
  sstat = fopen("/proc/stat", "rb");
  if (pstat == NULL) {
    return -1;
  }
  if (sstat == NULL) {
    fclose(sstat);
    return -1;
  }


  unsigned long long total_cpu2 = 0;
  ret = fscanf(sstat, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        &cpu_time[0], &cpu_time[1], &cpu_time[2], &cpu_time[3], &cpu_time[4],
        &cpu_time[5], &cpu_time[6], &cpu_time[7], &cpu_time[8], &cpu_time[9]);
  if (ret == 10) {
    for (int i = 0; i < 4; i ++) {
      total_cpu2 += cpu_time[i];
    }
  }

  unsigned long long total_proc2 = 0;
  char tmp2[2048];
  fgets(tmp2, 2048, pstat);
  ret = sscanf(tmp2, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u"
                      "%*u %*u %llu %llu %llu %llu %*[^\n]",
                      &utime, &stime, &cutime, &cstime);
  if (ret == 4) {
    total_proc2 = utime + stime + cutime + cstime;
  }

  double percent = 0.0;
  percent = 100 *
    ((double)(total_proc2 - total_proc1) / (double)(total_cpu2 - total_cpu1));
  fclose(pstat);
  fclose(sstat);
  return percent;
} /* get_cpu_usage() */

/* 
 * Details view
 */

char* get_details (pid_t pid) {
  char path [128];
  sprintf(path, "/proc/%d/status", pid);
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    printf("fopen failed\n");
    return NULL;
  }

  int len = 1024;
  char line[1024];

  char name[1024];
  char user[1024];
  char state[256];
  char vm[32];
  char rm[32];

  char* result = malloc(sizeof(char) * 2 * MAX);
  while (fgets(line, len, fp)) {
    if (strstr(line, "Name:")) {
      sscanf(line, "Name: %s\n", name);
      //printf("name: %s\n", name);
    }
    else if (strstr(line, "State")) {
      char tmp1[16];
      char tmp2[16];
      sscanf(line, "State: %s %s\n", tmp1, tmp2);
      sprintf(state, "%s-%s", tmp1, tmp2);
    }
    else if (strstr(line, "Uid")) {
      int uid = 0;
      sscanf(line, "Uid: %d %*[^\n]\n", &uid);
      struct passwd *pws = {0};
      pws = getpwuid(uid);
      if (pws) {
        strcpy(user, pws->pw_name);
      }
      else {
        strcpy(user, "Unknown");
      }
      //printf("user: %s\n", user);
    }
    else if (strstr(line, "VmSize")) {
      double vmsize = 0;
      sscanf(line, "VmSize: %lf kB\n", &vmsize);
      sprintf(vm, "%.2fMB", (vmsize / 1024.0));
      //printf("vm: %s\n", vm);
    }
    else if (strstr(line, "VmRSS")) {
      double rmsize = 0;
      sscanf(line, "VmRSS: %lf kB\n", &rmsize);
      sprintf(rm, "%.2fMB", (rmsize / 1024.0));
      //printf("rm: %s\n", rm);
      break;
    }
  }
  fclose(fp);
  fp = NULL;

  sprintf(path, "/proc/%d/smaps", pid);
  fp = fopen(path, "rb");
  if (fp == NULL) {
    printf("fopen failed\n");
    return NULL;
  }

  double shared_mem = 0.0;
  while (fgets(line, len, fp)) {
    if (strstr(line, "Shared_Clean")) {
      double tmp = 0.0;
      sscanf(line, "Shared_Clean: %lf kB\n", &tmp);
      shared_mem += (tmp / 1024.0);
    }
    else if (strstr(line, "Shared_Dirty")) {
      double tmp = 0.0;
      sscanf(line, "Shared_Dirty: %lf kB\n", &tmp);
      shared_mem += (tmp / 1024.0);
    }
  }
  char sm[32];
  sprintf(sm, "%.2fMB", shared_mem);
  //printf("shared mem: %s\n", sm);
  fclose(fp);
  fp = NULL;

  char cpu_time[64]; 
  sprintf(path, "/proc/%d/stat", pid);
  fp = fopen(path, "rb");
  if (fp == NULL) {
    printf("fopen failed\n");
    return NULL;
  }
  fgets(line, len, fp);
  unsigned long long total_time = 0;
  unsigned long long utime = 0;
  unsigned long long stime = 0;
  unsigned long long cutime = 0;
  unsigned long long cstime = 0;
  sscanf(line, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u"
                      "%*u %*u %llu %llu %llu %llu %*[^\n]",
                      &utime, &stime, &cutime, &cstime);
  total_time = utime + stime + cutime + cstime;
  double seconds = (double) total_time / (double) sysconf(_SC_CLK_TCK);
  sprintf(cpu_time, "%f(s)", seconds);
  fclose(fp);
  fp = NULL;

  struct stat st = {0};
  char date[36];
  sprintf(path, "/proc/%d", pid);
  int ret = stat(path, &st);
  if (!ret) {
    strftime(date, 36, "%d.%m.%Y-%H:%M:%S", localtime(&st.st_ctime));
  }
  else {
    strcpy(date, "Unknown");
  }

  double mem = get_mem_usage(pid);
  char memory[32];
  strcpy(memory, "Unknown");
  sprintf(memory, "%.2fMB", mem);

  sprintf(result, "%s %s %s %s %s %s %s %s %s\n",
            name, user, state, memory, vm, rm, sm, cpu_time, date);
  printf("%s", result); 
  return result;
} /* get_details() */
