#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* constants */
const char prompt[16] = "erik@15t-dy100";
#define MAXLINE 1024
#define MAXJOBS 16
#define MAXJID 1<<16;
int verbose = 1;

struct job_t {              /* The job struct */
	pid_t pid;              /* job PID */
	int jid;                /* job ID [1, 2, ...] */
	int state;              /* UNDEF, BG, FG, or ST */
	char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */

/* function prototypes */
int builtin_cmd(char** argv);
void eval(char* cmdline);
void usage(void);

int builtin_cmd(char** argv) {
    if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "exit") == 0) {
        exit(0);
    }
	return 0;
}

/* evaluate a command line */
void eval(char* cmdline) {
    char* argv[MAXLINE];
    char buf[MAXLINE];
    pid_t pid;
    int bg;

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

    /* check for background */
    if (argc > 0 && strcmp(argv[argc-1], "&") == 0) {
        bg = 1;
        argv[argc-1] = NULL;
    }

    /* builtin commands */
    builtin_cmd(argv);

    /* if forked process, use exec to run command. Parent proceed to bg flag check */
    if ((pid = fork()) == 0) {
        if (execvp(argv[0], argv) < 0) {
            printf("%s: Command not found.\n", argv[0]);
            exit(0);
        }
    }

    /* use bg to either wait or run in background*/
    if (!bg) {
        waitpid(pid, NULL, 0);
    } else {
        printf("[%d] %s\n", pid, cmdline);
    }
}

/* main loop */
int main(int argc, char** argv) {
    char cmdline[MAXLINE];
    char c;

    while ((c = getopt(argc, argv, "hv")) != EOF) {
		switch (c) {
			case 'h': /* help */
				usage();
				break;

			case 'v': /* verbose mode */
				verbose = 1;
				break;

			default:
				usage();
		}
	}

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

void usage(void) {
    printf("Usage: shell [-hv]\n");
	printf("   -h   print this message\n");
	printf("   -v   print additional diagnostic information\n");
	exit(1);
}