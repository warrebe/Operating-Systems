/*
    # Course: CS 344
    # Author: Benjamin Warren
    # Date: 4/21/2022
    # Assignment: Assignment 1 - base64enc 
    # Description: - Base64 encode data and print to standard output, 
    		         encodes either binary data file, or user input at runtime
    # Requirements:
    base64enc encode FILE, or standard input, to standard output.
    With no FILE, or when FILE is -, read standard input.
    Encoded lines wrap every 76 characters.
    The data are encoded as described in the standard base64 alphabet in RFC 4648.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> // typedef uint8_t
// Check that uint8_t type exists
#ifndef UINT8_MAX
#error "No support for uint8_t"
#endif

static char const alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz"
                               "0123456789+/=";
                               

// Encodes data from input file
void encodeFile(FILE *in_file){
    int i = 0;
    uint8_t in[3], out[4];
    /*
        # Citation for the following function:
        # Date: 04/11/2022
        # Adapted from: Code published by instructor Ryan Gambord on Ed
        # Source URL: https://edstem.org/us/courses/21025/discussion/1382259
    */
    while(1){
        size_t nread = 0;
        i = i + 4;
        for(size_t offset = 0; offset < sizeof in;){
            size_t n = fread(in, sizeof(uint8_t), 3, in_file);
            if(n == 0){
                // end of input, partially filled input buffer
                break;
            }
            offset += n;
            nread = offset / sizeof *in;
        }
        if (nread == 0){
            if(i == 4){
                fprintf(stdout, "Error: file/stdin read fail");
            }
            // Reached eof and have no input to process
            break;
        }
        if(nread == 1){
            // Read returned only one byte - eof encountered
            out[0] = in[0] >> 2;
            out[1] = ((in[0] & 0x03) << 4) | (in[1] >> 8);
            fprintf(stdout, "%c%c==", alphabet[out[0]], alphabet[out[1]]);
        }
        else if(nread == 2){
            // Read returned only two bytes - eof encountered
            out[0] = in[0] >> 2;
            out[1] = ((in[0] & 0x03) << 4) | (in[1] >> 4);
            out[2] = ((in[1] & 0x0F) << 2) | (in[2] >> 8);
            fprintf(stdout, "%c%c%c=", alphabet[out[0]], alphabet[out[1]], alphabet[out[2]]);
        }
        else{
            // Read returned the appropriate 3 bytes
            out[0] = in[0] >> 2;
            out[1] = ((in[0] & 0x03) << 4) | (in[1] >> 4);
            out[2] = ((in[1] & 0x0F) << 2) | (in[2] >> 6);
            out[3] = in[2] & 0x3F;
            for(int j = 0; j < 4; ++j){
                fprintf(stdout, "%c", alphabet[out[j]]);
            }
        }
        if((i % 76) == 0){
            fprintf(stdout, "\n");
        }
    }
    fprintf(stdout, "\n");
}

int main(int argc, char *argv[]) {
    /*
        # Citation for the following function:
        # Date: 04/11/2022
        # Adapted from: Code published by instructor Ryan Gambord on teams
    */
    FILE *in_file = stdin; // Get file or stdin when no file provided as arg
    if(argc > 2){
        fprintf(stderr, "Error: invalid number of arguments\n");
        return -1;
    }
    if(argc > 1 && *argv[1] != '-'){
        in_file = fopen(argv[1],"r");
        if(in_file == NULL) {
            fprintf(stderr, "ERROR: cannot open input file\n");
            return -1;
        }
    }
    encodeFile(in_file);
    fclose(in_file);
    return 0;
}