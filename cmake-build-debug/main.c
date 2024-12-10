#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
#include <string.h>
#define HISTORY_SIZE 10

char history[HISTORY_SIZE][MAX_LINE]; // Store last 10 commands
int historyCount = 0;

/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */


void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # of characters in the command line */
    i,      /* loop index for accessing inputBuffer array */
    start,  /* index where beginning of next command parameter is */
    ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO,inputBuffer,MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

    printf(">>%s<<",inputBuffer);
    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
            case ' ':
            case '\t' :               /* argument separators */
                if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */
                if (start != -1){
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default :             /* some other character */
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&'){
                    *background  = 1;
                    inputBuffer[i-1] = '\0';
                }
        } /* end of switch */
    }    /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */

    for (i = 0; i <= ct; i++)
        printf("args %d = %s\n",i,args[i]);
} /* end of setup routine */

void searchPathAndExecute(char *command, char *args[]) {
    char *path = getenv("PATH");
    char *dir = strtok(path, ":");
    char fullPath[MAX_LINE];

    while (dir != NULL) {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dir, command);
        if (access(fullPath, X_OK) == 0) { // Check if executable exists
            execv(fullPath, args);
            perror("execv"); // If execv fails, print error
            exit(1);
        }
        dir = strtok(NULL, ":");
    }
    fprintf(stderr, "Command not found: %s\n", command);
    exit(1);
}

//HISTORY
void add_to_history(const char *command) {
    snprintf(history[historyCount % HISTORY_SIZE], MAX_LINE, "%s", command);
    historyCount++;
}

void print_history() {
    int start = historyCount > HISTORY_SIZE ? historyCount - HISTORY_SIZE : 0;
    for (int i = start; i < historyCount; i++) {
        printf("%d %s\n", i - start, history[i % HISTORY_SIZE]);
    }
}

void execute_history_command(int index, char *args[]) {
    int start = historyCount > HISTORY_SIZE ? historyCount - HISTORY_SIZE : 0;
    if (index < 0 || index >= HISTORY_SIZE || index >= historyCount - start) {
        fprintf(stderr, "Invalid history index.\n");
        return;
    }
    char *command = history[(start + index) % HISTORY_SIZE];
    printf("Executing: %s\n", command);
    // Reparse and execute the command
    setup(command, args, &(int){0});
    pid_t pid = fork();
    if (pid == 0) {
        searchPathAndExecute(args[0], args);
    } else {
        waitpid(pid, NULL, 0);
    }
}

int main(void) {
    char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
    int background;             /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE / 2 + 1]; /* command line arguments */

    while (1) {
        background = 0;
        printf("myshell: ");
        fflush(stdout);
        setup(inputBuffer, args, &background); /* Parse input */

        if (args[0] == NULL) { // Empty command
            continue;
        }

        pid_t pid = fork(); // Create a new process
        if (pid < 0) {
            perror("fork failed");
            continue;
        }

        if (pid == 0) { // Child process
            searchPathAndExecute(args[0], args);
        } else { // Parent process
            if (background == 0) {
                waitpid(pid, NULL, 0); // Wait for the foreground process to finish
            } else {
                printf("[Background process started with PID %d]\n", pid);
            }
        }
    }
    return 0;
}