#include <cairo.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#include "main.h"
#include "proc_manager.h"
#include "proc_detail.h"
#include "basic_system_info.h"
#include "usage.h"

#include <gtk/gtk.h>

int g_processes_button_state = 0;
int g_tree_button_state = 0;
GtkWidget *g_main_window;
GtkWidget *g_dialog;
GtkMenu *process_popup;
GtkTreeStore *g_process_list;
GtkTreeView *g_process_view;
GtkListStore *g_file_systems;
process_t** g_proc_array;


/*
 * Wrapper function for adding processes
 * Overwrites selection with the new entry (can be used as parent pointer)
 */
 
void add_process(GtkTreeIter *selection, GtkTreeIter *parent, char *name,
                  char* status, double cpu, int pid, char* memory) {
  gtk_tree_store_insert_with_values(g_process_list, selection, parent, -1,
                                    NAME, name,
                                    STATUS, status,
                                    CPU, cpu,
                                    PID, pid,
                                    MEMORY, memory,
                                    -1);
}

/*
 * Wrapper function for adding processes WITH CHILDREN
 */

void add_children(GtkTreeIter* parent, int pid) {
  for (int i = 0; i < get_header_num(); i++) {
    process_t* proc = g_proc_array[i];
    if (proc->flag == 1) {
      continue;
    }
    if (proc->ppid == pid) {
      proc->flag = 1;
      GtkTreeIter *child = malloc(sizeof(GtkTreeIter)); 
      char memory[16];
      sprintf(memory, "%.2f MiB", get_mem_usage(proc->pid));
      add_process(child, parent, proc->name, proc->stat, proc->cpu_usage, proc->pid, memory); 
      add_children(child, proc->pid);
      free(child);
    }
  }
}

/*
 * Wrapper function for adding processes
 */
 
void add_file_system(char *device, char *dir, char *type, char *total, 
                      char *free, char *available, char *used) {
  gtk_list_store_insert_with_values(g_file_systems, NULL, -1, 
                                    DEVICE, device,
                                    DIRECTORY, dir,
                                    TYPE, type,
                                    TOTAL, total,
                                    FREE, free,
                                    AVAILABLE, available,
                                    USED, used,
                                    -1);
}

/*
 * Nearly pointless wrapper function to remind us that this exists
 * Or to make relevant code more readable
 */

void clear_process_list() {
  gtk_tree_store_clear(g_process_list);
}

/*
 * int compare function for sorting
 * will fail if called on a non gint column
 */
 
gint int_compare(GtkTreeModel *model, GtkTreeIter *val1, GtkTreeIter *val2, 
                  gpointer userdata) {
  gint sortcol = GPOINTER_TO_INT(userdata);
  GValue value1 = G_VALUE_INIT;
  gtk_tree_model_get_value((GtkTreeModel *)g_process_list, val1, sortcol, &value1);
  char *pid_text1 = g_strdup_value_contents(&value1);
  int pid1 = atoi(pid_text1);
  free(pid_text1);
  
  GValue value2 = G_VALUE_INIT;
  gtk_tree_model_get_value((GtkTreeModel *)g_process_list, val2, sortcol, &value2);
  char *pid_text2 = g_strdup_value_contents(&value2);
  int pid2 = atoi(pid_text2);
  free(pid_text2);
  
  return pid1 - pid2;
}

/*
 * int compare function for sorting
 * will fail if called on a non gint column
 */
 
gint double_compare(GtkTreeModel *model, GtkTreeIter *val1, GtkTreeIter *val2, 
                    gpointer userdata) {
  gint sortcol = GPOINTER_TO_INT(userdata);
  GValue value1 = G_VALUE_INIT;
  gtk_tree_model_get_value((GtkTreeModel *)g_process_list, val1, sortcol, &value1);
  char *text1 = g_strdup_value_contents(&value1);
  double percent1 = atof(text1);
  free(text1);
  
  GValue value2 = G_VALUE_INIT;
  gtk_tree_model_get_value((GtkTreeModel *)g_process_list, val2, sortcol, &value2);
  char *text2 = g_strdup_value_contents(&value2);
  double percent2 = atof(text2);
  free(text2);
  
  int ret = (int) percent1 - percent2;
  if (ret == 0) {
    return (percent1 - percent2 > 0) ? 1 : -1;
  }
  
  return ret;
}

/*
 * str compare function for sorting
 */
 
gint str_compare(GtkTreeModel *model, GtkTreeIter *val1, GtkTreeIter *val2, 
                  gpointer userdata) {
  gint sortcol = GPOINTER_TO_INT(userdata);
  GValue value1 = G_VALUE_INIT;
  gtk_tree_model_get_value((GtkTreeModel *)g_process_list, val1, sortcol, &value1);
  char *text1 = g_strdup_value_contents(&value1);
  
  GValue value2 = G_VALUE_INIT;
  gtk_tree_model_get_value((GtkTreeModel *)g_process_list, val2, sortcol, &value2);
  char *text2 = g_strdup_value_contents(&value2);
  
  gint ret = strcmp(text1, text2);
  
  free(text1);
  free(text2);
  
  return ret;
}

/*
 * Main function
 * Builds GUI from glade file with builder, and fetches pointers for
 * important widgets that need to be tracked or listened to
 * Then runs gtk main
 */


int main(int argc, char *argv[]) {

  gtk_init(&argc, &argv);

  GtkBuilder *builder = gtk_builder_new_from_file("manager.glade");

  g_main_window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
  gtk_builder_connect_signals(builder, NULL);

  //Fetch several interactive elements to link them
  GTK_WIDGET(gtk_builder_get_object(builder, "menubar_monitor_popup_quit"));
  GTK_WIDGET(gtk_builder_get_object(builder, "all_processes_button"));
  GTK_WIDGET(gtk_builder_get_object(builder, "my_processes_button"));
  GTK_WIDGET(gtk_builder_get_object(builder, "tree_format_button"));
  GTK_WIDGET(gtk_builder_get_object(builder, "refresh_button"));
  g_process_view = GTK_TREE_VIEW(gtk_builder_get_object(builder, "process_list_treeview"));
  process_popup = GTK_MENU(gtk_builder_get_object(builder, "menubar_process_popup"));
  
  g_process_list = GTK_TREE_STORE(gtk_builder_get_object(builder, "process_list_treestore"));
  g_file_systems = GTK_LIST_STORE(gtk_builder_get_object(builder, "file_systems_liststore"));
  
  GtkTreeSortable *sortable = GTK_TREE_SORTABLE(g_process_list);
  
  // Setup sort functions for each column
  
  gtk_tree_sortable_set_sort_func(sortable, NAME, str_compare, 
                                  GINT_TO_POINTER(NAME), NULL);
  gtk_tree_sortable_set_sort_func(sortable, STATUS, str_compare,
                                  GINT_TO_POINTER(STATUS), NULL);
  gtk_tree_sortable_set_sort_func(sortable, CPU, double_compare,
                                  GINT_TO_POINTER(CPU), NULL);
  gtk_tree_sortable_set_sort_func(sortable, PID, int_compare,
                                  GINT_TO_POINTER(PID), NULL);
  gtk_tree_sortable_set_sort_func(sortable, MEMORY, str_compare,
                                  GINT_TO_POINTER(MEMORY), NULL);

  gtk_tree_sortable_set_sort_column_id(sortable, PID, GTK_SORT_DESCENDING);

  //sortable = GTK_TREE_SORTABLE(g_file_systems);
  
  
  
  
  //Example populate command for tree and list
  //TODO: remove and populate correctly
  init_headers();
  list("A");
  g_proc_array = get_headers();
  GtkTreeIter *parent = malloc(sizeof(GtkTreeIter));
  for (int i = 0; i < get_header_num(); i++) {
    process_t *proc = g_proc_array[i];
    if (proc->flag == 1) {
      continue;
    }
    char memory[16];
    sprintf(memory, "%.2f MiB", get_mem_usage(proc->pid));
    add_process(parent, NULL, proc->name,
    proc->stat, proc->cpu_usage, proc->pid, memory);
    proc->flag = 1;
    if (g_tree_button_state == TREE_VIEW) {
      add_children(parent, proc->pid);
    }
  }
  free(parent);
  free_all();

  //add_file_system("device", "dir", "type", "total", "free", "avail", "used");


  //Kernel Version
  GtkLabel *kernel_ver_label = GTK_LABEL(gtk_builder_get_object(builder, 
                                        "kernel_version_text"));

  string ver = kernel_version();
  gtk_label_set_text(kernel_ver_label, ver);
  free(ver);
  ver = NULL;

  //Processor Version
  GtkLabel *processor_ver_label = GTK_LABEL(gtk_builder_get_object(builder,
                                            "processor_text"));
 
  string proc_ver = processor_version(); 
  gtk_label_set_text(processor_ver_label, proc_ver);
  free(proc_ver);
  proc_ver = NULL;

  //Memory
  GtkLabel *memory_label = GTK_LABEL(gtk_builder_get_object(builder,
                                     "memory_text"));

  char mem_str[30];
  sprintf(mem_str, "Memory:   %.3f GiB", meminfo());
  gtk_label_set_text(memory_label, mem_str);


  //Available Disk Space
  GtkLabel *disk_space_label = GTK_LABEL(gtk_builder_get_object(builder,
                                         "disk_space_text"));

  char disk_str[50];
  sprintf(disk_str, "Available disk space: %.3f GiB", total_space());
  gtk_label_set_text(disk_space_label, disk_str);

  g_object_unref(builder);

  gtk_widget_show(g_main_window);
  gtk_main();

  return 0;
}

/*
 * Sorts process tree given column
 */

void sort_process(int col) {
  GtkTreeSortable *sortable;
  GtkSortType      order;
  gint             sortid;

  sortable = GTK_TREE_SORTABLE(g_process_list);

  /* If we are already sorting by year, reverse sort order,
   *  otherwise set it to year in ascending order */

  if (gtk_tree_sortable_get_sort_column_id(sortable, &sortid, &order) == TRUE && sortid == col) {
    GtkSortType neworder;

    neworder = (order == GTK_SORT_ASCENDING) ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;

    gtk_tree_sortable_set_sort_column_id(sortable, col, neworder);
  }
  else {
    gtk_tree_sortable_set_sort_column_id(sortable, col, GTK_SORT_ASCENDING);
  }
}

/*
 * Sorts process tree given column
 */

void sort_file_system(int col) {
  GtkTreeSortable *sortable;
  GtkSortType      order;
  gint             sortid;

  sortable = GTK_TREE_SORTABLE(g_file_systems);

  /* If we are already sorting by year, reverse sort order,
   *  otherwise set it to year in ascending order */

  if (gtk_tree_sortable_get_sort_column_id(sortable, &sortid, &order) == TRUE && sortid == col) {
    GtkSortType neworder;

    neworder = (order == GTK_SORT_ASCENDING) ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;

    gtk_tree_sortable_set_sort_column_id(sortable, col, neworder);
  }
  else {
    gtk_tree_sortable_set_sort_column_id(sortable, col, GTK_SORT_ASCENDING);
  }
}

/*
 * Creates a popup menu when the user right clicks on the process treeview
 * Returns false if button is not a right click so it is not handled
 */

gboolean on_process_right_click(GtkWidget *widget, GdkEvent *event,
                                gpointer user_data) {
  if (event == NULL) {
    return false;
  }
  if (event->button.button == 3) {
    gtk_menu_popup_at_pointer(process_popup, event);
    return true;
  }
  return false;
}
 
/*
 * Handler for signal that the process radio button group has been changed
 */
 
void on_all_processes_button_toggled() {
  if (g_processes_button_state == ALL_PROCESSES) {
    g_processes_button_state = 1;
    printf("My processes enabled\n");
    clear_process_list();

    init_headers();
    list("M");
    g_proc_array = get_headers();
    GtkTreeIter *parent = malloc(sizeof(GtkTreeIter));
    for (int i = 0; i < get_header_num(); i++) {
      process_t *proc = g_proc_array[i];
      if (proc->flag == 1) {
        continue;
      }
      char memory[16];
      sprintf(memory, "%.2f MiB", get_mem_usage(proc->pid));
      add_process(parent, NULL, proc->name,
      proc->stat, proc->cpu_usage, proc->pid, memory);
      proc->flag = 1;
      if (g_tree_button_state == TREE_VIEW) {
        add_children(parent, proc->pid);
      }
    }
    free(parent);
    free_all();
    //check tree button state
    //repopulate list 
  }
  else if (g_processes_button_state == MY_PROCESSES) {
    g_processes_button_state = ALL_PROCESSES;
    printf("All processes enabled\n");
    clear_process_list();

    init_headers();
    list("A");
    g_proc_array = get_headers();
    GtkTreeIter *parent = malloc(sizeof(GtkTreeIter));
    for (int i = 0; i < get_header_num(); i++) {
      process_t *proc = g_proc_array[i];
      if (proc->flag == 1) {
        continue;
      }
      char memory[16];
      sprintf(memory, "%.2f MiB", get_mem_usage(proc->pid));
      add_process(parent, NULL, proc->name,
      proc->stat, proc->cpu_usage, proc->pid, memory);
      proc->flag = 1;
      if (g_tree_button_state == TREE_VIEW) {
        add_children(parent, proc->pid);
      }
    }
    free(parent);
    free_all();
    //check tree button state
    //repopulate list 
  }
  else {
    perror("Illegal process button state\n");
  }
}

/*
 * Handler for signal that tree checkbox has been changed
 */
 
void on_tree_format_button_toggled() {
  if (g_tree_button_state == LIST_VIEW) {
    g_tree_button_state = TREE_VIEW;
    printf("Tree view enabled\n");
    clear_process_list();
    init_headers();
    if (g_processes_button_state == MY_PROCESSES) {
      list("M");
    }
    else {
      list("A");
    }
    g_proc_array = get_headers();
    GtkTreeIter *parent = malloc(sizeof(GtkTreeIter));
    for (int i = 0; i < get_header_num(); i++) {
      process_t *proc = g_proc_array[i];
      if (proc->flag == 1) {
        continue;
      }
      char memory[16];
      sprintf(memory, "%.2f MiB", get_mem_usage(proc->pid));
      add_process(parent, NULL, proc->name,
      proc->stat, proc->cpu_usage, proc->pid, memory);
      proc->flag = 1;
      add_children(parent, proc->pid);
    }
    free(parent);
    free_all();
    //check process button state
    //repopulate list 
  }
  else if (g_tree_button_state == TREE_VIEW) {
    g_tree_button_state = LIST_VIEW;
    printf("Tree view disabled\n");
    clear_process_list();
    init_headers();
    if (g_processes_button_state == MY_PROCESSES) {
      list("M");
    }
    else {
      list("A");
    }
    g_proc_array = get_headers();
    GtkTreeIter *parent = malloc(sizeof(GtkTreeIter));
    for (int i = 0; i < get_header_num(); i++) {
      process_t *proc = g_proc_array[i];
      if (proc->flag == 1) {
        continue;
      }
      char memory[16];
      sprintf(memory, "%.2f MiB", get_mem_usage(proc->pid));
      add_process(parent, NULL, proc->name,
      proc->stat, proc->cpu_usage, proc->pid, memory);
      proc->flag = 1;
    }
    free(parent);
    free_all();
    //check process button state
    //repopulate list 
  }
  else {
    printf("Illegal tree view button state\n");
  }
}

/*
 * Handler for the signal that the refresh button has been clicked
 */
 
void on_refresh_button_clicked() {
  printf("Refreshing...\n");
  clear_process_list();
  if (g_processes_button_state == MY_PROCESSES) {
    init_headers();
    list("M");
    g_proc_array = get_headers();
    GtkTreeIter *parent = malloc(sizeof(GtkTreeIter));
    for (int i = 0; i < get_header_num(); i++) {
      process_t *proc = g_proc_array[i];
      if (proc->flag == 1) {
        continue;
      }
      char memory[16];
      sprintf(memory, "%.2f MiB", get_mem_usage(proc->pid));
      add_process(parent, NULL, proc->name,
      proc->stat, proc->cpu_usage, proc->pid, memory);
      proc->flag = 1;
      if (g_tree_button_state == TREE_VIEW) {
        add_children(parent, proc->pid);
      }
    }
    free(parent);
    free_all(); 
  }
  else if (g_processes_button_state == ALL_PROCESSES) {
    init_headers();
    list("A");
    g_proc_array = get_headers();
    GtkTreeIter *parent = malloc(sizeof(GtkTreeIter));
    for (int i = 0; i < get_header_num(); i++) {
      process_t *proc = g_proc_array[i];
      if (proc->flag == 1) {
        continue;
      }
      char memory[16];
      sprintf(memory, "%.2f MiB", get_mem_usage(proc->pid));
      add_process(parent, NULL, proc->name,
      proc->stat, proc->cpu_usage, proc->pid, memory);
      proc->flag = 1;
      if (g_tree_button_state == TREE_VIEW) {
        add_children(parent, proc->pid);
      }
    }
    free(parent);
    free_all();
  }
  //check process and tree button state
  //repopulate list 
}

/*
 * Helper function that gets the pid for the relevant selection in the treeview
 * Returns -1 if nothing is selected
 */
 
int get_selected_process_pid() {
  GtkTreeSelection *selected = gtk_tree_view_get_selection(g_process_view);
  if (selected == NULL) {
    return -1;
  }
  GtkTreeIter iter = { 0 };
  if (!(gtk_tree_selection_get_selected(selected,
      (GtkTreeModel **) &g_process_list, &iter))) {
    return -1;
  }
  
  GValue value = G_VALUE_INIT;
  gtk_tree_model_get_value((GtkTreeModel *)g_process_list, &iter, 3, &value);
  char *pid_text = g_strdup_value_contents(&value);
  int pid = atoi(pid_text);
  free(pid_text);

  return pid;
}

/*
 * Handler for when properties is selected in process popup menu
 */

void process_properties() {
  printf("Properties signal on PID %d\n", get_selected_process_pid());
  
  GtkBuilder *builder = gtk_builder_new();
  gtk_builder_add_from_file (builder, "manager.glade", NULL);

  GtkWidget *properties_window = GTK_WIDGET(gtk_builder_get_object(builder, "process_properties_window"));
  gtk_builder_connect_signals(builder, NULL);
  //GtkTreeView *properties_view = 
  GTK_TREE_VIEW(gtk_builder_get_object(builder, "process_properties_treeview"));
  GtkListStore *properties_list = GTK_LIST_STORE(gtk_builder_get_object(builder, "properties_liststore"));
  GtkLabel *label = GTK_LABEL(gtk_builder_get_object(builder, 
  "process_properties_text"));
  char title[1024];
  sprintf(title, "Properties of process <%d>", get_selected_process_pid());
  gtk_label_set_text(label, title);
  g_object_unref(builder);
  
  //TODO: Populate properties list
  int pid = get_selected_process_pid();
  char* properties = get_details(pid);
  if (properties) {
    char name[1024];
    char user[512];
    char stat[32];
    char memory[32];
    char vm[32];
    char rm[32];
    char sm[32];
    char cpu_time[32];
    char date[36];
    sscanf(properties, "%s %s %s %s %s %s %s %s %s", name, user, stat, memory, vm, rm ,sm,
                                                    cpu_time, date);
    gtk_list_store_insert_with_values(properties_list, NULL, -1, 0, "Name", 1, name, -1);
    gtk_list_store_insert_with_values(properties_list, NULL, -1, 0, "User", 1, user, -1);
    gtk_list_store_insert_with_values(properties_list, NULL, -1, 0, "Status", 1, stat, -1);
    gtk_list_store_insert_with_values(properties_list, NULL, -1, 0, "Memory", 1, memory, -1);
    gtk_list_store_insert_with_values(properties_list, NULL, -1, 0, "Virtual Memory", 1, vm, -1);
    gtk_list_store_insert_with_values(properties_list, NULL, -1, 0, "Resident Memory", 1, rm, -1);
    gtk_list_store_insert_with_values(properties_list, NULL, -1, 0, "Shared Memory", 1, sm, -1);
    gtk_list_store_insert_with_values(properties_list, NULL, -1, 0, "CPU Time", 1, cpu_time, -1);
    gtk_list_store_insert_with_values(properties_list, NULL, -1, 0, "Start Time", 1, date, -1);
  }
  free(properties);
  gtk_widget_show(properties_window);
}

/*
 * Handler for when list open files is selected in process popup menu
 */

void process_list_open_files() {
  printf("Open files signal on PID %d\n", get_selected_process_pid());
  
  GtkBuilder *builder = gtk_builder_new();
  gtk_builder_add_from_file (builder, "manager.glade", NULL);

  GtkWidget *open_files_window = GTK_WIDGET(gtk_builder_get_object(builder, "open_files_window"));
  gtk_builder_connect_signals(builder, NULL);
  //GtkTreeView *open_files_view = 
  GTK_TREE_VIEW(gtk_builder_get_object(builder, "open_files_treeview"));
  GtkListStore *open_files_list = GTK_LIST_STORE(gtk_builder_get_object(builder, "open_files_liststore"));
  GtkLabel *label = GTK_LABEL(gtk_builder_get_object(builder, 
  "open_files_text"));
  char title[1024];
  sprintf(title, "Files opened by <%d>", get_selected_process_pid());
  gtk_label_set_text(label, title);
  g_object_unref(builder);
  
  //TODO: Populate open files list
  int pid = get_selected_process_pid();
  char* files = lsof(pid);
  char* tmp = NULL;
  if (files) {
    tmp = strtok(files, "\n");
  }
  while (tmp) {
    int fd = 0;
    char type[128];
    char path[1024];
    sscanf(tmp, "%d %s %s", &fd, type, path);
    gtk_list_store_insert_with_values(open_files_list, NULL, -1, 0, fd, 1, type, 2, path, -1);
    tmp = strtok(NULL, "\n");
  }
  gtk_widget_show(open_files_window);
  free(files);
}

/*
 * Handler for when memory maps is selected in process popup menu
 */

void process_list_memory_maps() {
  printf("Memory map signal on PID %d\n", get_selected_process_pid());
  
  GtkBuilder *builder = gtk_builder_new();
  gtk_builder_add_from_file (builder, "manager.glade", NULL);

  GtkWidget *memory_maps_window = GTK_WIDGET(gtk_builder_get_object(builder, "memory_maps_window"));
  gtk_builder_connect_signals(builder, NULL);
  //GtkTreeView *memory_maps_view = 
  GTK_TREE_VIEW(gtk_builder_get_object(builder, "memory_maps_treeview"));
  GtkListStore *memory_maps_list = GTK_LIST_STORE(gtk_builder_get_object(builder, "memory_maps_liststore"));
  GtkLabel *label = GTK_LABEL(gtk_builder_get_object(builder, 
  "memory_maps_text"));
  char title[1024];
  sprintf(title, "Memory maps for process <%d>", get_selected_process_pid());
  gtk_label_set_text(label, title);
  g_object_unref(builder);
  
  //TODO: Populate memory_maps_list
  
  int pid = get_selected_process_pid();
  char* files = get_map(pid);
  char* tmp = NULL;

  if (files) {
    tmp = strtok(files, "\n");
  }
  while (tmp) {
    char filename[1024] = " ";
    char vmstart[20] = " ";
    char vmend[20] = " ";
    char vmsize[32] = " ";
    char flags[20] = " ";
    char offset[32] = " ";
    char pclean[32] = " ";
    char pdirty[32] = " ";
    char sclean[32] = " ";
    char sdirty[32] = " ";

    sscanf(tmp, "%s %s %s %s %s %s %s %s %s %s", filename, vmstart, vmend,
                  vmsize, flags, offset, pclean, pdirty, sclean, sdirty);
    /*
    printf("%s %s %s %s %s %s %s %s %s %s\n", filename, vmstart, vmend,
                  vmsize, flags, offset, pclean, pdirty, sclean, sdirty);
   */
    gtk_list_store_insert_with_values(memory_maps_list, NULL, -1, 0, filename,
                                    1, vmstart, 2, vmend, 3, vmsize, 4, flags, 5, offset,
                                    6, pclean, 7, pdirty, 8, sclean, 9, sdirty, -1);
   
    tmp = strtok(NULL, "\n");
  }
 
  gtk_widget_show(memory_maps_window);
  free(files);
}

/*
 * Handler for when kill is selected in process popup menu
 */

void process_kill() {
  printf("Kill signal on PID %d\n", get_selected_process_pid());
  kill_by_pid(get_selected_process_pid());
}

/*
 * Handler for when continue is selected in process popup menu
 */

void process_continue() {
  printf("Continue signal on PID %d\n", get_selected_process_pid());
  cont_by_pid(get_selected_process_pid());
}

/*
 * Handler for when stop is selected in process popup menu
 */

void process_stop() {
  printf("Stop signal on PID %d\n", get_selected_process_pid());
  stop_by_pid(get_selected_process_pid());
}

/*
 * Handler for drawing CPU history
 */

gboolean draw_cpu_history (GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    GdkRectangle da;     /* GtkDrawingArea size */
    gdouble dx = 2.0;
    gdouble dy = 2.0;    /* Pixels between each point */
    gdouble i;
    gdouble clip_x1 = 0.0;
    gdouble clip_y1 = 0.0;
    gdouble clip_x2 = 0.0;
    gdouble clip_y2 = 0.0;

    GdkWindow *window = gtk_widget_get_window(widget);

    /* Determine GtkDrawingArea dimensions */
    gdk_window_get_geometry (window,
            &da.x,
            &da.y,
            &da.width,
            &da.height);

    /* Draw on a black background */
    cairo_set_source_rgb (cr, 255.0, 255.0, 255.0);
    cairo_paint (cr);

    /* Change the transformation matrix */
    cairo_translate (cr, 0, 0);
    cairo_scale (cr, ZOOM_X, ZOOM_Y);

    /* Determine the data points to calculate (ie. those in the clipping zone */
    cairo_device_to_user_distance (cr, &dx, &dy);
    cairo_clip_extents (cr, &clip_x1, &clip_y1, &clip_x2, &clip_y2);
    cairo_set_line_width (cr, dx);

    /* Link each data point */
    /* Plotting of points occurs here */
    /* third arg of cairo_line_to is the point being added */
    for (i = 0; i < clip_x2; i += dx) {
      double time = cpu();
      cairo_line_to (cr, i, 0.5 * time + 0.5);
    }
    /* Draw the curve */
    cairo_set_source_rgba (cr, 1, 0.2, 0.2, 0.6);
    cairo_stroke (cr);

    return FALSE;
}

/*
 * Handler for drawing memory history
 */

gboolean draw_memory_history (GtkWidget *widget, cairo_t *cr, gpointer user_data) {

    GdkRectangle da;      /* GtkDrawingArea size */
    gdouble dx = 2.0; 
    gdouble dy = 2.0;     /* Pixels between each point */
    gdouble i;
    gdouble clip_x1 = 0.0;
    gdouble clip_y1 = 0.0;
    gdouble clip_x2 = 0.0;
    gdouble clip_y2 = 0.0;

    GdkWindow *window = gtk_widget_get_window(widget);

    /* Determine GtkDrawingArea dimensions */
    gdk_window_get_geometry (window,
            &da.x,
            &da.y,
            &da.width,
            &da.height);

    /* Draw on a black background */
    cairo_set_source_rgb (cr, 255.0, 255.0, 255.0);

    cairo_paint (cr);

    /* Change the transformation matrix */
    cairo_translate (cr, 0, 0);
    cairo_scale (cr, ZOOM_X, ZOOM_Y);

    /* Determine the data points to calculate (ie. those in the clipping zone */
    cairo_device_to_user_distance (cr, &dx, &dy);
    cairo_clip_extents (cr, &clip_x1, &clip_y1, &clip_x2, &clip_y2);
    cairo_set_line_width (cr, dx);

    /* Link each data point */
    for (i = 0; i < clip_x2; i += dx) {
      double *per = swap_mem_usage();
      double mem = per[1];
      cairo_line_to (cr, i, 1 - mem/100);
      free(per);
      per = NULL;
    }

    /* Draw the curve */
    cairo_set_source_rgba (cr, 1, 0.2, 0.2, 0.6);
    cairo_stroke (cr);

    for (i = 0; i < clip_x2; i += dx) {
      double *per = swap_mem_usage();
      double swp = per[0];
      cairo_line_to (cr, i, 1 - swp/100 - 0.1);
      free(per);
      per = NULL;
    }

    /* Draw the curve */
    cairo_set_source_rgba (cr, 0, 0.50, 0.0, 1.0);
    cairo_stroke (cr);

    return FALSE;
}

/*
 * Handler for drawing network history
 */

gboolean draw_network_history (GtkWidget *widget, cairo_t *cr, gpointer user_data) {

    GdkRectangle da;     /* GtkDrawingArea size */
    gdouble dx = 2.0;
    gdouble dy = 2.0;    /* Pixels between each point */
    gdouble i;
    gdouble clip_x1 = 0.0;
    gdouble clip_y1 = 0.0;
    gdouble clip_x2 = 0.0;
    gdouble clip_y2 = 0.0;

    GdkWindow *window = gtk_widget_get_window(widget);

    /* Determine GtkDrawingArea dimensions */
    gdk_window_get_geometry (window,
            &da.x,
            &da.y,
            &da.width,
            &da.height);

    /* Draw on a black background */
    cairo_set_source_rgb (cr, 255.0, 255.0, 255.0);
    cairo_paint (cr);

    /* Change the transformation matrix */
    cairo_translate (cr, 0, 0);
    cairo_scale (cr, ZOOM_X, ZOOM_Y);

    /* Determine the data points to calculate (ie. those in the clipping zone */
    cairo_device_to_user_distance (cr, &dx, &dy);

    cairo_clip_extents (cr, &clip_x1, &clip_y1, &clip_x2, &clip_y2);


    cairo_set_line_width (cr, dx);

    /* Draws x and y axis */

    /* Link each data point */
    for (i = 0; i < clip_x2; i += dx) {
      long *net_use1 = network_usage();
      long received_prev = net_use1[0];
      usleep(1000);

      long *net_use2 = network_usage();
      long received_new = net_use2[0];
      long diff_rec = received_new - received_prev;
      double difference_received = (double) diff_rec/100;

      free(net_use1);
      free(net_use2);
      net_use1 = NULL;
      net_use2 = NULL;
      cairo_line_to (cr, i, 1 - difference_received/100 - 0.2);
    }
    cairo_set_source_rgba (cr, 1, 0.2, 0.2, 1.0);
    cairo_stroke (cr);

    for (i = 0; i < clip_x2; i += dx) {
      long *net_use1 = network_usage();
      long transmit_prev = net_use1[1];
      usleep(1000);

      long *net_use2 = network_usage();
      long transmit_new = net_use2[1];
      long diff_tra = transmit_new - transmit_prev;
      double difference_transmit = (double) diff_tra/100;

      free(net_use1);
      free(net_use2);
      net_use1 = NULL;
      net_use2 = NULL;
      cairo_line_to (cr, i, 1 - difference_transmit/100 - 0.2);
    }
    cairo_set_source_rgba (cr, 0.0, 0.0, 1.0, 0.4);
    cairo_stroke (cr);


    return FALSE;
}

/*
 * Signal handler for process list name sort
 */

void on_process_list_name_col_clicked() {
  sort_process(NAME);
}

/*
 * Signal handler for process list status sort
 */

void on_process_list_status_col_clicked() {
  sort_process(STATUS);
}

/*
 * Signal handler for process list cpu sort
 */

void on_process_list_cpu_col_clicked() {
  sort_process(CPU);
}

/*
 * Signal handler for process list pid sort
 */

void on_process_list_pid_col_clicked() {
  sort_process(PID);
}

/*
 * Signal handler for process list memory sort
 */

void on_process_list_memory_col_clicked() {
  sort_process(MEMORY);
}

/*
 * Signal handler for file list device sort
 */

void on_file_systems_device_col_clicked() {
  sort_file_system(DEVICE);
}

/*
 * Signal handler for file list directory sort
 */

void on_file_systems_directory_col_clicked() {
  sort_file_system(DIRECTORY);
}

/*
 * Signal handler for file list type sort
 */

void on_file_systems_type_col_clicked() {
  sort_file_system(TYPE);
}

/*
 * Signal handler for file list total sort
 */

void on_file_systems_total_col_clicked() {
  sort_file_system(TOTAL);
}

/*
 * Signal handler for file list free sort
 */

void on_file_systems_free_col_clicked() {
  sort_file_system(FREE);
}

/*
 * Signal handler for file list available sort
 */

void on_file_systems_available_col_clicked() {
  sort_file_system(AVAILABLE);
}

/*
 * Signal handler for file list used sort
 */

void on_file_systems_used_col_clicked() {
  sort_file_system(USED);
}

/*
 * Handler for when top menu quit is activated
 * Quits the program
 */

void on_menu_quit_activate() {
  gtk_main_quit();
}

/*
 * Handler for when the about menu is activated
 */

void on_menu_about_activate() {
  GtkBuilder *builder;
  
  builder = gtk_builder_new();
  gtk_builder_add_from_file (builder, "manager.glade", NULL);

  g_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "about_dialog"));
  gtk_builder_connect_signals(builder, NULL);
  
  gtk_window_set_transient_for (GTK_WINDOW(g_dialog), GTK_WINDOW(g_main_window));
  
  g_object_unref(builder);

  gtk_widget_show(g_dialog);
  gtk_dialog_run(GTK_DIALOG(g_dialog));
  gtk_widget_destroy(g_dialog);
}

/*
 * Handler for when the main window is destroyed by the user clicking
 * the X button, quitting the program
 */

void on_main_window_destroy() {
  gtk_main_quit();
}
