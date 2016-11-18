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

#include <mpi.h>

#include "ashell.h"



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

void * client_loop( void *pcm )
{
	struct connection_manager *cm = (struct connection_manager *)pcm;
	
	char *buff = malloc( BUFF_SIZE );
	
	if( !buff )
	{
		perror("malloc");
		abort();
	}
	
	buff[0] = '\0';
	
	while(1)
	{
		int n = 0;
		
		if((n = recv(cm->outgoing_socket, buff, BUFF_SIZE - 1, 0)) < 0)
		{
		  perror("recv()");
		  break;
		}

		buff[n] = '\0';
		
		(cm->cb)( buff, cm->arg );
	}
	
	free( buff );
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
			perror("ERROR on accept");
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
		snprintf(pwd, 100, "PWD %s", cm->secret );
		/* Send AUTH */
		send(cm->outgoing_socket, pwd, strlen(pwd), 0);
		
		/* GET ACK */
		n = recv(cm->outgoing_socket, buff, 100 - 1, 0);
		buff[n] = '\0';
		if( !strcmp("AUTHOK\n", buff) )
		{
			printf("OK got ACK \n");
			got_ack =1;
		}
	}while(!got_ack);
	
	/* READ ID */

	n = recv(cm->outgoing_socket, buff, 100 - 1, 0);
	buff[n] = '\0';

	printf("==>%s\n", buff );

	int id = atoi( buff + 3 );
	
		
	printf("Endpoint id %d\n", id );
	
	if( cm->id == -1 )
	{
		/* First encounter */
		cm->id = id;
		char meta[500];
		int rank;
		MPI_Comm_rank(MPI_COMM_WORLD, &rank );
		snprintf(meta,500,"{\"cmd\":\"meta\", \"rank\" : %d, \"desc\" : \"%s\", \"host\" : \"%s\", \"port\" : %d}", rank, "", cm->hostname, cm->listening_port);
		send(cm->outgoing_socket, meta, strlen(meta), 0);
		flush( cm->outgoing_socket );
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
	
	struct xashell_plugin * next;
};




struct xashell_plugins
{
	struct xashell_plugin *plugins;
	
};



struct xashell
{
	struct connection_manager cm;

	char * plugin_prefix;
	
};


void xashell_command( char * data, void *ps)
{
	struct xashell * xsh = (struct xashell *)ps;
	printf("==>%s<===\n", data );
}



struct xashell * xashell_new( char * host, int port, char * secret, char * plugin_prefix )
{
	struct xashell * ret = malloc( sizeof( struct xashell) );
	
	if( !ret )
	{
		perror("malloc");
		return NULL;
	}
	
	if( connection_manager_init( &ret->cm , host, port , secret, xashell_command, (void*)ret ) )
	{
		fprintf(stderr, "Failed to connect\n");
		return NULL;
	}

	ret->plugin_prefix = strdup( plugin_prefix );

	
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
	
	printf("Connecting to %s:%s (SECRET %s)\n", host, port, secret );

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
	
	if( connection_manager_release( &s->cm ) )
	{
		fprintf(stderr,"Error : disconnecting");
		return 1;
	}
	
	return 0;
}

int ashell_echo(ashell_t shell, char * data )
{
	
	
}

char * appshell_cmd( ashell_t shell, char * cmd, char * data )
{
	
	
}

int ashell_register_command( ashell_t shell, char * cmd, char * (*callback)( char * data ) )
{
	
	
}

