#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
#include <string.h>
#define HISTORY_SIZE 10
#define MAX_BG_PROCESSES 10

pid_t bgProcesses[MAX_BG_PROCESSES];

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

//    for (i = 0; i <= ct; i++)
//        printf("args %d = %s\n",i,args[i]);
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
    // Check if the command is already in history
    int start = historyCount > HISTORY_SIZE ? historyCount - HISTORY_SIZE : 0;
    for (int i = start; i < historyCount; i++) {
        if (strcmp(history[i % HISTORY_SIZE], command) == 0) {
            // Remove the duplicate by shifting all commands after it
            for (int j = i; j < historyCount - 1; j++) {
                snprintf(history[j % HISTORY_SIZE], MAX_LINE, "%s", history[(j + 1) % HISTORY_SIZE]);
            }
            historyCount--; // Decrement history count to reflect removal
            break;
        }
    }

    // Shift all commands in history to make space for the new command at index 0
    for (int i = HISTORY_SIZE - 1; i > 0; i--) {
        snprintf(history[i], MAX_LINE, "%s", history[i - 1]);
    }

    // Add the new command at index 0
    snprintf(history[0], MAX_LINE, "%s", command);

    // If historyCount is less than HISTORY_SIZE, increment it
    if (historyCount < HISTORY_SIZE) {
        historyCount++;
    }
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

    // Retrieve the command from history
    char command[MAX_LINE];
    snprintf(command, MAX_LINE, "%s", history[(start + index) % HISTORY_SIZE]);

    // Move the executed command to the top of the history
    add_to_history(command);

    // Print the command being executed
    printf("Executing: %s\n", command);

    // Parse the command into args
    char *parsedArgs[MAX_LINE / 2 + 1]; // Parsed arguments
    char *token = strtok(command, " ");
    int argCount = 0;
    while (token != NULL) {
        parsedArgs[argCount++] = token;
        token = strtok(NULL, " ");
    }
    parsedArgs[argCount] = NULL;

    // Execute the parsed command
    pid_t pid = fork();
    if (pid == 0) {
        searchPathAndExecute(parsedArgs[0], parsedArgs);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        perror("Fork failed");
    }
}


pid_t foregroundPid = 0;

///////
void handle_sigint(int sig) {
    if (foregroundPid > 0) {
        kill(-foregroundPid, SIGTERM); // Send SIGTERM to process group
        printf("Foreground process %d terminated.\n", foregroundPid);
        foregroundPid = 0;
    }
}

int bgCount = 0;

void add_background_process(pid_t pid) {
    if (bgCount < MAX_BG_PROCESSES) {
        bgProcesses[bgCount++] = pid;
    }
}

void remove_background_process(pid_t pid) {
    for (int i = 0; i < bgCount; i++) {
        if (bgProcesses[i] == pid) {
            bgProcesses[i] = bgProcesses[--bgCount]; // Replace with last
            break;
        }
    }
}

void move_to_foreground(int index) {
    if (index < 0 || index >= bgCount) {
        fprintf(stderr, "Invalid background process index.\n");
        return;
    }
    pid_t pid = bgProcesses[index];
    remove_background_process(pid);
    foregroundPid = pid;
    printf("Bringing process %d to foreground.\n", pid);
    waitpid(pid, NULL, 0);
    foregroundPid = 0;
}

void handle_exit() {
    // Check for background processes and wait for their termination
    for (int i = 0; i < bgCount; i++) {
        pid_t result = waitpid(bgProcesses[i], NULL, WNOHANG);
        if (result > 0) {
            // Process has terminated, remove from the list
            remove_background_process(bgProcesses[i]);
            i--; // Adjust index to recheck the shifted array
        }
    }

    // If there are still active background processes, prevent exit
    if (bgCount > 0) {
        printf("There are %d background processes still running. Please terminate them first.\n", bgCount);
    } else {
        printf("Exiting shell.\n");
        exit(0); // Exit when all processes are cleaned up
    }
}


int main(void) {
    char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
    int background;             /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE / 2 + 1]; /* command line arguments */
    signal(SIGTSTP, handle_sigint); // Handle ^Z

    while (1) {
        background = 0;
        printf("myshell: ");
        fflush(stdout);
        setup(inputBuffer, args, &background);

        // Built-in commands
        if (args[0] == NULL) continue; // Ignore empty input

        // Handle "exit"
        if (strcmp(args[0], "exit") == 0) {
            handle_exit();
            continue;
        }

        // Handle "history"
        if (strcmp(args[0], "history") == 0) {
            if (args[1] != NULL && strcmp(args[1], "-i") == 0 && args[2] != NULL) {
                int index = atoi(args[2]);
                execute_history_command(index, args);
            } else {
                print_history();
            }
            continue;
        }

        // Handle "fg %<num>"
        if (strcmp(args[0], "fg") == 0) {
            if (args[1] != NULL && args[1][0] == '%') {
                int index = atoi(&args[1][1]);
                move_to_foreground(index);
            } else {
                fprintf(stderr, "Usage: fg %%<num>\n");
            }
            continue;
        }

        // Add the command to history
        add_to_history(inputBuffer);

        // Process command execution
        pid_t pid = fork();
        if (pid < 0) {
            perror("Failed to fork");
        } else if (pid == 0) {
            // Child process: execute command
            searchPathAndExecute(args[0], args);
        } else {
            // Parent process
            if (background == 1) {
                printf("[Background process started with PID %d]\n", pid);
                add_background_process(pid);
            } else {
                foregroundPid = pid;
                waitpid(pid, NULL, 0);
                foregroundPid = 0;
            }
        }
    }
    return 0;
}