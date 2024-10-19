#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>
#include <sys/time.h>

#define SEED 12345678

//Generate random numbers
unsigned int numgen(unsigned int count, unsigned long int dest[])
{

  unsigned int i = 0;

  srandom(SEED);

  while(count--) {
    dest[i++] = random();
  }

  return i;
}

__host__
void errorexit(const char *s) {
    printf("\n%s",s);	
    exit(EXIT_FAILURE);	 	
}

__device__
unsigned long isPrime(unsigned long n)
{
  if (n <= 1)
		return true;
	for (unsigned long int i = 2; i <= sqrtf(n); i++)
		if (n % i == 0)
			return false;
    return true;
}

//Check if number is prime for current index index and increment result if so
__global__
void CheckHowManyPrimes(unsigned long numbers[], unsigned long long *primes, long size)
{
  int index = blockIdx.x * blockDim.x + threadIdx.x;

  if (index < size)
  {
    if (isPrime(numbers[index]))
    {
      atomicAdd(primes, 1);
    }
  }
}

int main(int argc, char **argv)
{

  //Get input
  Args ins__args;
  parseArgs(&ins__args, &argc, argv);
  long inputArgument = ins__args.arg;
  
  //Generate random numbers array
  unsigned long int *numbers = (unsigned long int *)malloc(inputArgument * sizeof(unsigned long int));
  numgen(inputArgument, numbers);

  struct timeval ins__tstart, ins__tstop;
  gettimeofday(&ins__tstart, NULL);


  unsigned long *d_numbers;
  unsigned long long *d_primes;
  unsigned long long primes = 0;
  
  //Move all the necessary data to the GPU mem
  cudaMalloc(&d_numbers, inputArgument * sizeof(unsigned long));
  cudaMalloc(&d_primes, sizeof(unsigned long long));
  cudaMemcpy(d_numbers, numbers, inputArgument * sizeof(unsigned long), cudaMemcpyHostToDevice);
  cudaMemcpy(d_primes, &primes, sizeof(unsigned long long), cudaMemcpyHostToDevice);
  
  //Run main calculations on GPU, grid and block size definitions
  int blockSize = 256;
  int gridSize = (inputArgument + blockSize - 1) / blockSize;
  CheckHowManyPrimes<<<gridSize, blockSize>>>(d_numbers, d_primes, inputArgument);
  
  //Get the result
  cudaMemcpy(&primes, d_primes, sizeof(unsigned long long), cudaMemcpyDeviceToHost);
  printf("Number of primes: %lld\n", primes);
	
  //Clear used GPU mem
  cudaFree(d_numbers);
  cudaFree(d_primes);

  free(numbers);

  //Synchronize CUDA computations
  gettimeofday(&ins__tstop, NULL);
  
  //Show elapsed time
  ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);
  
}
