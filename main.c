#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "ashell.h"
#include <unistd.h>


int main( int argc, char ** argv )
{
	MPI_Init(&argc, &argv );
	
	ashell_t s = ashell_init_from_env(NULL);
	
	printf("SH %p\n", s );
	
	sleep(10);
	
	ashell_release( s );
	
	MPI_Finalize();
	
	return 0;
}
