/*
    # Course: CS 344
    # Author: Benjamin Warren
    # Date: 5/24/2022
    # Assignment: Assignment 4 - MultiThread 
    # Description: 
    # Requirements:
    Write a program that creates 4 threads to process input from standard input as follows

    Thread 1, called the Input Thread, reads in lines of characters from the standard input.
    Thread 2, called the Line Separator Thread, replaces every line separator in the input by a space.
    Thread 3, called the Plus Sign thread, replaces every pair of plus signs, i.e., "++", by a "^".
    Thread 4, called the Output Thread, write this processed data to standard output as lines of exactly 80 characters.

    Furthermore, in your program these 4 threads must communicate with each other using the Producer-Consumer approach. 
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

// Initialize buffers
char input[1000]; // For intiial input from user
char seperateLines[50000]; // For truncating input to remove newline charactes and replace with spaces
char plusReplaced[50000]; // Replaces inputs of ++ with ^
int lineCounter = 0; // Indicates number of output lines written
int STOP = 0;

// Initialize critical exclusion variable
pthread_mutex_t mutex;

// Initialize the condition variables
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t sub = PTHREAD_COND_INITIALIZER;
pthread_cond_t out = PTHREAD_COND_INITIALIZER;

/*
Function for replacing ++ with ^
*/
char* plusSignReplace(char* line){
    // Get first instance location of ++
    char* substring = strstr(seperateLines, "++");
    // No instances of ++
    if(substring == NULL){
        return NULL;
    }
    int subLen = strlen(substring) - 1;

    // Adapted from CodeVault's "Replace substrings"
    // Adjust space at ++ to size of ^
    memmove(substring + 1, substring + 2, subLen);
    // Copy ^ into the string at the location of ++
    memcpy(substring, "^", 1);

    // Return non-NULL value
    return substring;
}

/*
Function that formats and prints out, then trims the buffer to remove printed data
*/
void printFormat(char* replacedString){
    fprintf(stdout, "%.80s\n", replacedString);
    fflush(stdout);
    // Remove output from buffer
    memmove(replacedString, replacedString + 80, strlen(replacedString) - 79);
    memcpy(replacedString, "", 0);
    // Increment linecounter
    ++lineCounter;
}

/*
 Function that the input producer thread will run. Produce an input string. Put in the buffer only when there is space in the buffer. If the buffer is full, then wait until there is space in the buffer.
*/
void *inputThread(void *args){
    memset(input, 0, sizeof(input));
    while(1){
        // Lock mutex before checking
        pthread_mutex_lock(&mutex);
        while(strlen(input) > 0){
            // Buffer is full. Wait for the consumer to signal that the buffer is empty
            pthread_cond_wait(&empty, &mutex);
        }
        // Get user input
        fgets(input, 1000, stdin);
        input[strlen(input) - 1] = 0;
        if(!(strcmp(input, "STOP")) || lineCounter == 49){
            // STOP or output limit has been received
            pthread_cond_signal(&full);
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        // Signal to the consumer that the buffer is no longer empty
        pthread_cond_signal(&full);
        // Unlock the mutex
        pthread_mutex_unlock(&mutex);
    }
}

/*
 Function that the consumer input thread will run. Get strings from the buffer if the buffer is >= 80 char. If the buffer is < 80 char then wait until there is data in the buffer.
*/
void *lineSeparatorThread(void *args){
    memset(seperateLines, 0, sizeof(seperateLines));
    while(1){
        // Lock the mutex before checking if the input buffer has data
        pthread_mutex_lock(&mutex);
        while(strlen(input) == 0){
            // Buffer is empy
            pthread_cond_wait(&full, &mutex);
        }
        if(!strcmp(input, "STOP") || lineCounter == 49){
            break;
        }
        strcat(seperateLines, input);
        strcat(seperateLines, " ");
        memset(input, 0, sizeof(input));
        // Signal to plusSignThread that seperateLines buffer has input
        pthread_cond_signal(&sub);
        // Signal to inputThread that the input buffer has space
        pthread_cond_signal(&empty);
        // Unlock the mutex
        pthread_mutex_unlock(&mutex);
    }
    seperateLines[strlen(seperateLines)] = '\255';
    // Signal to plusSignThread that seperateLines buffer has input
    pthread_cond_signal(&sub);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void *plusSignThread(void *args){
    while(1){
        // Lock the mutex before checking if the input buffer has data
        pthread_mutex_lock(&mutex);
        // Wait for buffer to have input
        while(strlen(seperateLines) == 0){
            pthread_cond_wait(&sub, &mutex);
            break;
        }
        // Replace all instances of ++
        while(plusSignReplace(seperateLines));
        // Copy data to new buffer
        strcat(plusReplaced, seperateLines);
        memset(seperateLines, 0, sizeof(seperateLines));
        if(plusReplaced[strlen(plusReplaced) - 1] == '\255'){
            break;
        }
        // Signal to outputThread that the plusReplace buffer has input
        pthread_cond_signal(&out);
        // Unlock the mutex
        pthread_mutex_unlock(&mutex);
    }
    // Signal to outputThread that the plusReplace buffer has input
    pthread_cond_signal(&out);
    // Unlock the mutex
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void *outputThread(void *args){
    while(1){
        // Lock the mutex before checking if the input buffer has data
        pthread_mutex_lock(&mutex);
        // Wait for buffer to have input
        while(strlen(plusReplaced) == 0){
            pthread_cond_wait(&out, &mutex);
            break;
        }
        if(plusReplaced[strlen(plusReplaced) - 1] == '\255'){
            plusReplaced[strlen(plusReplaced) - 1] = 0;
            STOP = 1;
        }
        // Output lines of 80 characters
        while(strlen(plusReplaced) >= 80){
            printFormat(plusReplaced);
        }
        fflush(stdout);
        // Unlock the mutex
        pthread_mutex_unlock(&mutex);
        if(STOP){
            break;
        }
    }
    return NULL;
}

int main(void){
    /*
    Outline for producer consumer approach adapted from Conditional Variables learning module
    */
    // Initialize the mutex
    pthread_mutex_init(&mutex, NULL);

    // Create a thread and tell it to run the function
    pthread_t tid;
    pthread_create(&tid, NULL, inputThread, NULL);

    pthread_t tid2;
    pthread_create(&tid2, NULL, lineSeparatorThread, NULL);

    pthread_t tid3;
    pthread_create(&tid3, NULL, plusSignThread, NULL);

    pthread_t tid4;
    pthread_create(&tid4, NULL, outputThread, NULL);

    // Wait for the threads to finish
    pthread_join(tid, NULL); // Call after to run concurrently
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    pthread_join(tid4, NULL);

    // Destroy Mutex
    pthread_mutex_destroy(&mutex);

    return 0;
}
