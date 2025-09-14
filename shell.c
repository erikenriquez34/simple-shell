#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

/* global */
#define PROMPT "\e[0;36merik@15t-dy100\e[0m> "
#define MAXLINE 1024
#define MAXJOBS 16
#define MAXJID 1<<16
#define VB "[\e[0;31mVerbose\e[0m]"
int verbose = 0;
int nextjid = 1; 

/* job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

struct job_t {              /* The job struct */
	pid_t pid;              /* job PID */
	int jid;                /* job ID [1, 2, ...] */
	int state;              /* UNDEF, BG, FG, or ST */
	char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */

/* function prototypes */
int builtin_cmd(char** argv);
void eval_pipeline();
void eval(char* cmdline);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);

void clearjob(struct job_t* job);
void initjobs(struct job_t* jobs);
int maxjid(struct job_t* jobs);
int addjob(struct job_t* jobs, pid_t pid, int state, char* cmdline);
int deletejob(struct job_t* jobs, pid_t pid);
pid_t fgpid(struct job_t* jobs);
struct job_t* getjobpid(struct job_t* jobs, pid_t pid);
struct job_t* getjobjid(struct job_t* jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t* jobs);
void usage(void);

/* builtin_cmd - If the user has typed a built-in command then execute immediately*/
int builtin_cmd(char** argv) {
    if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "exit") == 0) { /* exit */
        exit(0);
    } else if (strcmp(argv[0], "jobs") == 0) { /* view jobs */
    	listjobs(jobs);
		return 1;
	} else if (strcmp(argv[0], "fg") == 0) { /* set fg process */
		if (argv[1] == NULL) {
			printf("fg command requires %%jobid argument\n");
			return 1;
		}
		if (argv[1][0] != '%') {
			printf("fg argument must be %%jobid\n");
			return 1;
		}
		int jid = atoi(&argv[1][1]);
		struct job_t *job = getjobjid(jobs, jid);
		if (job == NULL) {
			printf("%%%d: No such job\n", jid);
			return 1;
		}
		kill(-job->pid, SIGCONT); /* signal continues the job */
		job->state = FG;
		waitfg(job->pid);
		return 1;
	} else if (strcmp(argv[0], "bg") == 0) { /* begin but keep running in background */
		if (argv[1] == NULL) {
			printf("bg command requires %%jobid argument\n");
			return 1;
		}
		if (argv[1][0] != '%') {
			printf("bg argument must be %%jobid\n");
			return 1;
		}

		int jid = atoi(&argv[1][1]);
		struct job_t *job = getjobjid(jobs, jid);
		if (job == NULL) {
			printf("%%%d: No such job\n", jid);
			return 1;
		}

		kill(-job->pid, SIGCONT); /* continues the job */
		job->state = BG;
		printf("[%d] (%d) %s\n", job->jid, job->pid, job->cmdline);
		return 1;
	}
	return 0;
}

/* specialized run for any lines with pipes */
void eval_pipeline() {
    printf("Handle pipe here!\n");
}

/* evaluate a command line */
void eval(char* cmdline) {
    char* argv[MAXLINE];
    char buf[MAXLINE];
    pid_t pid;
    int bg = 0;
	int pipes = 0;

    strcpy(buf, cmdline);

    /* tokenize the line */
    int argc = 0;
    char* token = strtok(buf, " \t");
    while (token != NULL) {
        argv[argc] = token;
		if (strcmp(token, "|") == 0) {pipes = 1;}
        token = strtok(NULL, " \t");
        argc++;
    }

    argv[argc] = NULL;
	/* spamming enter or someething */
    if (argv[0] == NULL) {return;}

    /* check for background */
    if (argc > 0 && strcmp(argv[argc-1], "&") == 0) {
        bg = 1;
        argv[argc-1] = NULL;
    }

	/* handle pipelines */
	if (pipes) {
		eval_pipeline();
	}

    /* check builtins first, then check others. I think this works could be problematic*/
    if (!builtin_cmd(argv)) {
		/* if forked process, use exec to run command. Parent proceed to bg flag check */
		if ((pid = fork()) == 0) {
			setpgid(0, 0);
			if (execvp(argv[0], argv) < 0) {
				printf("%s: Command not found.\n", argv[0]);
				exit(0);
			}
		}
	}

    /* use bg to either wait or run in background*/
    if (!bg) {
		addjob(jobs, pid, FG, cmdline);

        int status; /* if stopped by ctrl+z, return control */
		if (waitpid(pid, &status, WUNTRACED) > 0) {
			if (WIFSTOPPED(status)) {
				struct job_t *job = getjobpid(jobs, pid);
				if (job) {
					job->state = ST;
					printf("\n[%d] (%d) stopped by signal %d\n",
						job->jid, pid, WSTOPSIG(status));
				}
			} else { /* process completed, kill */
				deletejob(jobs, pid);
			}
		}
    } else {
		addjob(jobs, pid, BG, cmdline);
        printf("[%d] (%d) %s\n", pid2jid(pid), pid, cmdline);
    }
}

void waitfg(pid_t pid) {
    int status;
    while (fgpid(jobs) == pid) {
        /* wait for process */
        if (waitpid(pid, &status, WUNTRACED) > 0) {
            if (WIFSTOPPED(status)) {
                struct job_t *job = getjobpid(jobs, pid);
                if (job) job->state = ST;
                break;
            } else {
                deletejob(jobs, pid);
                break;
            }
        }
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
				printf("%s%s Toggled verbose mode.\n", PROMPT, VB);
				verbose = 1;
				break;

			default:
				usage();
		}
	}

	/* init */
	initjobs(jobs);
	signal(SIGCHLD, sigchld_handler);
	signal(SIGTSTP, sigtstp_handler);
	signal(SIGINT, sigint_handler);
	
    while (1) {
        printf(PROMPT);
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

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
void sigchld_handler(int sig) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		/* Child terminated normally or by signal then delete job */
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            deletejob(jobs, pid);
			if (verbose) {printf("\n%s Deleted job with pid=%d by SIGCHLD.\n", VB, pid);}
		/* Child stopped so mark job as stopped */
        } else if (WIFSTOPPED(status)) {
            struct job_t *job = getjobpid(jobs, pid);
            if (job != NULL) {
                job->state = ST;
                printf("Job [%d] (%d) stopped\n", job->jid, pid);
            }
        }
    }
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig) {
	pid_t pid = fgpid(jobs);
    if (pid != 0) {
		/*-pid sends interrupt to group*/
        kill(-pid, SIGINT);
    }
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig) {
    pid_t pid = fgpid(jobs);
    if (pid != 0) {
		/*-pid sends  to group*/
        kill(-pid, SIGTSTP);
    }
}

/* clearjob - clear the entries in a job struct */
void clearjob(struct job_t* job) {
	job->pid = 0;
	job->jid = 0;
	job->state = UNDEF;
	job->cmdline[0] = '\0';
}

/* initjobs - initialize the job list */
void initjobs(struct job_t* jobs) {
	int i;

	for (i = 0; i < MAXJOBS; i++)
		clearjob(&jobs[i]);
}

/* maxjid - returns largest allocated job ID */
int maxjid(struct job_t* jobs) {
	int i, max=0;

	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].jid > max)
			max = jobs[i].jid;
	return max;
}

/* addjob - add a job to the job list */
int addjob(struct job_t* jobs, pid_t pid, int state, char* cmdline) {
	int i;

	if (pid < 1)
		return 0;

	for (i = 0; i < MAXJOBS; i++) {
		if (jobs[i].pid == 0) {
			jobs[i].pid = pid;
			jobs[i].state = state;
			jobs[i].jid = nextjid++;
			if (nextjid > MAXJOBS)
				nextjid = 1;
			strcpy(jobs[i].cmdline, cmdline);
			if(verbose){
				printf("%s%s Added job [%d] %d %s\n", PROMPT, VB, jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
			}
			return 1;
		}
	}
	printf("Tried to create too many jobs\n");
	return 0;
}

/* deletejob - delete a job whose PID=pid from the job list */
int deletejob(struct job_t* jobs, pid_t pid) {
	int i;

	if (pid < 1)
		return 0;

	for (i = 0; i < MAXJOBS; i++) {
		if (jobs[i].pid == pid) {
			clearjob(&jobs[i]);
			nextjid = maxjid(jobs)+1;
			return 1;
		}
	}
	return 0;
}

/* fgpid - return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t* jobs) {
	int i;

	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].state == FG)
			return jobs[i].pid;
	return 0;
}

/* getjobpid  - find a job (by PID) on the job list */
struct job_t* getjobpid(struct job_t* jobs, pid_t pid) {
	int i;

	if (pid < 1)
		return NULL;
	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].pid == pid)
			return &jobs[i];
	return NULL;
}

/* getjobjid  - find a job (by JID) on the job list */
struct job_t* getjobjid(struct job_t* jobs, int jid) {
	int i;

	if (jid < 1)
		return NULL;
	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].jid == jid)
			return &jobs[i];
	return NULL;
}

/* pid2jid - map process ID to job ID */
int pid2jid(pid_t pid) {
	int i;

	if (pid < 1)
		return 0;
	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].pid == pid) {
			return jobs[i].jid;
		}
	return 0;
}

/* listjobs - print the job list */
void listjobs(struct job_t* jobs) {
	int i;

	for (i = 0; i < MAXJOBS; i++) {
		if (jobs[i].pid != 0) {
			printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
			switch (jobs[i].state) {
				case BG:
					printf("Running ");
					break;
				case FG:
					printf("Foreground ");
					break;
				case ST:
					printf("Stopped ");
					break;
				default:
					printf("listjobs: Internal error: job[%d].state=%d ",
					   i, jobs[i].state);
			}
			printf("%s\n", jobs[i].cmdline);
		}
	}
}

/* help message option */
void usage(void) {
    printf("Usage: shell [-hv]\n");
	printf("   -h   print this message\n");
	printf("   -v   print additional diagnostic information\n");
	exit(1);
}