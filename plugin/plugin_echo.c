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
