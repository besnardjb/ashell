# /***************************************************************************
#  *  Copyright (c) ParaTools SAS, All rights reserved.                      *
#  *                                                                         *
#  *  This library is free software; you can redistribute it and/or          *
#  *  modify it under the terms of the GNU Lesser General Public             *
#  *  License as published by the Free Software Foundation; either           *
#  *  version 3.0 of the License, or (at your option) any later version.     *
#  *                                                                         *
#  *  This library is distributed in the hope that it will be useful,        *
#  *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
#  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
#  *  Lesser General Public License for more details.                        *
#  *                                                                         *
#  *  You should have received a copy of the GNU Lesser General Public       *
#  *  License along with this library.                                       *
#  *                                                                         *
#  *  AUTHORS:                                                               *
#  *  	- Jean-Baptiste BESNARD  jbbesnard@paratools.fr                      *
#  *                                                                         *
#  ***************************************************************************/
import cmd, sys
import socket
from _thread import *
import threading
import json
import os, random, string
import time

HOST = socket.gethostbyaddr(socket.gethostname())[0]
PORT = 0

endpoint_id = 0


def gen_pwd(l=16):
	length = l
	chars = string.ascii_letters + string.digits + '@#^&()'
	random.seed = (os.urandom(1024))
	return ''.join(random.choice(chars) for i in range(length))

PWD=gen_pwd()


command_s_refid = int(0)
command_buffer={}


def gen_ret(ret="OK"):
	ret = { "ret" : ret }
	return ret;

def gen_ret_str(ret="OK"):
	ret = { "ret" : ret }
	return json.dumps(ret) + "\n";

class Endpoint():
	def __init__(self, s):
		#ID
		global endpoint_id
		endpoint_id=endpoint_id + 1
		self.id = endpoint_id
		#Client Side
		self.s = s
		self.client_connected =1
		#Shell Side
		self.s_shell = None
		self.shell_connected=0
		#Remote Meta
		self.rank = -1
		self.desc = "(nill)"
		self.host = "(nill)"
		self.port = 0
	
	def disconnect_client_notify(self):
		self.client_connected = 0
	
	def attach_client(self,s):
		if self.client_connected == 1:
			print("Warning client " + str(self.id) + "is already connected, closing previous")
			self.disconnect_client()
		if self.shell_connected == 1:
			print("Warning shell " + str(self.id) + "is already connected, closing previous")
			self.disconnect_shell()
		self.s = s
		self.client_connected = 1
		print("Client reattached to " + str(self.id) )
	
	def disconnect_client(self):
		self.disconnect_client_notify()
		if self.client_connected == 0:
			#Client is not here
			return
		try:
			self.s.shutdown( socket.SHUT_RDWR )
		except:
			pass
	
	def disconnect_shell_notify(self):
		self.shell_connected = 0
		
	def disconnect_shell(self):
		self.disconnect_shell_notify()	
		if self.shell_connected == 0:
			return
		try:
			self.s_shell.shutdown( socket.SHUT_RDWR )
		except:
			pass
		self.disconnect_shell_notify()	
	
	def get_connected_socket(self):
		if self.shell_connected == 1:
			return self.s_shell
		if self.client_connected == 1:
			return self.s
		return None

	def send_command( self, command, data ):
		global command_s_refid
		global command_buffer
		jscmd={}
		jscmd["s_refid"]=command_s_refid
		command_s_refid=command_s_refid+1
		jscmd["cmd"]=command
		jscmd["data"]=data
		s = self.get_connected_socket();
		if s == None:
			print("Error No connected socket")
			return
		cret = json.dumps( jscmd ) + "\n"
		#print("Sending" +  cret )
		s.sendall( cret.encode() )
		ret = None
		counter=0
		while ret == None:
			ret = command_buffer.get( str(jscmd["s_refid"]) )
			#print("Ret  " + json.dumps(ret) +  " -- " + json.dumps(command_buffer) );
			counter= counter+1
			if ret == None:
				time.sleep(0.00001)
				if 1e6 < counter:
					print("(E) Timeout getting command return");
					break
			else:
				#print("(i) Command OK");
				del command_buffer[ str(jscmd["s_refid"]) ]
				break
		return ret

	def prune(self):
		self.disconnect_client()
		self.disconnect_shell()
	
	def set_meta( self, meta ):
		self.rank = meta.get("rank");	
		self.desc = meta.get("desc");
		self.host = meta.get("host");
		self.port = meta.get("port");
		return gen_ret()
	
	def dump( self ):
		ret = "[" + str(self.id) + "] : Rank " + str(self.rank) + " " + self.desc + "@(" + self.host + ":" + str(self.port) + ")"
		
		if self.shell_connected or self.client_connected :
			ret += " ACTIVE"
		else:
			ret += " STALL"
		
		return ret
			
	def process_command( self, command ):
		#print("(i) " + command )
		global command_buffer
		
		ret = {}
		
		jcmd = None
		
		try:
			jcmd = json.loads( command )
		except:
			ret = "Could not parse command : " + command + "\n"
			print(ret)
			
		#Do we process a return ? */
		refid = jcmd.get("s_refid");
		cmd = jcmd.get("cmd");
		data = jcmd.get("data");	
		
		if refid != None:
			#print("Pushing cmd return from " + str(refid) )
			command_buffer[ str(refid) ] = data
			#print( command_buffer )
			return None;
		
		if data != None:
			#print("====")
			#print(data)
			#print("====")
			
			if cmd == "echo":
				print(data["s"] + "\n")
				ret = gen_ret()
			elif cmd == "meta":
				ret = self.set_meta( data )
		
		if jcmd.get("refid") != None:
			ret["refid"] = jcmd["refid"];
		
		return ret

class ListenServ():
	def __init__(self, cm):
		global HOST, PORT, PWD
		self.cm = cm;
		self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.sockets = []
		try:
			self.s.bind((HOST, PORT))
		except socket.error as msg:
			print("Bind failed. Error Code : " + str(msg[0]) + " Message " + msg[1])
			sys.exit()
		self.s.listen(10)
		PORT=self.s.getsockname()[1]
		#print("Socket now listening on port " + str(PORT))
		threading.Thread(target=self.serverthread,
				args=[]).start()
	
	def end_server(self):
		for i in range( 0, len(self.sockets)):
			self.sockets[i].shutdown(socket.SHUT_RDWR)
		self.s.shutdown( socket.SHUT_RDWR );
		self.s.close()

	def reconnect( self, endpoint ):
		if endpoint.get_connected_socket() != None:
			#Already running
			return;
		s = socket.socket( socket.AF_INET, socket.SOCK_STREAM)
		try:
			s.connect((endpoint.host, endpoint.port))
		except:
			print("Failled to connect to target " + endpoint.host + ":" + str(endpoint.port))
			return;
		endpoint.s_shell = s
		endpoint.shell_connected=1
		threading.Thread(target=self.clientthread, args=[s]).start()
		return s
		

	def readacommand( self, databuff, sock ):
		if '\n' in databuff:
			sret = databuff.split("\n")
			databuff = "\n".join(sret[1:])
			ret = sret[0]
			#print("RET " + ret  )
			return ret
		else:
			tdata = sock.recv(1024)
			if not tdata: 
				return None
			else:
				databuff = databuff + tdata.decode()
			return self.readacommand( databuff, sock )
	
	def clientthread(self, conn ):
		endpoint = self.cm.register_endpoint( conn )
		
		did_auth=0
		databuff = ""	
		while True:
			sdata = self.readacommand( databuff, conn )
			if sdata == None:
				print("Client Disconnected")
				break
			#print("IN -> " + sdata)
			if len(sdata.replace("\n","")) == 0:
				continue
			#for i in range(0, len(sdata)):
			#	print(str(i) + ":" + sdata[i])
			reply = gen_ret();
			if sdata.find("PWD") != -1 :
				pp = sdata.split(" ")
				if pp[1] != PWD:
					reply=gen_ret("BAD_PWD");
				else:
					conn.sendall(gen_ret_str("AUTHOK").encode())
					did_auth=1
					#First notify the unique ID to the client
					sid = "ID " + str(endpoint.id) + "\n"
					conn.sendall(sid.encode())
			elif did_auth == 0:
				reply="BAD_PWD"
			elif sdata.find("RESET") != -1 :
				print("In reset")
				ss = sdata.split(" ");
				if len(ss)!= 2:
					reply="ERR Error RESET takes an id\n"
				else:
					prev_endpoint = self.cm.get_endpoint( ss[1] )
					if prev_endpoint != None:
						self.cm.unregister_endpoint( endpoint )
						endpoint = prev_endpoint
						endpoint.attach_client( conn )
						reply=gen_ret("RESET OK");
					else:
						reply="ERR No such endpoint " + ss[1] + "\n"
			elif sdata.find("CLOSE") != -1:
				reply = 'INF OK...' + sdata + "\n"
				print("CLOSING Connection")
				conn.shutdown(socket.SHUT_RDWR)
				conn.close()
				self.sockets.remove( conn )
				endpoint.disconnect_client_notify()
				return
			elif sdata.find("END") != -1:
				reply = 'INF OK...' + sdata + "\n"
				print("CLOSING Server")
				self.end_server()
				return
			else:
				reply = endpoint.process_command( sdata )
			
			if reply != None :
				#print(json.dumps( reply ) )
				conn.sendall(( json.dumps(reply) + "\n").encode() )

		try:
			conn.close()
		except:
			pass
		self.sockets.remove(conn)
		endpoint.disconnect_client_notify()

	def serverthread(self):
		while 1:
			try:
				conn, addr = self.s.accept()
			except:
				print("Listening server closed")
				return;
			#print("Connected with " + addr[0] + ":" + str(addr[1]))
			self.sockets.append( conn );
			threading.Thread(target=self.clientthread, args=[conn]).start()
		s.close()
		print("Server Exited")


class ConnectionManager():
	def __init__(self):
		self.l = ListenServ(self)
		self.endpoints=[]
	
	def close(self):
		self.l.end_server();
	
	def register_endpoint(self, s ):
		ret = Endpoint( s )
		self.endpoints.append( ret )
		return ret
	
	def unregister_endpoint( self, endpoint ):
		print( "Enpoint disconnecting" )
		self.endpoints.remove( endpoint )

	def list_endpoints( self ):
		for i in range( 0, len( self.endpoints ) ):
			print(self.endpoints[i].dump());

	def prune_endpoints( self ):
		for i in range( 0, len( self.endpoints ) ):
			self.endpoints[i].prune();

	def clear_endpoints( self ):
		self.prune_endpoints()
		del self.endpoints[:]

	def get_endpoint( self, id ):
		for i in range( 0, len( self.endpoints ) ):
			if str(id) == str(self.endpoints[i].id):
				return self.endpoints[i]
		return None

	def command_all( self, command, data ):
		ret = []
		for i in range( 0, len( self.endpoints ) ):
			self.refresh_endpoint( i ) 
			ret.append( self.endpoints[i].send_command( command, data ) )
		return ret

	def command_to( self, id, command, data ):
		endp = self.get_endpoint( id );
		if endp == None:
			#print("No such endpoint")
			return None
		else:
			self.refresh_endpoint( id ) 
			return endp.send_command( command, data )

	def refresh_endpoint( self, id ):
		#print("Reconnecting to "+ str(id) )
		endp = self.get_endpoint( id );
		if endp != None :
			self.l.reconnect( endp )
		#else:
			#print("No such endpoint " + str(id))

conn = ConnectionManager()



class ExaShell(cmd.Cmd):
	global HOST, PORT, PWD
	intro = 'Welcome to the InSitu ExaStamp SHELL\n\n'+\
			"Please export the following environment variable\nprior to launching your job:\n\n"+\
			"export ASHELL_ADDR=\"" + HOST + ":" + str(PORT) + ":" + PWD + "\"\n" 
	prompt = '(exashell) '
	file = None

	def do_status(self, arg):
		'Display ExaStamp Processes Status'
		conn.list_endpoints();
	def do_prune(self, arg):
		'Prune all live connections'
		conn.prune_endpoints();
	def do_clear(self, arg):
		'Clear all meta-data'
		conn.clear_endpoints();
	def do_refresh(self, arg):
		'Restore connection to a given Endpoint'
		if len( arg ) != 1 :
			print("Too many args");
		else:
			eid = int( arg[0] )
			conn.refresh_endpoint( eid );
	def do_cmd(self, arg):
		'Restore connection to a given Endpoint'
		if len( arg ) < 3 :
			print("Not enough args");
		else:
			sarg = arg.split(" ");
			target = sarg[0]
			command = sarg[1]
			data = ''.join(sarg[2:])

			ret = {}

			try:
				jdata = json.loads( data )
			except:
				print("Could not process " + data);
				return
			if target == "*":
				ret = conn.command_all( command, data )
			else:
				ret = conn.command_to( int(target), command, data )
			print(json.dumps( ret ))
	def do_exit(self, arg):
		'Quit the ExaStamp Shell'
		print('Exitting...')
		conn.close()
		return True

if __name__ == '__main__':
	ExaShell().cmdloop()
