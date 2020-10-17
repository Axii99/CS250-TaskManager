CC = gcc

CFLAGS = -g -Wall -Werror `pkg-config --cflags gtk+-3.0` -o

PKG = `pkg-config --libs gtk+-3.0` -rdynamic -lm

SRC = main.c proc_manager.c proc_detail.c basic_system_info.c filesystems.c usage.c
# Once ready: processes.c resources.c system_info.c file_systems.c

manager: $(SRC)
	$(CC) $(CFLAGS) manager $(SRC) $(PKG)

clean:
	rm -f *.o
