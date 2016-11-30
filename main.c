 /***************************************************************************
  *  Copyright (c) ParaTools SAS, All rights reserved.                      *
  *                                                                         *
  *  This library is free software; you can redistribute it and/or          *
  *  modify it under the terms of the GNU Lesser General Public             *
  *  License as published by the Free Software Foundation; either           *
  *  version 3.0 of the License, or (at your option) any later version.     *
  *                                                                         *
  *  This library is distributed in the hope that it will be useful,        *
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
  *  Lesser General Public License for more details.                        *
  *                                                                         *
  *  You should have received a copy of the GNU Lesser General Public       *
  *  License along with this library.                                       *
  *                                                                         *
  *  AUTHORS:                                                               *
  *  	- Jean-Baptiste BESNARD  jbbesnard@paratools.fr                     *
  *                                                                         *
  ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "ashell.h"
#include <unistd.h>


int main( int argc, char ** argv )
{
	MPI_Init(&argc, &argv );
	
	ashell_t s = ashell_init_from_env(NULL);
	
	sleep(500);
	
	ashell_release( s );
	
	MPI_Finalize();
	
	return 0;
}
