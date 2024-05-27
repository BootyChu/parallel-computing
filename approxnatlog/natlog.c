/******************************************************************************
Title : natlog.c
Author : Anton Ha
Created on : March 2, 2023

Description : 
Calculate the natural log of a given valid value. We calculate this value 
through the approximate_ln() in parallel, it divides the tasks cyclically
among the processors to balance computation load. The function calculates the 
optimal width (dx), then loops through the user given valid number of segments
calculating the midpoint and adding it to the processors sum, and multiplying
the sum by the width to get the area. Lastly, the root node will output the 
value thats being computed, the estimated value of ln,the error in 
computation to the actual ln value, and then the time of computation.
Usage : natlog
Build with: mpicc -Wall -g -o natlog natlog.c -lm
Execute with:
mpirun --use-hwthread-cpus natlog (computation#) (#ofsegments) 2> /dev/null
Modifications: March 7, 2023 (added error checking)
******************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "mpi.h"

#define ROOT 0

double approximate_ln (int num_segments, int id, int p, int upper);
/**
 * @param: int number of segments, int processors' id, number of processors,
 * value of ln computation
 *
 * @brief: In order to calculate ln, we know that ln is the area from 1 to the 
 * upper value under the curve 1/x. Using this, I was able to use the rectangle
 * rule, to calculate the optimal width of computation, and loop through the 
 * number of segments to find the height at a certain index thats added to 
 * the variable "sum", then finally return the area by computing the total sum
 * of heights and width. Finally getting the area under the curve "1/x" from
 * 1 to "upper", which is also known as natural log of "upper".
 * 
 * @return: double of the approximate value of ln of int upper
*/

void input_validation(char* input,int id);
/**
 * @param: string input, int processor id
 *
 * @brief: Checks the input to be a numerical and if the number
 * greater than or equal to one. If the input is not numerical, 
 * or the number is less 1, it will print a usage error from
 * root processorand exit the program
 * 
*/

void print_error(char* error_message, int id);
/**
 * @param: string input, int id
 *
 * @brief: prints the error message to the user from the
 * root processor and exit the program safely.
 * 
*/

int main(int argc, char *argv[]){
  
    //validate the correct number of command line arguments was given.

    int id;             /* rank of executing process   */
    int p;              /* number of processes         */
    double ln_estimate; //estimate of the ln at a given value
    double local_ln; //each process's contribution
    double error; //calculate error between estimate and actual value
    double elapsed_time; //calculate elapsed time of total computation time

    MPI_Init(&argc, &argv);
    MPI_Comm_rank( MPI_COMM_WORLD, &id );
    MPI_Comm_size (MPI_COMM_WORLD, &p);

    if(3 != argc){
        //throw error for invalid command line arguments
        print_error("Insufficient command line arguments!",id);
    }else{
        //check the two given command line arguments
        input_validation(argv[1],id);
        input_validation(argv[2],id);
    }

    double computing_number = atof(argv[1]); //# of ln computation
    int num_segments = atof(argv[2]);     /* numbers of terms in series   */

    //START TIMER!
    MPI_Barrier(MPI_COMM_WORLD);
    elapsed_time = - MPI_Wtime();

    local_ln = approximate_ln(num_segments, id, p,computing_number);

    MPI_Reduce(&local_ln,&ln_estimate, 
               1, MPI_DOUBLE, MPI_SUM,
                ROOT, MPI_COMM_WORLD);
    
    //END TIMER!
    MPI_Barrier(MPI_COMM_WORLD);
    elapsed_time += MPI_Wtime();

    if(ROOT == id){
	    error = log(computing_number) - ln_estimate;
        printf("%.16g\t%.16f\t%.16f\t%.6f seconds\n",
               computing_number,ln_estimate, error, elapsed_time);
        fflush(stdout);
    }
    MPI_Finalize();
    return 0;

}

double approximate_ln (int num_segments, int id, int p, int upper){
	
    double dx, midpoint;

    dx = ((double)upper - 1) / (double) num_segments;
    //calculate the optimal width to ensure equal calculation
    
    double sum = 0.0;

    //loop through the number of segments and add 1/(midpoint) to the sum
    for(int i = id + 1; i <= num_segments; i+=p){
        midpoint = 1 + dx*((double)i - 0.5);
        sum += 1/midpoint;
    }

    return dx * sum; //return the total area (width * total height)
}

void input_validation(char* input,int id){

    /*
    loop through the input string and check if its numerical
    by seeing current char is a number or a decimal point, and
    if it is a decimal point check if there exists one already
    before 
    */
    bool has_decimal = false;
    for(int i = 0; i < strlen(input); i++){
        if( '.' == input[i] ){
            if(has_decimal){
                print_error("Inputs have to be numerical!",id);
            }
            has_decimal = true;
        }
        else if( !isdigit(input[i]) ){
            print_error("Inputs is invalid!",id);
        }
    }
    if( atof(input) < 1 ){
        print_error("Inputs have to be greater than or equal to one!",id);
    }
}

void print_error(char* error_message, int id){
    if(ROOT == id){
        printf("%s\n",error_message);
        fflush(stdout);
    }
    MPI_Finalize();
    exit(EXIT_FAILURE);
}
