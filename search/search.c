/******************************************************************************
Title : search.c
Author : Anton Ha
Created on : March 20, 2023

Description : We are given a pattern and file name in the command line
arguments that we will check where the pattern occurs in the file, and printing
the indexes where it occurs. To compute this in parallel, I decided to agglomorate
the file as fairly among the amount of processors I am using. The ROOT processor
handles all of the input and outputting, and distributing of the file. With the
processor's chunk of the file, it would be using a brute force method that will
check where the pattern occurs and send the results to the root processor. Where
the root processor will be printing it to the user.

Usage : search
Build with: 
mpicc -Wall -g -o search search.c
Execute with:
mpirun --use-hwthread-cpus search <pattern> <file_name> 2> /dev/null
Modifications: March 26, 2023 (implemented pattern validation)
******************************************************************************/
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
#include "mpi.h"

#define ROOT 0
long int check_pattern(int* res, int checking, int start_index, 
                       char* chunk, char* pattern);
/**
 * @param: array of int res, int number of checking, int starting index,
 *         string of its chunk, string of the pattern
 *
 * @brief: check_pattern will check if the pattern exist within the chunk,
 * we will keep track of the index in reference to the file of it inside res and
 * keep track of the number of times it occured, which will be the return val
 *
 * @return: long int of how many times pattern occured inside the chunk
 * 
*/
int number_of_char(int id, int file_size, int p);
/**
 * @param: int processor's id, int size of file, int number of processors 
 *
 * @brief: Using the formula, we properly calculate how many processor {id} will check
 * in its chunk to ensure we distribute the file as fairly among the processors
 * 
*/
void print_error(char* error_message);
/**
 * @param: string input
 *
 * @brief: prints the error message to the user from the
 * root processor and exit the program safely.
 * 
*/

int main(int argc, char *argv[]){

    int id;             //rank of executing process
    int p;              // number of processes

    char* pattern = NULL;  // pattern of the execution
    intmax_t pattern_length; //length of the pattern
    char* chunk = NULL; // each processor's unique chunk of the file

    int in_fd; //placeholder to get the size of the file
    intmax_t file_size; //hold size of file

    intmax_t local_check; 
    //the amount the processor is going to check for pattern in the chunk
    intmax_t start_index; 
    //placeholder to keep each processor's index of the chunk

    MPI_Status status; //info about the communication operation of send / recv

    MPI_Init(&argc, &argv); 
    MPI_Comm_rank( MPI_COMM_WORLD, &id );
    MPI_Comm_size (MPI_COMM_WORLD, &p);

    if(ROOT == id){
        //check if valid amount of command line arguments
        if(3 != argc){
            char error_message[strlen(argv[0]) + 50]; //for error message
            sprintf(error_message, "Usage: %s <pattern> <file name>", argv[0]);
            print_error(error_message);
        } else {
            //set up pattern
            char *temp = strdup(argv[1]);  // Use strdup to copy string
            if (temp == NULL) {
                print_error("Pattern memory allocation failed!");
            }
            pattern = (char *)malloc(strlen(temp) + 1 * sizeof(char));
            if (pattern == NULL) {
                print_error("Pattern memory allocation failed!");
            }
            int c = 0;
            for(int i = 0; i < strlen(temp); i++){
                if(temp[i] >= 32 && temp[i] <= 126){
                    //make sure only valid ascii characters
                    pattern[c] = temp[i];
                    c++;
                }
            }
            pattern_length = c;
            pattern[pattern_length] = '\0';
            
            //open the file
            if ( ( in_fd = open ( argv [2] , O_RDONLY ) ) == -1 ) {
                print_error("Couldn't read the file!");
            }
            file_size = lseek ( in_fd , 0 , SEEK_END ) ;
            close ( in_fd );
        }
    }
    MPI_Bcast(&pattern_length , 1 , MPI_INT , ROOT , MPI_COMM_WORLD );
    MPI_Bcast(&file_size, 1 , MPI_LONG , ROOT , MPI_COMM_WORLD );

    //find out how much space we need for the chunk, and allocate for pattern
    local_check = number_of_char( id, file_size, p ); 
    //the number of chars each processor will check
    if (id != ROOT) {
        pattern = (char *)malloc(pattern_length + 1 * sizeof(char));
        if (pattern == NULL) {
            print_error("Pattern memory allocation failed!");
        }
    }
    chunk = (char *)malloc((local_check + pattern_length + 1) * sizeof(char));
    if (chunk == NULL) {
        print_error("Chunk memory allocation failed!");
    }

    MPI_Bcast(pattern, pattern_length, MPI_CHAR , ROOT , MPI_COMM_WORLD );
    pattern[pattern_length] = '\0';

    int rec_chunk; //error checking of recieving chunk
    int rec_starting; //error checking of recieving starting
    if(ROOT == id){

        intmax_t num_elements = 0; 
        //num of elements so far read to keep track of indexes
        FILE *file = fopen(argv[2],"r");
        //read the file

        //if the min chunk size is less than pattern length,
        //dont need to check chunk for pattern else store the chunk
        if(num_elements + pattern_length > file_size){
            chunk[0] = '\0';
        }else{
            fread(chunk,sizeof(char), local_check + pattern_length - 1, file);
        }
        chunk[local_check + pattern_length - 1] = '\0'; //null terminate chunk

        start_index = num_elements; //starting index of the processor 0's chunk
        num_elements += local_check;
        
        intmax_t i_check;
        //keep track of processor[i]'s amount of char(s) checking in the chunk
        int send_starting; //error checking in sending starting
        int send_chunk; //error checking in sending chunk
        for(int i = 1; i < p; i++){
            i_check = number_of_char( i, file_size, p ); 
            //number of chars the processor will check
            fseek(file, num_elements, SEEK_SET); 
            //increment file pointer to the left index of the processor's chunk

            //if the min chunk size is less than pattern length,
            //dont need to check chunk for pattern else store the chunk
            char *temp_chunk = (char *)malloc((i_check + pattern_length + 1) * sizeof(char));
            if (temp_chunk == NULL) {
                print_error("Chunk memory allocation failed!");
            }
            if(num_elements + pattern_length > file_size){
                temp_chunk[0] = '\0';
            }else{
                fread(temp_chunk,sizeof(char), i_check + pattern_length - 1, file);
            }
            temp_chunk[i_check + pattern_length - 1] = '\0'; //null terminate chunk

            send_chunk = MPI_Send(temp_chunk, i_check + pattern_length, 
                                    MPI_CHAR, i, 1, MPI_COMM_WORLD);
            if (send_chunk != MPI_SUCCESS){
                print_error("Error in sending chunk!");
            }
            //send starting index of the corresponding chunk to processor i
            send_starting = MPI_Send(&num_elements, 1, 
                                        MPI_INT, i, 1, MPI_COMM_WORLD);
            if (send_starting != MPI_SUCCESS){
                print_error("Error in sending starting index!");
            }
            //increment num elements to keep track of elements we are going to check
            num_elements += i_check;
            free(temp_chunk);
        }
        fclose(file); //close file as its not needed anymore
    }
    else{
        //process of recieving its chunk and starting index
        rec_chunk = MPI_Recv(chunk, (local_check + pattern_length), MPI_CHAR,
                                     ROOT, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (rec_chunk != MPI_SUCCESS){
            print_error("error in receiving chunk");
        }
        rec_starting = MPI_Recv(&start_index, 1, MPI_INT, ROOT, 
                                MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (rec_starting != MPI_SUCCESS){
            print_error("error in receiving starting index");
        }
    }

    //local_res is the result array of the indexes where pattern occurs in its chunk
    int local_res[local_check];
    //valid is the number of valid indexes in the chunk
    int valid = check_pattern(local_res, local_check, start_index, 
                              chunk, pattern);
    int send_valid; //error checking in sending valid
    int send_local_res; //error checking in sending local_res
    if (id == ROOT) {
        //print processor 0's result array
        for (int j = 0; j < valid; j++) {
                printf("%i\n", local_res[j]);
        }

        int recv_valid; //error checking in recieving valid
        int recv_local_res; //error checking in recieving local res
        //increment thru the number of processors starting from 1
        //so processor 0 can print the local_res of each processor
        for (int i = 1; i < p; i++) {
            //recieving 'valid' of processor i
            recv_valid = MPI_Recv(&valid, 1, MPI_INT, i, 1, 
                                    MPI_COMM_WORLD, &status);
            if (recv_valid != MPI_SUCCESS){
                print_error("Error in recieving number of valid indexes in local res!");
            }
            //recieving local_res of processor i
            recv_local_res = MPI_Recv(local_res, valid, MPI_INT, 
                                      i, 1, MPI_COMM_WORLD, &status);
            if (recv_local_res != MPI_SUCCESS){
                print_error("Error in recieving local res array!");
            }
            //print the results to the terminal
            for (int j = 0; j < valid; j++) {
                printf("%i\n", local_res[j]);
            }
        }
    }else{
        //send processor 0 its valid
        send_valid = MPI_Send(&valid, 1, MPI_INT, ROOT,
                              1, MPI_COMM_WORLD);
        if (send_valid != MPI_SUCCESS){
            print_error("Error in sending number of valid indexes in local res!");
        }
        //send processor 0 its local_res
        send_local_res = MPI_Send(local_res, valid, MPI_INT, 
                                  ROOT, 1, MPI_COMM_WORLD);
        if (send_local_res != MPI_SUCCESS){
            print_error("Error in sending local res array!");
        }
    }
    free(pattern); //free pattern to prevent memory leaks
    free(chunk); //free chunk to prevent memory leaks
    MPI_Finalize();
    return 0;
}
void print_error(char* error_message){
    printf("%s\n",error_message);
    MPI_Abort(MPI_COMM_WORLD, 1);
}

int number_of_char(int id, int file_size, int p){
    return ( ( ( id + 1 ) * file_size ) / p ) -
           ( ( id * file_size ) / p );
}
long int check_pattern(int* res, int checking, int start_index, 
                       char* chunk, char* pattern){
    int count; //counter
    int valid = 0; //number of times pattern occur in chunk
    for(int i = 0; i < strlen(chunk); i++){
        count = 0; //keep track of how many times chunk[ptr] == pattern[ptr]

        for(int j = 0; j < strlen(pattern); j++){
            if(i + j >= strlen(chunk) || pattern[j] != chunk[i + j]){
                //break if i + j passes out of bounds or chunk[ptr1] != pattern[ptr2]
                break;
            }
            count ++;
        }

        if(count == strlen(pattern)){ 
            //if count == length of pattern, we have found an occurence!
            res[valid] = start_index; //store the index in regards to the file
            valid += 1;
        }
        start_index ++; 
        //keep the index in the file the same as the loops index of chunk
    }
    return valid;
}