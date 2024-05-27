/*******************************************************************************
  Title          : send_recv_demo.c
  Author         : Stewart Weiss
  Created on     : January 15, 2014
  Description    : Hello world via point-to-point communication
  Purpose        : To show the principle of using MPI_Send and MPI_Recv
  Usage          : send_recv_demo
  Build with     : mpicc -Wall test.c
  Modifications  : 
 
*******************************************************************************/
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
void print_error(char* error_message){
    printf("%s\n",error_message);
}
int check_pattern(int* res, int checking, int start_index, char* chunk, char* pattern){
    int count;
    int valid = 0;
    for(int i = 0; i < strlen(chunk); i++){
        count = 0;
        for(int j = 0; j < strlen(pattern); j++){
            if(pattern[j] != chunk[i + j]){
                break;
            }
            count ++;
        }

        if(count == strlen(pattern)){
            res[valid] = start_index;
            valid += 1;
        }
        start_index ++;
    }
    return valid;
}

int main( int argc, char **argv )
{
    char* pattern = NULL;
    int in_fd;
    long int filesize;
    long int pattern_length;
    if(3 != argc){
        print_error("Insufficient command line arguments!");
    } else {
        pattern = strdup(argv[1]);  // Use strdup to copy string
        if (pattern == NULL) {
            print_error("Memory allocation failed!");
        }
        if ( ( in_fd = open ( argv [2] , O_RDONLY ) ) == -1 ) {
            print_error("Couldn't read the file!");
        }
        filesize = lseek ( in_fd , 0 , SEEK_END ) ;
        pattern_length = strlen(pattern);
    }
    // char * chunk;
    // chunk = malloc((filesize) * sizeof(char));
    // FILE *file = fopen("test.txt","r");
    // fread(chunk,sizeof(char), filesize, file);
    // printf("chunk: %s, file size: %li\n", chunk, filesize);
}