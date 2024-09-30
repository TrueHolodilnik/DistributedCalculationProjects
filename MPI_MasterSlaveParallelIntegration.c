#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include "numgen.c"
#include <stdbool.h>

//Tags types
#define DATA 0
#define END 10
#define RESULT 50
#define FINISH 100

#define DEBUG

#define SUBRANGE_SIZE 50

// check is number prime
bool isPrime(unsigned long int n){
	if (n <= 1)
		return true;
	for (unsigned long int i = 2; i <= n / 2; i++)
		if (n % i == 0)
			return false;
    return true;
}

int isPrimeloop(unsigned long int *numbers, int size)
{
    int ans = 0;
    for (int i = 0; i < size; i++) {

	if (isPrime(numbers[i]))
		ans++;
    }
    return ans;
}

int main(int argc,char **argv) {

	Args ins__args;
	parseArgs(&ins__args, &argc, argv);

	//program input argument
	long inputArgument = ins__args.arg; 

	struct timeval ins__tstart, ins__tstop;

	int myrank, nproc;
	unsigned long int *numbers, *subrange, *newsubrange, result = 0;

    MPI_Request *requests;
	int *resulttemp;
    int requestcompleted;

	int range_number=1;
	int range[2];
	int i, flag;
	int subrange_size = SUBRANGE_SIZE;

	MPI_Status status;
  
	MPI_Init(&argc,&argv);

	// obtain my rank
	MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
	// and the number of processes
	MPI_Comm_size(MPI_COMM_WORLD,&nproc);

	if(!myrank){
		gettimeofday(&ins__tstart, NULL);

		numbers = (unsigned long int*)malloc(inputArgument * sizeof(unsigned long int));
		numgen(inputArgument, numbers);

		requests = (MPI_Request*)malloc(3 * (nproc - 1) * sizeof(MPI_Request));
		resulttemp = (int*)malloc((nproc - 1) * sizeof(int));

	}

	if (myrank == 0)
	{
		range[0] = 0;

		// first distribute some ranges to all slaves
		for (i = 1; i < nproc; i++) {
		
			range[1] = range[0] + SUBRANGE_SIZE;

			#ifdef DEBUG
				printf ("\nRange %d,%d to process %d", range[0], range[1], i);
				fflush (stdout);
			#endif

			// send it to process i
			MPI_Send (&(numbers[range[0]]), SUBRANGE_SIZE, MPI_UNSIGNED_LONG, i, DATA, MPI_COMM_WORLD);

			range[0] = range[1];
		}


		for (i = 0; i < 2 * (nproc - 1); i++) requests[i] = MPI_REQUEST_NULL;	// none active at this point
		for (i = 1; i < nproc; i++)
			MPI_Irecv(&(resulttemp[i - 1]), 1, MPI_INT, i, RESULT, MPI_COMM_WORLD, &(requests[i - 1]));

		for (i = 1; i < nproc; i++) {
			range[1] = range[0] + SUBRANGE_SIZE;

			#ifdef DEBUG
				printf ("\nRange %d,%d to process %d", range[0], range[1], i);
				fflush (stdout);
			#endif  

			// send it to process i
			MPI_Isend(&(numbers[range[0]]), SUBRANGE_SIZE, MPI_UNSIGNED_LONG, i, DATA, MPI_COMM_WORLD, &(requests[nproc - 2 + i]));

			range[0] = range[1];
		}

		while (range[1] < inputArgument) {
			// wait for completion of any of the requests
			MPI_Waitany(2 * nproc - 2, requests, &requestcompleted, MPI_STATUS_IGNORE);

			if (requestcompleted < (nproc - 1)) {
				result += resulttemp[requestcompleted];


				MPI_Wait(&(requests[nproc - 1 + requestcompleted]), MPI_STATUS_IGNORE);
				range[1] = range[0] + SUBRANGE_SIZE;

				if (range[1] > inputArgument) {
					range[1] = inputArgument;

					#ifdef DEBUG
						printf("\nfinal range last numbers %d,%d to process %d", range[0], range[1], requestcompleted + 1);
						fflush(stdout);
					#endif
					numbers[range[0] - 1] = range[1] - range[0];
					MPI_Isend(&(numbers[range[0] - 1]), range[1]-range[0], MPI_UNSIGNED_LONG, requestcompleted + 1, END, MPI_COMM_WORLD, &(requests[nproc - 1 + requestcompleted]));
					
					// now issue a corresponding recv
					MPI_Irecv(&(resulttemp[requestcompleted]), 1, MPI_INT, requestcompleted + 1, RESULT, MPI_COMM_WORLD, &(requests[requestcompleted]));
				}
				else {
					#ifdef DEBUG
						printf("\nnext range %d,%d to process %d", range[0], range[1], requestcompleted + 1);
						fflush(stdout);
					#endif

					MPI_Isend(&(numbers[range[0]]), SUBRANGE_SIZE, MPI_UNSIGNED_LONG, requestcompleted + 1, DATA, MPI_COMM_WORLD, &(requests[nproc - 1 + requestcompleted]));
					range[0] = range[1];

					// now issue a corresponding recv
					MPI_Irecv(&(resulttemp[requestcompleted]), 1, MPI_INT, requestcompleted + 1, RESULT, MPI_COMM_WORLD, &(requests[requestcompleted]));
				}
			}		
		}
	
		for (i = 1; i < nproc; i++) {
			MPI_Isend (NULL, 0, MPI_DOUBLE, i, FINISH, MPI_COMM_WORLD, &(requests[2 * nproc - 3 + i]));
		}
		
		// synchronize with slaves
		MPI_Waitall(3 * nproc - 3, requests, MPI_STATUSES_IGNORE);

		// now simply add the results
		for (i = 0; i < (nproc - 1); i++) {
			result += resulttemp[i];
		}

		// now receive results for the initial sends
		for (i = 0; i < (nproc - 1); i++) {
			MPI_Recv(&(resulttemp[i]), 1, MPI_INT, i + 1, RESULT, MPI_COMM_WORLD, &status);
			result += resulttemp[i];
		}

		// now display the result
		printf ("\nHi, I am process 0, the result is %ld\n", result);

	}
	else {				

		requests = (MPI_Request *) malloc (2 * sizeof (MPI_Request));
		requests[0] = requests[1] = MPI_REQUEST_NULL;
		resulttemp = (int*)malloc(2 * sizeof(int));
		subrange = (unsigned long int*)malloc(SUBRANGE_SIZE * sizeof(unsigned long int));
		newsubrange = (unsigned long int*)malloc(SUBRANGE_SIZE * sizeof(unsigned long int));

		// first receive the initial data
		MPI_Recv(subrange, SUBRANGE_SIZE, MPI_UNSIGNED_LONG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

		while (status.MPI_TAG != FINISH) {

			MPI_Irecv(newsubrange, SUBRANGE_SIZE, MPI_UNSIGNED_LONG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &(requests[0]));

			resulttemp[1] = isPrimeloop(subrange, subrange_size);

			range_number++;

			MPI_Wait(&(requests[1]), &status);
			MPI_Wait(requests, &status);

			//printf("\nStatus: %d %d", flag, status.MPI_TAG);
			resulttemp[0] = resulttemp[1];
			// and start sending the results back
			MPI_Isend(&resulttemp[0], 1, MPI_INT, 0, RESULT, MPI_COMM_WORLD, &(requests[1]));
			
			// determine if its end of arrat by tag
			if (status.MPI_TAG == DATA) {
				for (int i = 0; i < SUBRANGE_SIZE; i++) {
					subrange[i] = newsubrange[i];
				}
			}
			else if (status.MPI_TAG == END) {
				for (int i = 0; i < newsubrange[0]; i++) {
					subrange[i] = newsubrange[i + 1];
					subrange_size = newsubrange[0];
				}
			}
		}
		
		printf ("\nSlave %d done\n", myrank);
		// now finish sending the last results to the master
		MPI_Wait(&(requests[1]), MPI_STATUS_IGNORE);
	}

	if (!myrank) {
		gettimeofday(&ins__tstop, NULL);
		ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);
	}
  
	MPI_Finalize();
	return 0;
}

