#define ALL_PROCESSES (0)
#define MY_PROCESSES (1)
#define LIST_VIEW (0)
#define TREE_VIEW (1)

#define ZOOM_X (100.0)
#define ZOOM_Y (187.0)

enum {
  NAME = 0,
  STATUS = 1,
  CPU = 2,
  PID = 3,
  MEMORY = 4
};

enum {
  DEVICE = 0,
  DIRECTORY = 1,
  TYPE = 2,
  TOTAL = 3,
  FREE = 4,
  AVAILABLE = 5,
  USED = 6
};


void add_file_system(char *, char *, char *, char *, char *, char *, char *);
