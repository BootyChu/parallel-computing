/******************************************************************************
Title : steady.c
Author : Anton Ha
Created on : April 8, 2023

Description : In this program, we are given a file that has 6 values, the 
dimensions of our grid, north, west, east, and south tempertatures of the grid.
Our duty is to print the steady temperature at given coordinates x and y of the
grid. To find out the steady temperature of the grid, we used monte carlo method
where at every point of the grid, we would compute the amount of random walks
from the point to any boundary. Once we hit a boundary, we will average in 
that boundary temperature into the point's temperature. We then compute the
difference of the original temp to the new temp, and if the max diff of every
point is less than or equal to the convergence threshold, our grid is now
steady and we print the value of the given coordinate. To parallelize this
method, I have decided to break up the grid into rows, giving each processes
its own number of rows to compute in the inner grid of the grid, and used an
all reduction to find the max difference of every point to find when the grid
has reached a steady state!

Usage : steady
Build with: 
mpicc -Wall -g -o steady steady.c
Execute with:
mpirun --use-hwthread-cpus steady <file name> <point x> <point y> 2> /dev/null
Modifications: April 14, 2024 (fixed output coordinate mix up)
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
#include <math.h>
#include <time.h>
#include "mpi.h"

typedef struct { /* A 2 D Point */
int x ;
int y ;
} point2d ;

//movements of the corresponding direction
const point2d East = { 1 , 0}; 
const point2d West = { -1 , 0};
const point2d North = { 0 , 1};
const point2d South = { 0 , -1};

#define ROOT 0
# define NORTH 1
# define EAST 2
# define SOUTH 3
# define WEST 4
# define CONVERGENCE_THRESHOLD 0.05


void print_error(char* error_message);
/**
 * @param: string input
 *
 * @brief: prints the error message to the user from the
 * root processor and exit the program safely.
 * 
*/

point2d next_dir ();
/**
 * @brief: returns a random direction
 *
 * @return: point2d of the random direction 
*/

int on_boundary ( point2d point , int width , int height );
/**
 * @param: point2d of the current point, int width of grid,
 * int height of the grid
 *
 * @brief: checks if the coordinates of the point is on the
 * boundary.
 * 
*/
point2d next_point ( point2d oldpoint , point2d direction );
/**
 * @param: point2d of the old point, point2d of the direction
 * it will move the old point by
 * 
 * @brief: moves old coordinate by the direction
 * 
 * @return: returns point2d of the new coordinate
*/

int number_of_checks(int id, int size, int p);
/**
 * @param: int id of the processes, int size of the
 * tasks to distribute, int number of processes
 * 
 * @brief: computes each processes number of checks
 * 
 * @return: int number of checks that process will do
*/
void print_boundary(int x, int y, double boundary_temp[], int height, int width);
/**
 * @param: int x coordinate, int y coordinate, array of 4 that contains the 
 * NESW temperature, int height of grid, int width of grid
 * 
 * @brief: handles edge case of when (x,y) is on the boundary, to stop unnecessary
 * computation of the inner grid of the grid, and prints the (x,y).
 * 
*/

int main(int argc, char *argv[]){

    int id;             //rank of executing process
    int p;              // number of processes

    int rows, cols; //number of rows / cols of the grid
    point2d current; //intialize current point
    double boundary_temp[4]; //array of NESW temperature
    int output_x, output_y; //coordinates of the output
    MPI_Status status; //info about the communication operation of send / recv

    MPI_Init(&argc, &argv); 
    MPI_Comm_rank( MPI_COMM_WORLD, &id );
    MPI_Comm_size (MPI_COMM_WORLD, &p);

    //seed into random to ensure each processes have it own unique random number
    srand(time(NULL) + id);
    
    if(ROOT == id){
        //check if valid amount of command line arguments
        if(4 != argc){
            char error_message[strlen(argv[0]) + 50]; //for error message
            sprintf(error_message, "Usage: %s <file name> <x> <y>", argv[0]);
            print_error(error_message);
        } else {
            //open the file
            FILE *file = fopen(argv[1],"r");
            if(file == NULL){
                print_error("Error opening file!");
            }
            //read the file data into its corresponding values and check
            if (fscanf(file, "%i %i", &rows, &cols) != 2) {
                print_error("error reading file inputs");
            }
            if (fscanf(file, "%lf %lf %lf %lf", &boundary_temp[0], &boundary_temp[1], 
                                        &boundary_temp[2], &boundary_temp[3]) != 4) {
                print_error("error reading file inputs");
            }
            //read the file data into its corresponding values
        
            if(rows <= 0 || cols <= 0){
                print_error("invalid rows / cols number");
            }
            //check if output coordinates are a valid number
            for(int i = 2; i < 4; i++){
                for(int j = 0; j < strlen(argv[i]);j++){
                    if(!isdigit(argv[i][j])){
                        print_error("output coordinates has to be an non negative integer");
                    }
                }
            }
            output_x = atoi(argv[2]);
            output_y = atoi(argv[3]);
            //validate output coordinates
            if(output_x < 0 || output_y < 0 || output_x >= rows || output_y >= cols){
                print_error("invalid point on the graph");
            }
            //if output coordinates is on the boundary, just print it and abort
            if(output_x == 0 || output_x == rows - 1 || output_y == 0 || output_y == cols - 1){
                print_boundary(output_x, output_y, boundary_temp, rows, cols);
            }
            //shift the coordinates to match with the inner grid (grid not including boundaries)
            output_x -= 1;
            output_y -= 1;
            fclose (file);
        }
    }
    //broadcast dimensios, output coords, and NESW temps
    MPI_Bcast(&rows, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&cols, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&output_x, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&output_y, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(&boundary_temp, 4, MPI_DOUBLE, ROOT, MPI_COMM_WORLD);

    //compute the process number of rows to check and allocate memory for the rows
    int check = number_of_checks(id, (rows - 2), p); //processes number of rows to check
    double** chunk = NULL; //corresponding chunk in the inner grid
    chunk = (double **)malloc(check * sizeof(double*)); //allocate number of rows its checking
    if (chunk == NULL) {
        print_error("Chunk memory allocation failed!");
    }
    if(chunk){
        //each row will have size cols - 2
        for(int i = 0; i < check; i++){
            chunk[i] = (double *)malloc((cols - 2) * sizeof(double));
            if (chunk[i] == NULL) {
                print_error("Chunk memory allocation failed!");
            }
        }
    }
    //intialize every point in the chunk to zero
    for (int i = 0; i < check; i++) {
        for (int j = 0; j < cols - 2; j++) {
            // Set specific initial values
            chunk[i][j] = 0.0;
        }
    }
    int count = 0; //number of times we checked every point in the grid
    int c_row = 0; //chunk row index
    int c_col = 0; //chunk col index
    int location; //location of the random walk
    double oldvalue = 0;
    double maxdiff; //max diff of the chunk
    double diff; //at a point of the difference of new value minus old value
    double global_max = 0; //max diff of the entire inner grid
    while( 1 ){
        //reset max diff
        maxdiff = 0;
        //reset chunk row / col index
        c_row = 0;
        c_col = 0;
        for(int i = id + 1; i < rows - 1; i += p){
            for(int j = 1; j < cols - 1; j++){
                //current is the coordinates of this iterations of the inner grid
                current.x = j;
                current.y = i;
                //keep random walking till we hit a boundary
                while(0 == (location = on_boundary(current, cols, rows))){
                    current = next_point(current,next_dir());
                }
                //store old value
                oldvalue = chunk[c_row][c_col];
                //compute new value by averaging in the boundary we hit into the old value
                chunk[c_row][c_col] = ( oldvalue * count + boundary_temp [ location - 1]) / (count + 1);
                //the difference of new - old
                diff = fabs(chunk[c_row][c_col] - oldvalue);
                //update maxdiff if diff is greater than max diff
                if(diff > maxdiff){
                    maxdiff = diff;
                }
                //increment chunk col index
                c_col += 1;
            }
            //increment chunk row index
            c_row += 1;
            //reset chunk col index
            c_col = 0;
        }
        //all reduce to find the global max diff of the entire inner grid
        MPI_Allreduce(&maxdiff, &global_max, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        //if break if we are with the convergence threshold else keep computing after incrementing count
        if(global_max <= CONVERGENCE_THRESHOLD){
            break;
        }else{
            count += 1;
        }
    }

    double output_value; //value to be printed out
    int id_has = (output_x % p); //which process id has the output value
    int output_chunk_row = output_x / p; //which row in the chunk has the output value
    int output_recv, output_send;
    if(id == ROOT){
        //check if root process has the output value to avoid self communication
        if(id_has != 0){
            //recieve the output value from the process id that has the output value
            output_recv = MPI_Recv(&output_value, 1, MPI_DOUBLE, id_has, 
                                    MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if(output_recv != MPI_SUCCESS){
                print_error("Error in recieving output value");
            }
            //print output to the terminal 
            printf("%.2lf\n",output_value);
        }else{
            //print output to the terminal
            printf("%.2lf\n",chunk[output_chunk_row][output_y]);
        }
    }else if(id == id_has){
        //send output value to root process
        output_send = MPI_Send(&chunk[output_chunk_row][output_y], 1, 
                                MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);
        if(output_send != MPI_SUCCESS){
            print_error("Error in sending output value");
        }
    }
    //free up the chunk space
    for(int i = 0; i < check; i++){
        free(chunk[i]);
    }
    free(chunk);
    MPI_Finalize();
    return 0;
}
void print_error(char* error_message){
    fprintf(stderr,"%s", error_message);
    fflush(stderr);
    MPI_Abort(MPI_COMM_WORLD, 1);
}
point2d next_dir ()
{
    int random = rand() % 4; //get random number
    //return random direction
    if ( random == 0 ) return North ;
    else if ( random == 1 ) return East ;
    else if ( random == 2 ) return South ;
    else return West ;
}
int on_boundary ( point2d point , int width , int height )
{
    //check if on the boundary, if it is return which boundary
    if ( 0 == point . x ) return WEST ;
    else if ( width -1 == point . x ) return EAST ;
    else if ( 0 == point . y ) return NORTH ;
    else if ( height - 1 == point . y ) return SOUTH ;
    else return 0;
}
point2d next_point ( point2d oldpoint , point2d direction )
{
    //return new point that was moved by direction of the old point
    point2d temp ;
    temp . x = oldpoint . x + direction . x ;
    temp . y = oldpoint . y + direction . y ;
    return temp ;
}
int number_of_checks(int id, int size, int p){
    int every = size / p; //every processes has atleast this many tasks
    int overload = size % p; //remaining tasks that is leftover
    if(id < overload){
        //give leftover tasks to these processes
        return every + 1;
    }else{
        //give the number of tasks every processes has to check
        return every;
    }
}
void print_boundary(int x, int y, double boundary_temp[], int height, int width){
    //handle edge cases of small grid
    double avg = 0; //handle edge case avg
    if(height == 1 && width == 1){ //handle case when the grid is a 1x1
        for(int i = 0; i < 4; i++){
            avg += boundary_temp[i];
        }
        avg /= 4;
        printf("%.2lf\n", avg);
        MPI_Abort(MPI_COMM_WORLD, MPI_SUCCESS);
    }
    else if(height == 1 && width == 2){
        //handle 1x2 grid
        if(y == 0){
            avg = ( boundary_temp [0] + boundary_temp[2] + boundary_temp [3]) / 3;
        }else{
            avg = ( boundary_temp [0] + boundary_temp[2] + boundary_temp [1]) / 3;
        }
        printf("%.2lf\n", avg);
        MPI_Abort(MPI_COMM_WORLD, MPI_SUCCESS);
    }
    else if(height == 2 && width == 1){
        //handle 2x1 grid
        if(x == 0){
            avg = ( boundary_temp [0] + boundary_temp[1] + boundary_temp [3]) / 3;
        }else{
            avg = ( boundary_temp [1] + boundary_temp[2] + boundary_temp [3]) / 3;
        }
        printf("%.2lf\n", avg);
        MPI_Abort(MPI_COMM_WORLD, MPI_SUCCESS);
    }
    //create the plate with the boundaries and corner filled in with their temps
    double ** plate = NULL;
    plate = (double **)malloc(height * sizeof(double *));
    if(plate == NULL){
        print_error("error allocing space for plate (boundary case)");
    }
    for (int i = 0; i < height; i++) {
        plate[i] = (double *)malloc(width * sizeof(double));
        if(plate[i] == NULL){
            print_error("error allocing space for plate (boundary case)");
        }
    }
    plate [0][0] = ( boundary_temp [0] + boundary_temp [3]) /2;
    plate [0][ width -1] = ( boundary_temp [0] + boundary_temp [1]) /2;
    plate [ height -1][0] = ( boundary_temp [3] + boundary_temp [2]) /2;
    plate [ height -1][ width -1] = ( boundary_temp [2] + boundary_temp [1]) /2;
    /* 2. Initialize the temperatures along the edges of the plate . */
    for ( int j = 1; j < width -1; j ++ ) {
        plate [0][ j ] = boundary_temp [0];
        plate [ height -1][ j ] = boundary_temp [2];
    }
    for ( int i = 1; i < height -1; i ++ ) {
        plate [ i ][0] = boundary_temp [3];
        plate [ i ][ width -1] = boundary_temp [1];
    }
    //print the output coordinate
    printf("%.2lf\n",plate[x][y]);
    //free the plate space
    for (int i = 0; i < height; i++) {
        free(plate[i]);
    }
    free(plate);
    MPI_Abort(MPI_COMM_WORLD, MPI_SUCCESS);
}
