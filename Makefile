# Make file for pman
CFLAGS= -DNDEBUG -g -Wall
LDLIBS= -lreadline -lm
CC=gcc

all: ADTlinkedlist.o utils.o pman.o 
	$(CC) $^ $(LDLIBS) $(CFLAGS) -o pman

%.o: %.c
	$(CC) -c $(LDLIBS) $(CFLAGS) $^
	
clean:
	rm -f *.o *.gch pman

debug:
	$(MAKE) CFLAGS='-Wextra -pedantic-errors -fsanitize=address -Wall -g'
