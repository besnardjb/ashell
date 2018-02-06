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



char * do_echo( json_t * data, json_t * ret )
{
       
    fprintf(stderr, "%s\n", json_dumps( data, 0) );

        json_t * edata = json_object_get( data, "txt");

        if( edata )
        {
                fprintf(stderr, "%s", json_string_value( edata ) ); 
        }
        else
        {
                fprintf(stderr, "Error no payload given to echo" ); 
        }


	return "OK";
}




int ashell_plugin_init(ashell_t shell , void** my_cxt )
{
		ashell_debug("Loading plugin ECHO\n");
		ashell_echo( shell, "Hello from echo plugin" );

	        ashell_register_command( shell, "echo", do_echo);
}

int ashell_plugin_release(ashell_t shell , void** my_cxt )
{
		ashell_debug("Releasing plugin ECHO\n");
}

char * ashell_plugin_name( ashell_plugin_t plugin )
{
	return "ECHO";
}

int ashell_plugin_data( ashell_t shell, ashell_plugin_t plugin, char * desc, void * data )
{
	printf("HERE we have desc %s data %p\n", desc, data );
	return 0;
}
