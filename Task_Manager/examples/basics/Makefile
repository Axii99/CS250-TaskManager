# If you add .c/.h pairs, add their names without any extension here
# Try to only modify this line
SOURCE_NAMES = window button packing

# Use GNU compiler
CC = gcc -g -Wall -Werror `pkg-config --cflags gtk+-3.0` -o
PKG = `pkg-config --libs gtk+-3.0`

#SRC_H = $(SOURCE_NAMES:=.h) 
SRC_O = $(SOURCE_NAMES:=.o)

all: window button packing

window: window.c
	$(CC) window window.c  $(PKG)
	
button: button.c
	$(CC) button button.c  $(PKG)
	
packing: packing.c
	$(CC) packing packing.c  $(PKG)

