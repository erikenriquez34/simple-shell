CC = gcc
CFLAGS = -Wall -g -Werror

shell:
        $(CC) $(CFLAGS) shell.c -o shell

clean:
        rm -f shell