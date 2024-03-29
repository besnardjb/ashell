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
#include <stdlib.h>
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <jansson.h>
#include <stdint.h>
#include <stdarg.h>
#include <mpi.h>

#include "ashell.h"


static int ashell_is_verbose = -1;

void ashell_debug(const char *fmt, ...)
{
	if( ashell_is_verbose == -1 )
	{
		char * envv = getenv("ASHELL_VERBOSE");

		if( envv )
		{
			ashell_is_verbose = 1;
		}
		else
		{
			ashell_is_verbose = 0;
		}
	}

	if( !ashell_is_verbose )
		return;

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}	

struct connection_manager
{
	char * host;
	int port;
	char * secret;
	
	void (*cb)(char *,void*);
	void * arg;
	
	char hostname[150];
	int listening_port;
	
	int listening_socket;
	pthread_t listening_thread;
	
	int outgoing_socket;
	pthread_t outgoing_thread;
	int id;
	
};


#define BUFF_SIZE (16 * 1024 * 1024)


static char linebuffer[BUFF_SIZE] = {0};

char * socket_readline( int socket )
{
	/* Is there a line in current buffer */
	int n;

	char *cr = strstr( linebuffer, "\n");
	
	if( cr )
	{
		*cr = '\0';
		//printf("GOT CR (%s)\n", linebuffer );
		char * ret = strdup( linebuffer );
		
		char * rest = strdup( cr + 1 );
		int len_rest = strlen( rest );
		
		if( len_rest )
		{
			memcpy( linebuffer, rest, len_rest );
			*( linebuffer + len_rest ) = '\0';
		}
		else
		{
			linebuffer[0] = '\0';
		}
		
		free( rest );
		
		return ret;
	}
	else if( socket != -1 )
	{
		
		int dest = strlen( linebuffer );

		if((n = recv(socket, linebuffer + dest, 1024, 0)) < 0)
		{
		  perror("recv()");
		  return (void*)0x1;
		}
		
		if( n == 0 )
		{
			perror("recv()");
			return (void *)0x1;
		}
		else
		{
			*(linebuffer + dest + n ) = '\0';
		}

		//printf("Getting DATA (%s)\n", linebuffer );

		return socket_readline( socket );
	}


	//printf("RET NULL SOKC %d (%s)\n", socket, linebuffer );
	return NULL;
}

void * client_loop( void *pcm )
{
	struct connection_manager *cm = (struct connection_manager *)pcm;
	
	while(1)
	{
		char * data = socket_readline( cm->outgoing_socket );
		
		if( data )
		{
			if( data == (void *)0x1 )
			{
				break;
			}
			else
			{
				(cm->cb)( data, cm->arg );
				free( data );
			}
		}

	}
	
	fprintf(stderr, "Client disconnected\n");
	
}


void * server_loop( void *pcm )
{
	struct connection_manager *cm = (struct connection_manager *)pcm;
	struct sockaddr_in cli_addr;
	int clilen = sizeof(struct sockaddr_in);
	
	while(1)
	{
		
		int new_fd = accept(cm->listening_socket, (struct sockaddr *)&cli_addr, &clilen);
		
		if (new_fd < 0)
		{
			//perror("ERROR on accept");
			break;
		}
		
		/* Now set incoming socket as outgoing socket */
		
		if( cm->outgoing_socket != -1 )
		{
			shutdown( cm->outgoing_socket, SHUT_RDWR );
			close( cm->outgoing_socket );
			cm->outgoing_socket = -1;
		}
		
		cm->outgoing_socket = new_fd;
		
		fprintf(stderr, "New client from shell\n");
		
		char reset[400];
		snprintf(reset, 400, "RESET %d\n", cm->id );
		
		/* Restore CTX */
		send(cm->outgoing_socket, reset, strlen(reset), 0);
		
		pthread_create( &cm->outgoing_thread, NULL, client_loop, (void*)cm);
	}
	
	fprintf(stderr, "Server leaving\n");
}



int connection_manager_connect( struct connection_manager *cm )
{
	if( cm->outgoing_socket != -1 )
	{
		/* Already up */
		return 0;
	}
	
	int sock;
	struct sockaddr_in server;

	//Create socket
	sock = socket(AF_INET , SOCK_STREAM , 0);
	if (sock == -1)
	{
		perror("Could not create socket");
		return 1;
	}

	struct hostent *he;
	struct in_addr **addr_list;
	
	he = gethostbyname(cm->host);
	if (he == NULL)
	{
	  fprintf (stderr, "Unknown host %s.\n", cm->host);
	  return 1;
	}

	server.sin_addr = *(struct in_addr *) he->h_addr;
	server.sin_family = AF_INET;
	server.sin_port = htons( cm->port );

	//Connect to remote server
	if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		perror("connect failed. Error");
		return 1;
	}

	cm->outgoing_socket = sock;
	
	int got_ack = 0;
	char buff[100];
	int n;
	
	do
	{
		char pwd[100];
		snprintf(pwd, 100, "PWD %s\n", cm->secret );
		/* Send AUTH */
		send(cm->outgoing_socket, pwd, strlen(pwd), 0);
		
		/* GET ACK */
		char * ack = socket_readline( cm->outgoing_socket );

		if( !ack )
		{
			printf("Error failled data exchange\n");
			return 1;
		}

		if( strstr(ack, "AUTHOK") )
		{
			got_ack =1;
		}


		free( ack );
	}while(!got_ack);
	
	/* READ ID */

	char * tid = socket_readline( cm->outgoing_socket );

	if( !tid )
	{
		printf("Error failled data exchange\n");
		return 1;
	}


	int id = atoi( tid + 3 );
	
	free( tid );
	
	
	if( cm->id == -1 )
	{
		/* First encounter */
		cm->id = id;
		char meta[500];
		int rank;
		MPI_Comm_rank(MPI_COMM_WORLD, &rank );
		snprintf(meta,500,"{\"cmd\":\"meta\", \"data\" : { \"rank\" : %d, \"desc\" : \"%s\", \"host\" : \"%s\", \"port\" : %d}}\n", rank, "", cm->hostname, cm->listening_port);
		send(cm->outgoing_socket, meta, strlen(meta), 0);
	}
	else
	{
		/* Reset configuration */
		char reset[100];
		snprintf(reset, 100, "RESET %d", cm->id );
		send(cm->outgoing_socket, reset, strlen(reset), 0);
	}
	
	
	pthread_create( &cm->outgoing_thread, NULL, client_loop, (void*)cm);
	
	
	return 0;
}


int connection_manager_listen( struct connection_manager *cm )
{
   struct sockaddr_in serv_addr;
   int  n;
   
   /* First call to socket() function */
   cm->listening_socket = socket(AF_INET, SOCK_STREAM, 0);
   
   if (cm->listening_socket < 0)
   {
      perror("ERROR opening socket");
      return 1;
   }
   
   /* Initialize socket structure */
   bzero((char *) &serv_addr, sizeof(serv_addr));
   
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(0);
   
   /* Now bind the host address using bind() call.*/
   if (bind(cm->listening_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
   {
      perror("ERROR on binding");
      return 1;
   }

   if( listen(cm->listening_socket,5) != 0 )
   {
      perror("ERROR on listen");
      return 1;
   }
   
	/* Retrieve listening port */
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	if (getsockname(cm->listening_socket, (struct sockaddr *)&sin, &len) == -1)
	{
		perror("getsockname");
		return 1;
	}
	else
	{
		cm->listening_port = ntohs(sin.sin_port);
	}
	
	if( gethostname( cm->hostname, 150 ) != 0 )
	{
		return 1;
	}


	if( pthread_create(&cm->listening_thread, NULL, server_loop, (void *)cm) != 0 )
	{
		return 1;
	}

   return 0;
}

int connection_manager_send( struct connection_manager * cm , char * data )
{
	if( cm->outgoing_socket == -1 )
	{
		if( connection_manager_connect( cm ) )
		{
			fprintf(stderr, "Could not connect to server\n");
			return 1;
		}
	}
	
	if( cm->outgoing_socket == -1  )
	{
		fprintf(stderr, "Error socket not set\n");
		return 1;
	}
	
	send(cm->outgoing_socket, data, strlen(data), 0);
	
	char newline[2]= "\n";
	send(cm->outgoing_socket, newline, strlen(newline), 0);	
	
	return 0;
}


int connection_manager_init( struct connection_manager * cm , char * host, int port , char * secret , void (*cb)(char *, void *), void * arg)
{	
	cm->host = strdup( host );
	cm->port = port;
	cm->secret = strdup( secret );
	
	cm->listening_socket = -1;
	cm->outgoing_socket = -1;
	cm->id = -1;
	
	if( !cb )
	{
		fprintf(stderr, "Error no callback provided\n");
		return 1;
	}
	
	cm->cb = cb;
	cm->arg = arg;
	
	
	if( connection_manager_listen( cm ) )
	{
		return 1;
	}
	
			
	if( connection_manager_connect( cm ) )
	{
		return 1;
	}

	
	return 0;
}


int connection_manager_release( struct connection_manager * cm )
{

	if( cm->listening_socket != -1 )
	{
		shutdown( cm->listening_socket, SHUT_RDWR );
		close( cm->listening_socket );
		cm->listening_socket = -1;
	}

	if( cm->outgoing_socket != -1 )
	{
		char * cl = "CLOSE\n";
		send(cm->outgoing_socket, cl, strlen(cl), 0);
		
		shutdown( cm->outgoing_socket, SHUT_RDWR );
		close( cm->outgoing_socket );
		cm->outgoing_socket = -1;
	}
	
	pthread_join( cm->outgoing_thread, NULL );
	pthread_join( cm->listening_thread, NULL );
	
	return 0;
}



struct xashell_plugin
{
	char * name;
	char * path;
	void * storage;
	int (*plugin_init)( ashell_t shell , ashell_plugin_t plugin );
	char * (*plugin_name)( ashell_plugin_t plugin );
	int (*plugin_release)( ashell_t shell , ashell_plugin_t plugin );
	int (*plugin_data)( ashell_t shell, ashell_plugin_t plugin, char * desc, void * data );
	struct xashell_plugin * next;
};

struct xashell_plugin * xashell_plugin_new( char * path )
{
	struct xashell_plugin * ret = malloc( sizeof( struct xashell_plugin) );
	
	if( !ret )
	{
		perror("malloc");
		return NULL;
	}
	
	ret->path = strdup( path );
	
	void * handle = dlopen( path, RTLD_LAZY );
	
	if( handle == NULL )
	{
			free( ret->path );
			free(ret);
			return NULL;
	}
	
	ret->plugin_init = (int (*)(ashell_t,ashell_plugin_t))dlsym( handle, "ashell_plugin_init");
	ret->plugin_release = (int (*)(ashell_t,ashell_plugin_t))dlsym( handle, "ashell_plugin_release");
	ret->plugin_name = (char * (*)(ashell_plugin_t))dlsym( handle, "ashell_plugin_name");
	ret->plugin_data = (int (*)(ashell_t,ashell_plugin_t,char *, void *))dlsym( handle, "ashell_plugin_data");
	
	if(!ret->plugin_init 
	|| !ret->plugin_release
	|| !ret->plugin_name 
	|| !ret->plugin_data )
	{
		fprintf(stderr, "Could not find all required symbols in target plugin\n"
						"Plugin : %s", path);
		free( ret->path );
		free(ret);
		return NULL;
	}

	ret->name = ret->plugin_name((ashell_plugin_t)ret);

	return ret;
}


struct xashell_plugins
{
	struct xashell_plugin *plugins;
	int plugin_count;
};

int xashell_plugins_init( struct xashell_plugins *pls , char * prefix )
{
	struct dirent *dir;
	DIR * d = opendir( prefix );
	
	pls->plugins = NULL;
	pls->plugin_count = 0;
	
	if( !d )
	{
		perror("Failled to open prefix");
		return 1;
	}
	
	while( dir = readdir( d ) )
	{
	
		if( dir == NULL )
		{
			break;
		}
		
		if( !strstr( dir->d_name, ".so" ) )
		{
			continue;
		}
	
		char path[500];
		snprintf(path, 500, "%s/%s", prefix, dir->d_name );
		struct xashell_plugin * new_p = xashell_plugin_new( path );
	
		if( new_p )
		{
			new_p->next = pls->plugins;
			pls->plugins = new_p;
			pls->plugin_count++;
		}
	}

	ashell_debug( "aShell : Successfully loaded %d aShell plugins\n", pls->plugin_count);
	
	closedir( d );
	
	return 0;
}

struct xashell;

int xashell_plugins_call_init( struct xashell_plugins *pls ,  struct xashell * shell )
{
	struct xashell_plugin * tmp = pls->plugins;
	
	while(tmp)
	{
		(tmp->plugin_init)( shell, &tmp->storage );
		tmp = tmp->next;
	}
	
	return 0;
}

int xashell_plugins_call_release( struct xashell_plugins *pls ,  struct xashell * shell )
{
	struct xashell_plugin * tmp = pls->plugins;
	
	while(tmp)
	{
		(tmp->plugin_release)( shell, &tmp->storage );
		tmp = tmp->next;
	}
	
	return 0;
}

struct xashell_plugin * xashell_plugins_get_by_name( struct xashell_plugins *pls , char *name )
{
	struct xashell_plugin * tmp = pls->plugins;
	
	while(tmp)
	{
		if( !strcmp(name, tmp->name))
			return tmp;
		tmp = tmp->next;
	}
	
	return NULL;
}



struct xashell_command
{
	char * (*callback)( json_t * data, json_t * ret );
	char * name;
	struct xashell_command * next;
};


struct xashell_command * xashell_command_new( char * name, char * (*callback)( json_t * data, json_t * ret ) )
{
	struct xashell_command * ret = malloc( sizeof(struct xashell_command));
	
	if( !ret )
	{
		perror("malloc");
		return NULL;
	}
	
	ret->name = strdup( name );
	ret->callback = callback;

	return ret;
}


struct xashell_commands
{
	struct xashell_command * commands;
};


int xashell_commands_init( struct xashell_commands * cmds )
{
	cmds->commands = NULL;
	return 0;
}


 struct xashell_command * xashell_commands_get( struct xashell_commands * cmds , const char *name )
{
	/* Push new command front */
	struct xashell_command *tmp = cmds->commands;
	
	while( tmp )
	{
		if( !strcmp(tmp->name, name ) )
		{
			return tmp;
		}
	
		tmp = tmp->next;
	}
	
	return NULL;
}



int xashell_commands_push( struct xashell_commands * cmds, struct xashell_command * cmd )
{
	/* Push new command front */
	cmd->next = cmds->commands;
	cmds->commands = cmd;
	
	return 0;
}


struct xashell
{
	struct connection_manager cm;
	struct xashell_plugins plugins;
	struct xashell_commands cmds;
	char * plugin_prefix;
	json_t * command_buffer;
	
};



static unsigned int ref_id = 1;

json_t * xashell_command_to_shell( struct xashell * shell, char * cmd , json_t * data)
{
	int rank;
	MPI_Comm_rank( MPI_COMM_WORLD, &rank );
	uint64_t ref_id64 = ((uint64_t)rank << 32 ) | ref_id++;
	char ref[64];
	snprintf( ref, 64, "%llu", (long long unsigned int)ref_id64 );
	
	json_t * jcmd = json_object();
	json_t * jcmds = json_string(cmd);
	json_t * jref_id = json_string(ref);
	
	json_object_set(jcmd, "cmd", jcmds );
	json_object_set(jcmd, "data", data );
	json_object_set(jcmd, "refid", jref_id );

	char * command = json_dumps(jcmd, JSON_COMPACT);

	json_decref(jcmds);	
	json_decref(jcmd);

	int ret = connection_manager_send( &shell->cm , command );
	
	free( command );
	
	if( ret )
	{
		return json_null();
	}
	
	/* Wait for anwser */
	
	json_t * jret = NULL;
	
	int count = 0;
	
	while( (jret = json_object_get( shell->command_buffer, ref) ) == NULL )
	{
		count++;
		
		if( 1e6 < count )
		{
			fprintf(stderr,"Error timemout while waiting for command result\n");
			json_t * jret = json_null();
			break;
		}
		
		usleep(1500);
	}
	
	if( jret )
	{
		json_incref( jret );
		json_object_del( shell->command_buffer, ref );
	}
	
	
	return jret;
}




void xashell_incoming_command( char * data, void *ps)
{
	struct xashell * xsh = (struct xashell *)ps;
	
	char * cr = strstr(data,"\n");
	
	if( cr )
	{
		*cr = '\0';
	}
	
	if( strlen( data ) == 0 )
	{
		return;
	}
	
		
	
	json_t *root;
    json_error_t error;
	
	root = json_loads(data, 0, &error);

	if(!root)
	{
		fprintf(stderr, "JSON : %s\n", data);
		fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
		return;
	}
	
	if(!json_is_object(root))
	{
		fprintf(stderr, "error: incomming command is not an object\n");
		json_decref(root);
		return;
	}
	
	/* Is it an anwser ? */
	
	json_t * ref_id = json_object_get(root, "refid" );
	
	if( ref_id )
	{
		if(  json_is_string( ref_id ) )
		{
			json_object_set( xsh->command_buffer, json_string_value( ref_id ), root );
			return;
		}
	}
	
	/* So it is a command ? */
	
	json_t * cmd = json_object_get(root, "cmd" );
	json_t * jdata = json_object_get(root, "data" );
	
	if( !jdata )
	{
		jdata = json_null();
	}
	
	if( cmd )
	{
		json_t * s_ref_id =  json_object_get(root, "s_refid" );
		
		json_t * cmdret = appshell_cmd( xsh, json_string_value(cmd) , jdata );
		
		if( !json_is_object(cmdret ) )
		{
			fprintf(stderr, "Command %s : Return value must be an object\n",json_string_value(cmd));
			abort();
		}
		
		/* Reply with the shell refID */
		if( s_ref_id )
		{
			json_object_set_new( cmdret, "s_refid", s_ref_id );
		}
		
		char * txt_command = json_dumps(cmdret, JSON_COMPACT);
		json_decref( cmdret );
		
		int ret = connection_manager_send( &xsh->cm , txt_command );
	
		free( txt_command );
	}

	
	return;
}



int xashell_echo( struct xashell * shell, char * data)
{
	json_t * jdata = json_object();
	json_t * s = json_string(data);
	
	json_object_set(jdata, "s", s );
	
	json_t * ret = xashell_command_to_shell( shell, "echo" , jdata);
	
	json_decref(s);
	json_decref(jdata);
	json_decref(ret);
	
	return 0;
}


struct xashell * xashell_new( char * host, int port, char * secret, char * plugin_prefix )
{
	struct xashell * ret = malloc( sizeof( struct xashell) );
	
	if( !ret )
	{
		perror("malloc");
		return NULL;
	}
	
	if( connection_manager_init( &ret->cm , host, port , secret, xashell_incoming_command, (void*)ret ) )
	{
		fprintf(stderr, "Failed to connect\n");
		return NULL;
	}

	ret->plugin_prefix = strdup( plugin_prefix );

	if( xashell_plugins_init( &ret->plugins , plugin_prefix ) )
	{
		fprintf(stderr, "Failed to load plugins\n");
		return NULL;		
	}
	
	/* Start command buffer */
	ret->command_buffer = json_object();
	
	/* Init Command list */
	xashell_commands_init( &ret->cmds );

	/* Now Initialize plugins */
	xashell_plugins_call_init( &ret->plugins ,  ret );

	
	return ret;
}




ashell_t ashell_init_from_env(char *plugin_prefix)
{
	char * env = getenv("ASHELL_ADDR");
	
	if( !env )
	{
		fprintf(stderr, "Error : No ASHELL_ADDR found in environment\n");
		return NULL;
	}

	char * host = strdup( env );
	char *pp = strstr(host, ":");

	if( !pp )
		goto ER;
	
	*pp = '\0';
	
	char * port = strdup( pp + 1 );
	pp = strstr(port, ":");

	if( !pp )
		goto ER;
	
	*pp = '\0';
	
	char * secret = strdup( pp + 1 );
	
	ashell_debug("aShell : Connecting to %s:%s (SECRET %s)\n", host, port, secret );

	ashell_t ret = ashell_init(host, atoi(port), secret, plugin_prefix);
	
	free( host );
	free( port );
	free( secret );
	
	return ret;
ER:
	fprintf(stderr,"Error : Could not parse ASHELL_ADDR env. variable (HOST:PORT:SECRET)\n");
	return NULL;
}

ashell_t ashell_init(char *host, int port, char * secret, char *plugin_prefix)
{
	if(!plugin_prefix)
	{
		plugin_prefix = getenv("ASHELL_PREFIX");
		if(!plugin_prefix)
		{
			fprintf(stderr,"Error : No plugin prefix found specify\n"
						   "        one using the ASHELL_PREFIX variable\n");
			return NULL;
		}
	}
	
	return (void*)xashell_new( host,  port, secret, plugin_prefix );
}

int ashell_release(ashell_t shell)
{
	struct xashell * s = (struct xashell *)shell;

	if( !s )
	{
		return 1;
	}
	
	if( connection_manager_release( &s->cm ) )
	{
		fprintf(stderr,"Error : disconnecting");
		return 1;
	}

	/* Now Release plugins */
	xashell_plugins_call_release( &s->plugins ,  s );
	
	
	return 0;
}

int ashell_echo(ashell_t shell, char * data )
{
	struct xashell * s = (struct xashell *)shell;
	return xashell_echo( s, data);
}

json_t * appshell_cmd( ashell_t shell, const char * cmd, json_t * data )
{
	struct xashell * s = (struct xashell *)shell;

	struct xashell_command * pcmd = xashell_commands_get( &s->cmds , cmd );

	char * ret_string = "NO RET";
	json_t * ret = json_object();
	json_t * ret_data = json_object();


	if( pcmd && pcmd->callback )
	{
		 ret_string = pcmd->callback( data, ret_data );
	}
	else
	{
		ret_string = "No such command";
	}

	json_object_set_new( ret, "ret", json_string( ret_string ) );
	json_object_set_new( ret, "data", ret_data );

	return ret;
}

int ashell_data( ashell_t shell, char * plugin, char * desc, void * data )
{
	struct xashell * s = (struct xashell *)shell;

	struct xashell_plugin *pl = xashell_plugins_get_by_name( &s->plugins , plugin );

	if( !pl )
	{
		return 1;
	}

	(pl->plugin_data)( shell, (ashell_plugin_t)pl, desc, data );

	return 0;
}

int ashell_register_command( ashell_t shell, char * cmd, char * (*callback)( json_t * data, json_t * ret ) )
{
	struct xashell * s = (struct xashell *)shell;
	
	struct xashell_command * ncmd = xashell_command_new( cmd, callback );
	
	if( ! ncmd )
		return 1;
	
	return xashell_commands_push( &s->cmds, ncmd );
}

