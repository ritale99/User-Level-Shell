all: cshell.c 
	gcc -g -lm -Wall -o cshell cshell.c

clean: 
	$(RM) cshell
