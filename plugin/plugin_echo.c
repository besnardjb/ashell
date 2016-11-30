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
#include <string.h>
#include <jansson.h>
#include "ashell.h"


int ashell_plugin_init(ashell_t shell , void** my_cxt )
{
		fprintf(stderr, "Loading plugin ECHO\n");
		
		ashell_echo( shell, "Hello from echo plugin" );
		//ashell_echo( shell, "Hello from echo plugin" );
		//ashell_echo( shell, "Hello from echo plugin" );
		//ashell_echo( shell, "Hello from echo plugin" );
		//ashell_echo( shell, "Hello from echo plugin" );
		//ashell_echo( shell, "Hello from echo plugin" );
		
}

int ashell_plugin_release(ashell_t shell , void** my_cxt )
{
		fprintf(stderr, "Releasing plugin ECHO\n");
}
