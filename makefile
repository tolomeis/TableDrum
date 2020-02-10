#---------------------------------------------------
# Target file to be compiled by default
#---------------------------------------------------
MAIN = tableDrum
#---------------------------------------------------
# CC is the compiler to be used
#---------------------------------------------------
CC = gcc
#---------------------------------------------------
# CFLAGS are the options passed to the compiler
#---------------------------------------------------
CFLAGS = -lpthread -lasound -lm
#---------------------------------------------------
# OBJS are the object files to be linked
#---------------------------------------------------
OBJ1 = alsa_interface
OBJ2 = ptask
OBJS = $(MAIN).o $(OBJ1).o $(OBJ2).o
#---------------------------------------------------
# LIBS are the external libraries to be used
#---------------------------------------------------
LIBS = `allegro-config --libs`

#---------------------------------------------------
# Dependencies
# main: main.o alsa_interface.o ptask.o
# gcc -o main main.o ... -lm -lasound ...
# main.o: main.c
# gcc -c ,main.c
#---------------------------------------------------
$(MAIN): $(OBJS)
	$(CC) -o $(MAIN) $(OBJS) $(LIBS) $(CFLAGS)
$(MAIN).o: $(MAIN).c
	$(CC) -c $(MAIN).c
$(OBJ1).o: $(OBJ1).c
	$(CC) -c $(OBJ1).c
$(OBJ2).o: $(OBJ2).c
	$(CC) -c $(OBJ2).c
#---------------------------------------------------
# Command that can be specified inline: make clean
#---------------------------------------------------
clean:
	rm -rf *o $(MAIN)