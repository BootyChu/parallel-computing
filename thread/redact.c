/******************************************************************************
Title : redact.c
Author : Anton Ha
Created on : March 20, 2023

Description : We are checking through multi threaded programming if a pattern 
occurs in an input text, and if it does we redact it. To do this in parallel,
using p threads, I decided to agglomorate the file as fairly among the threads.
The main thread will handle all command line arguments, reading the input file
into memory, intializing flag array, and creation of threads. Each thread will
check if the pattern occurs in the file using brute force method. If there
is an occurence, to determine if this thread will redact the input text in
memory is decided by the flag array that will keep track of what thread altered
what part of the input text. Finally, it creates an output file of the given
output file name and the main thread write the results into the file. 

Usage : redact
Build with: 
gcc redact.c
Execute with:
./a.out <number of threads> <pattern> <input file> <output file>
Modifications: May 3, 2023 (added comments)
******************************************************************************/
#include <sys/stat.h>
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
#include <pthread.h>

typedef struct task_data{
    intmax_t first; //start index of checking
    intmax_t last; //last index of checking
    char* pattern;
    pthread_t thread_id;
    int file_size;
    char* text; //global input file text in memory
    int* flag; //mentoined below
    char redact_char; //corresponding redact char
    int id;
} task_data;
static char* input_text; //reading the input file into memory
static int* flag;
/*
flag is an array of int that is size of the input file and
when a pattern occurs in the input text, the thread will
flag the index with its id, to ensure we keep track of ranks alteration
*/

pthread_mutex_t update_mutex; //declare global mutex
pthread_barrier_t barrier; //declare thread barrier

void * check_pattern(void * thread_data);
/**
 * @param: void pointer of an array of task_data
 *
 * @brief: check_pattern is the multi threading process where each
 * thread will check its own specified chunk of the file if the
 * pattern occurs. If it does, the thread will replace the pattern
 * with its corresponding redact character
 * if overlap higher thread id will over right the lower thread id
 * 
*/

int main(int argc, char *argv[]){

    int num_threads;
    char* pattern;
    FILE* file; //file pointer to input file
    char* output_name; //output file name
    task_data *thread_data; //data of corresponding thread
    pthread_attr_t attr; 

    int in_fd; //placeholder to get the size of the file
    intmax_t file_size; //hold size of file
    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if (argc != 5){ 
        //if it doesnt have 5 number of command line args
        fprintf(stderr, "Usage: redact <number of threads> <pattern> <input file> <output file>\n");
        exit(1);
    }
    
    //check number of thread arg
    for(int i = 0; i < strlen(argv[1]);i ++){
        if(!isdigit(argv[1][i])){
            fprintf(stderr, "Provide a valid number of threads!\n");
            exit(1);
        }
    }
    num_threads = atoi(argv[1]);

    //handle pattern arg
    pattern = strdup(argv[2]);  // Use strdup to copy string

    //handles output file arg
    output_name = strdup(argv[4]);  // Use strdup to copy string

    //handle input file
    //get file size
    if ( ( in_fd = open ( argv [3] , O_RDONLY ) ) == -1 ) {
        fprintf(stderr, "Couldn't read the file!\n");
        exit(1);
    }
    file_size = lseek ( in_fd , 0 , SEEK_END ) ;
    close ( in_fd );

    file = fopen(argv[3],"r");
    input_text = (char *)malloc((file_size + 1) * sizeof(char));
    if(input_text == NULL){
        fprintf(stderr, "Failed to allocate memory for text!\n");
        exit(1);
    }
    fread(input_text, 1, file_size, file);
    input_text[file_size] = '\0';

    //handles flag
    flag = (int * )malloc(file_size * sizeof(int));
    if(flag == NULL){
        fprintf(stderr, "Failed to allocate memory!\n");
        exit(1);
    }
    //initialize every cell of flag to -1
    for(int i = 0; i < file_size; i++){
        flag[i] = -1;
    }

    thread_data = calloc(num_threads, sizeof(task_data));
    if(thread_data == NULL){
        fprintf(stderr, "error calloc thread data");
        exit(1);
    }
    //intialize barrier
    pthread_barrier_init(&barrier,NULL,num_threads);
    pthread_mutex_init(&update_mutex , NULL ) ;

    //redact string of thread where its corresponding char will be id % 64
    char* redact_string = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_ ";
    int retval;

    for(int i = 0; i < num_threads; i++){

        thread_data[i].first = (i * file_size) / num_threads;
        thread_data[i].last = ((i + 1) * file_size) / num_threads;
        thread_data[i].pattern = pattern;
        thread_data[i].file_size = file_size;
        thread_data[i].text = input_text;
        thread_data[i].flag = flag;
        thread_data[i].redact_char = redact_string[i % 64];
        thread_data[i].id = i;
        thread_data[i].text = input_text;

        retval = pthread_create(&(thread_data[i].thread_id),&attr,
                                check_pattern, (void *) &thread_data[i]);
        if(retval){
            fprintf(stderr, "error creating pthread");
            exit(-1);
        }   
    }   
    //join all threads
    for( int t = 0 ; t < num_threads ; t++) {
        pthread_join ( thread_data [ t ] . thread_id , ( void ** ) NULL ) ;
    }
    
    //write to output file
    FILE *output;
    output = fopen(output_name, "w");
    if(output == NULL){
        fprintf(stderr, "Failed to open output file!\n");
        exit(1);
    }
    //write to the output file
    fprintf(output,"%s",input_text);

    //close and free memory used
    fclose(output);
    fclose(file);
    free(input_text);
    free(thread_data);
    free(flag);
    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&update_mutex);
}


void * check_pattern(void * thread_data){
    task_data *t_data = (task_data*) thread_data;

    char redact_char = t_data->redact_char;
    char* pattern = t_data->pattern;
    int pattern_length = strlen(pattern);
    int chunk_length = (t_data->last - t_data->first) + strlen(pattern);
    char* chunk = (char *)malloc((chunk_length + 1) * sizeof(char));
    if(chunk == NULL){
        fprintf(stderr, "error allocating memory of chunk");
        exit(-1);
    }

    //checks if each thread has a valid section length it has to check
    //also checks if the last index of the chunk is in within bounds
    if(t_data->last - t_data->first > 0 && t_data->last + pattern_length - 2 < t_data->file_size){
        strncpy(chunk,input_text + t_data->first, chunk_length);
        chunk[chunk_length] = '\0';
    }else{
        chunk[0] = '\0';
    }   
    //barrier to ensure thread doesnt copy manipulated data
    pthread_barrier_wait(&barrier);

    int count; 
    int start_index = t_data->first;
    for(int i = 0; i < strlen(chunk); i++){
        count = 0; //keep track of how many times chunk[ptr] == pattern[ptr]
        for(int j = 0; j < strlen(pattern); j++){
            if(i + j >= strlen(chunk) || pattern[j] != chunk[i + j]){
                //break if i + j passes out of bounds or chunk[ptr1] != pattern[ptr2]
                break;
            }
            count ++;
        }
        if(count == pattern_length){ 
            //we found a pattern occurence
            pthread_mutex_lock (&update_mutex);
            for(int k = start_index; k < start_index + pattern_length; k++){
                if(flag[k] == -1){
                    //from the start index to pattern length we replace it with redact char if section hasnt
                    //been manipulated with 
                    (t_data->text)[k] = redact_char;
                    flag[k] = t_data->id;
                }else{
                    //if text been manipulated with we check 
                    //if the previous manipulator's id is lower than current thread id
                    //we change current index of the input text to current redact char and
                    //update manipulator's id to current id
                    if(flag[k] < t_data->id){
                        (t_data->text)[k] = redact_char;
                        flag[k] = t_data->id;
                    }
                }
            }
            pthread_mutex_unlock (&update_mutex);
        }
        start_index ++; 
    }
    free(chunk);
    pthread_exit((void*)0);
}