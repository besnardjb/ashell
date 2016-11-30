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
#include "ashell.h"
#include <sys/sysinfo.h>
#include <jansson.h>

int sysinfo(struct sysinfo *info);


char * read_sysinfo( json_t * data, json_t * ret )
{
	
	struct sysinfo info;
	
	if( sysinfo(&info) < 0 )
	{
		perror("sysinfo");
		return "ERROR : Could not read sysinfo";
	}

	json_t * uptime = json_integer(info.uptime);
	
	json_t * load = json_array();
	json_array_append_new( load, json_integer(info.loads[0]));
	json_array_append_new( load, json_integer(info.loads[1]));
	json_array_append_new( load, json_integer(info.loads[2]));

	json_t * totalram = json_integer(info.totalram);
	json_t * freeram = json_integer(info.freeram);
	
	json_t * sharedram = json_integer(info.sharedram);
	json_t * bufferram = json_integer(info.bufferram);
	
	json_t * totalswap = json_integer(info.totalswap);
	json_t * freeswap = json_integer(info.freeswap);
	
	json_t * procs = json_integer(info.procs);
	
	json_t * totalhigh = json_integer(info.totalhigh);
	json_t * freehigh = json_integer(info.freehigh);
	
	json_t * memunit = json_integer(info.mem_unit);


	json_object_set_new( ret , "uptime", uptime );
	json_object_set_new( ret , "load", load );
	json_object_set_new( ret , "totalram", totalram );
	json_object_set_new( ret , "freeram", freeram );
	json_object_set_new( ret , "sharedram", sharedram );
	json_object_set_new( ret , "bufferram", bufferram );
	json_object_set_new( ret , "totalswap", totalswap );
	json_object_set_new( ret , "freeswap", freeswap );
	json_object_set_new( ret , "procs", procs );
	json_object_set_new( ret , "totalhigh", totalhigh );
	json_object_set_new( ret , "freehigh", freehigh );
	json_object_set_new( ret , "memunit", memunit );

	return "OK";
}

int ashell_plugin_init(ashell_t shell , void** my_cxt )
{
	ashell_debug( "Loading plugin MEMORY\n");

	ashell_register_command( shell, "memory", read_sysinfo);

}

int ashell_plugin_release(ashell_t shell , void** my_cxt )
{
	ashell_debug("Releasing plugin MEMORY\n");
}
