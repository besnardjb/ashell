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
#ifndef ASHELL_H
#define ASHELL_H

#include <jansson.h>

/** This is the defininition of the opaque ashell object
 * @note This object is to be initialized and freed in the main
 */
typedef void* ashell_t; 

/** This is the value for an invalid aShell instance
 */
#define ASHELL_ERROR (NULL)

/** This is the common way of initializing a shell
 * 
 * @warning This command depends on the following env variabes
 * 			- ASHELL_ADDR="HOST:PORT:PWD"
 * 			- ASHELL_PREFIX=PATH_TO_PLUGINS (only if plugin_prefix is NULL)
 *
 * @arg plugin_prefix Path to be provided by the application  for plugins
 * @return A valid ashell object on success (ASHELL_ERROR otherwise)
 */
ashell_t ashell_init_from_env(char *plugin_prefix);

/** This is the command to manually initalize a SHELL
 *
 * @arg host Remote hostname of the shell
 * @arg port Remote port of the shell
 * @arg secret Password for the shell
 * @arg plugin_prefix A prefix for the remote shell
 * @return A valid ashell object on success (ASHELL_ERROR otherwise)
 */
ashell_t ashell_init(char *host, int port, char * secret, char *plugin_prefix);

/** This command has to be called on a aShell instance to remove it
 * @note It is valid to call it on ASHELL_ERROR shell
 * @arg shell aShell instance to be freed
 * @return 0 On success 1 otherwise
 */
int ashell_release(ashell_t shell);

/** This command print a line in the application shell
 * @arg shell Shell instance to print to
 * @arg data A string to print
 * @return 0 on success 1 otherwise
 */
int ashell_echo(ashell_t shell, char * data );

/** This command is used to send a command to the local aShell instance
 * 
 * @warning "data" MUST be a JSON object 
 * @note Returned object contains the following members:
 * 		- data : Data set by the callback
 * 		- ret : A string describing a possible error
 * 		For example :
 * 			{ data : {}, "ret": "OK" }
 *
 * @arg shell aShell instance to target
 * @arg cmd The command to call
 * @arg data Input JSON data to be sent to the command
 * @return A JSON describing command return and possible errors
 */
json_t * appshell_cmd( ashell_t shell, const char * cmd, json_t * data );

/** This command is used by plugins to register new commands
 * @arg shell aShell instance to register in
 * @arg cmd New command to define
 * @arg callback The function which will be called on command
 * @return 0 on success 1 otherwise
 */
int ashell_register_command( ashell_t shell, 
							 char * cmd, 
							 char * (*callback)( json_t * data, json_t * ret  ) );

/** A print function only enabled when ashell is in Verbose mode
 * @note Use the env var ASHELL_VERBOSE to make aShell verbose
 * @arg fmt Format (like printf)
 * @arg ... Optionnal data (like printf)
 */
void ashell_debug(const char *fmt, ...);

#endif /* ASHELL_H */
