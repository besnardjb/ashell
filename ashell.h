#ifndef ASHELL_H
#define ASHELL_H


typedef void* ashell_t; 

ashell_t ashell_init_from_env(char *plugin_prefix);

ashell_t ashell_init(char *host, int port, char * secret, char *plugin_prefix);

int ashell_release(ashell_t shell);

int ashell_echo(ashell_t shell, char * data );

char * appshell_cmd( ashell_t shell, char * cmd, char * data );

int ashell_register_command( ashell_t shell, char * cmd, char * (*callback)( char * data ) );



#endif /* ASHELL_H */
