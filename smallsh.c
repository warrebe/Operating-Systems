/*
    # Course: CS 344
    # Author: Benjamin Warren
    # Date: 5/09/2022
    # Assignment: Assignment 3 - smallsh
    # Description: Small shell program that emulates BASH
    # Requirements:
    Provide a prompt for running commands
    Handle blank lines and comments, which are lines beginning with the # character
    Provide expansion for the variable $$
    Execute 3 commands exit, cd, and status via code built into the shell
    Execute other commands by creating new processes using a function from the exec family of functions
    Support input and output redirection
    Support running commands in foreground and background processes
    Implement custom handlers for 2 signals, SIGINT and SIGTSTP
*/

// Define POSIX for signal handling if not pre-defined
// Must be done before include
#define _POSIX_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>

#define CLI_LENGTH 2048
#define MAX_ARGS 512
// Define home directory as starting directoy
#define HOME_DIR getenv("PWD")

// Define strdup()
extern char* strdup(const char*);

// Foreground only set to false at start
int foregroundOnly = 0;
//Status for most recent non-background process exited
int lastStatus = 0;

/*
Input struct for command line entry storage
*/
struct Input{ 
    char* args[MAX_ARGS];
    char inputFile[256];
    char outputFile[256];
    int background;
};

/* 
Signal handler for SIGSTP 
Taken from canvas template
*/
void handle_SIGTSTP(){
    if(foregroundOnly == 0){
        char* message = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 50);
        fflush(stdout);
        foregroundOnly = 1;
    }
    else{
        char* message = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30);
        fflush(stdout);
        foregroundOnly = 0;
    }
}

/*
Function for expading instances of $$ in arguments
*/
char* varExpansion(char* arg, char* pid){
    // Get first instance location of $$
    char* substring = strstr(arg, "$$");

    // No instances of $$
    if(substring == NULL){
        return NULL;
    }

    int subLen = strlen(substring) - 1;

    // Adapted from CodeVault's "Replace substrings"
    // Adjust space at $$ to size of pid
    memmove(substring + strlen(pid), substring + 2, subLen);
    // Copy the pid into the argument at the location of $$
    memcpy(substring, pid, strlen(pid));

    // Return non-NULL value
    return substring;
}

/*
Command Execution
*/
int runCmd(struct Input process, struct sigaction SIGINT_action){
    // Exit smallsh
    if(!strcmp(process.args[0], "exit")){
        return -1;
    }

    // CD Process
    else if(!strcmp(process.args[0], "cd")){
        //If no arg, go home
        if(process.args[1] == 0){
            if(chdir(HOME_DIR)){
                fprintf(stderr, "Error: could not switch to home directory\n");
                return 1;
            }
            else{
                printf("Went Home %s\n", HOME_DIR);
                fflush(stdout);
            }
        }
        else{
            char cwd[4096]; // Max pathname length for Linux
            char pathName[4096];
            // Add / before directory to change to if not absolute path
            if(process.args[1][0] != '/'){
                strcpy(pathName, strcat(getcwd(cwd, sizeof(cwd)), "//"));
                strcat(pathName, process.args[1]);
            }
            else{
                strcpy(pathName, process.args[1]);
            }

            if(chdir(pathName)){
                fprintf(stderr, "Error: could not switch to new directory\n");
                fflush(stdout);
                return 1;
            }
        }
        return 1;
    }
    // Status
    else if(!strcmp(process.args[0], "status")){
        printf("exit value %d\n", lastStatus);
        fflush(stdout);
    }
    // Everything else
    else{
        // Garbage value to initiate spawnpid
        pid_t spawnpid = -5;
        int childStatus;

        // If fork is successful, the value of spawnpid will be 0 in the child, the child's pid in the parent
        spawnpid = fork();
        int childPid = spawnpid;

        // Switch status mirrors canvas module
        switch (spawnpid){
            case -1:
            // Code in this branch will be exected by the parent when fork() fails and the creation of child process fails as well
                perror("fork() failed!");
                exit(1);
                break;

            case 0:
            // spawnpid is 0 in the child;
                // To not ruin os1
                alarm(600);

                // If input file is specified
                if(strlen(process.inputFile) != 0){
                    int sourceFD = open(process.inputFile, O_RDONLY);
                    if (sourceFD == -1) {
                        perror(process.inputFile); 
                        exit(1); 
                    }
                    // Written to terminal
                    printf("sourceFD == %d\n", sourceFD); 
                    fflush(stdout);
                    // Store file description for input to command
                    int result = dup2(sourceFD, 0);
                    if (result == -1) { 
                        perror("source dup2()"); 
                        exit(2); 
                    }
                }

                // If output file is specified
                if(strlen(process.outputFile) != 0){
                    int targetFD = open(process.outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                    if (targetFD == -1) {                      
                        perror(process.outputFile);
                        exit(1);
                    }
                    // Currently printf writes to the terminal
                    printf("The file descriptor for targetFD is %d\n", targetFD);
                    fflush(stdout);
                    // Use dup2 to point FD 1, i.e., standard output to targetFD
                    int result = dup2(targetFD, 1);
                    if (result == -1) {
                        perror("target dup2"); 
                        exit(2); 
                    }
                    // Now whatever we write to standard out will be written to targetFD
                }

                //Enable ^C signal for child process
                SIGINT_action.sa_handler = SIG_DFL;
                // Install signal handler
                sigaction(SIGINT, &SIGINT_action, NULL);
                //invoke execvp()
                execvp(process.args[0], (char* const*)process.args);
                perror(process.args[0]);
                exit(EXIT_FAILURE);

            default:
            // spawnpid is the pid of the child

                // if background character found and foreground only not enabled by ^Z
                if(process.background && !foregroundOnly){
                    // WNOHANG specified. If the child hasn't terminated, waitpid will immediately return with value 0
                    childPid = waitpid(childPid, &childStatus, WNOHANG);
                    printf("Background process ID: %d\n", spawnpid);
                    fflush(stdout);
                }
                // Background not specified or not permitted
                else{
            		childPid = waitpid(childPid, &childStatus, 0);
                    lastStatus = WEXITSTATUS(childStatus);
                }

            // Check to see if any background processes have finished
            // -1 to indicate any process, > 0 to indicate that a process that ended was found
            while((spawnpid = waitpid(-1, &childStatus, WNOHANG)) > 0){
                // How did the process finish?
                if(WIFEXITED(childStatus)){
                    printf("Child %d exited normally with status %d\n", spawnpid, WEXITSTATUS(childStatus));
                    fflush(stdout);
                } 
                else{
                    printf("Child %d exited abnormally due to signal %d\n", spawnpid, WTERMSIG(childStatus));
                    fflush(stdout);
                }
            }
        }
    }
    // No exit
    return 1;
}

/*
Main shell process
*/
int shell(){
    // Initialize SIGINT_action struct to be empty ^C
	struct sigaction SIGINT_action = {0};

	// Fill out the SIGINT_action struct
	// Register SIG_IGN as handler to ignore ^C
	SIGINT_action.sa_handler = SIG_IGN;
	// Block all catchable signals while handle_SIGINT is running
	sigfillset(&SIGINT_action.sa_mask);
	// No flags set
	SIGINT_action.sa_flags = 0;

	// Install our signal handler
	sigaction(SIGINT, &SIGINT_action, NULL);
    // Initialize SIGSTP_action struct to be empty ^Z
	struct sigaction SIGTSTP_action = {0};

	// Fill out the SIGTSTP_action struct
	// Register handle_SIGTSTP as handler to redirect ^Z
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	// Block all catchable signals while handle_SIGSTP is running
	sigfillset(&SIGTSTP_action.sa_mask);
	// No flags set
	SIGTSTP_action.sa_flags = 0;

	// Install our signal handler
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    //Command prompt
    fprintf(stdout, ": ");
    fflush(stdout);
    
    //Input buffer
    char input[CLI_LENGTH];
    memset(input, 0, sizeof(input));
    
    //Struct to hold the the process to be run informtion
    struct Input process;
    memset(process.inputFile, 0, sizeof(input));
    process.background = 0;
    memset(process.args, 0, sizeof(process.args));

    // Get user input
    fgets(input, CLI_LENGTH, stdin);
    input[strlen(input) - 1] = 0;

    // Ignore line with # or empty input
    if(input[0] == '#' || (!strcmp(input, ""))){
        return 1;
    }

    pid_t pid = getpid();
    // Convert PID to string, PID max for BASH is 32768 (5 spaces) + 1 for NULL
    char pidStr[6];
    memset(pidStr, 0, sizeof(pidStr));
    sprintf(pidStr, "%d", pid);
    
    // Tokenize input into space delimited portions
    char* token = strtok(input, " ");
    int j = 0;
    while(token != NULL){
        // Background cahracter in input, remove and switch background to true
        if(!strncmp(token, "&", 1)){
            process.background = 1;
        }
        // Input file specified
        else if(!strncmp(token, "<", 1)){
            token = strtok(NULL, " ");
            strcpy(process.inputFile, token);
        }
        // Output file specified
        else if(!strncmp(token, ">", 1)){
            token = strtok(NULL, " ");
            strcpy(process.outputFile, token);
        }
        else{
            process.args[j] = strdup(token);
            //Variable expansion for $$
            while(varExpansion(process.args[j], pidStr));
        }
        ++j;
        token = strtok(NULL, " ");
    }

    process.args[j] = 0;

    // Call to run command
    int ret = runCmd(process, SIGINT_action);
    for(int i = 0; process.args[i]; ++i){
        free(process.args[i]);
    }
    // If return is -1, exit has been called
    if(ret == -1){
        return -1;
    }
    // Else run it again
    else{
        return 1;
    }
}

/*
Main Loop
*/
int main(){
    while(1){
        int status = shell();

        if(status == -1){
            break;
        }
    }

    return 0;
}
