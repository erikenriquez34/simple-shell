#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* constants */
const char prompt[16] = "erik@15t-dy100";
const int MAXLINE = 1024;
const int verbose = 1;

/* evaluate a command line */
void eval(char* cmdline) {
    char* argv[MAXLINE];
    char buf[MAXLINE];
    pid_t pid;

    strcpy(buf, cmdline);

    /* tokenize the line */
    int argc = 0;
    char* token = strtok(buf, " \t");
    while (token != NULL) {
        argv[argc] = token;
        token = strtok(NULL, " \t");
        argc++;
    }

    argv[argc] = NULL;
    if (argv[0] == NULL) {return;}

    /* builtin exit */
    if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "exit") == 0) {
        exit(0);
    }

    /* use exec to run command */
    if ((pid = fork()) == 0) {
        if (execvp(argv[0], argv) < 0) {
            printf("%s: Command not found.\n", argv[0]);
            exit(0);
        }
    }

    /* make parent wait for any fg */
    waitpid(pid, NULL, 0);
}

/* main loop */
int main(int argc, char** argv) {
    char cmdline[MAXLINE];

    while (1) {
        printf("\e[0;36m%s\e[0m> ", prompt);
        fflush(stdout);

        /* handle errors and end of file */
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            perror("fgets error");
        if (feof(stdin))
            exit(0);

        /* strip command line*/
        cmdline[strcspn(cmdline, "\n")] = '\0';
        eval(cmdline);
    }
}