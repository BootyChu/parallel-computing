#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Data to be scattered (only relevant on root process)
    int *sendbuf = NULL;
    int sendcount; // Number of elements to send to each process
    int *displs = NULL; // Displacements to specify where each segment starts in sendbuf

    // Receive buffer for each process
    int recvbuf;

    if (rank == 0) {
        // Allocate memory for send buffer and displacements array
        sendbuf = (int *)malloc(size * sizeof(int)); // Assuming size elements to be sent
        if (sendbuf == NULL) {
            perror("Memory allocation failed");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        displs = (int *)malloc(size * sizeof(int));
        if (displs == NULL) {
            perror("Memory allocation failed");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Initialize send buffer and displacements array
        for (int i = 0; i < size; i++) {
            sendbuf[i] = i; // Example data to be scattered
            displs[i] = i; // Displacements are the same as ranks
        }

        // Calculate the number of elements to send to each process
        sendcount = size; // In this example, each process gets 'size' elements
    }

    // Scatter data to all processes
    MPI_Scatterv(sendbuf, &sendcount, displs, MPI_INT, &recvbuf, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Print received data on each process
    printf("Process %d received data: %d\n", rank, recvbuf);

    // Clean up memory
    if (rank == 0) {
        free(sendbuf);
        free(displs);
    }

    MPI_Finalize();
    return 0;
}