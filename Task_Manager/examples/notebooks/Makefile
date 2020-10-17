CC = gcc

CFLAGS = -Wall              \
         -Werror

notebook: notebook.c 
	$(CC) notebook.c -o notebook $(CFLAGS) `pkg-config gtk+-2.0 --cflags --libs`

clean: 
	rm -f *.o notebook
