import cmd, sys
import socket
from _thread import *
import threading
import json

HOST = ""
PORT = 0

endpoint_id = 0

class Endpoint():
	def __init__(self, s):
		global endpoint_id
		self.s = s
		endpoint_id=endpoint_id + 1
		self.id = endpoint_id
		self.client_connected = 1
		self.s_shell = None
		self.shell_connected=0
		self.rank = -1
		self.desc = "(nill)"
	
	def disconnect_client_notify(self):
		self.client_connected = 0
	
	def get_id(self):
		return self.id
	
	def attach_client(self,s):
		if self.client_connected == 1:
			print("Warning client " + str(self.id) + "is already connected, closing previous")
			self.disconnect_client()
		self.s = s
		self.client_connected = 1
		print("Client reattached to " + str(self.id) )
	
	def disconnect_client(self):
		if self.client_connected == 0:
			#Client is not here
			return
		try:
			self.s.shutdown( socket.SHUT_RDWR )
		except:
			pass
		self.disconnect_client_notify()
	
	def disconnect_shell_notify(self):
		self.shell_connected = 0
		
	def disconnect_shell(self):
		if self.shell_connected == 0:
			return
		try:
			self.s_shell.shutdown( socket.SHUT_RDWR )
		except:
			pass
		self.disconnect_shell_notify()	
	
	def prune(self):
		self.disconnect_client()
		self.disconnect_shell()
	
	def set_meta( self, meta ):
		self.rank = meta.get("rank");	
		self.desc = meta.get("desc");
		self.host = meta.get("host");
		self.port = meta.get("port");
		return "OK"
	
	def dump( self ):
		ret = "Endpoint : Rank " + str(self.rank) + " " + self.desc
		return ret
			
	def process_command( self, command ):
		print("(i) " + command )
		
		ret = "(nil)\n"
		
		data = None
		
		try:
			data = json.loads( command )
		except:
			ret = "Could not parse command : " + command + "\n"
			print(ret)		
		
		
		if data != None:
			print("====")
			print(data)
			print("====")
			if data["cmd"] == "echo":
				ret = "RET : " + data["s"] + "\n"
			elif data["cmd"] == "meta":
				ret = self.set_meta( data )
		
		return ret

class ListenServ():
	def __init__(self, cm):
		self.cm = cm;
		self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.sockets = []
		try:
			self.s.bind((HOST, PORT))
		except socket.error as msg:
			print("Bind failed. Error Code : " + str(msg[0]) + " Message " + msg[1])
			sys.exit()
		self.s.listen(10)
		print("Socket now listening on port " + str(self.s.getsockname()[1]))
		threading.Thread(target=self.serverthread,
				args=[]).start()
	
	def end_server(self):
		for i in range( 0, len(self.sockets)):
			self.sockets[i].shutdown(socket.SHUT_RDWR)
		self.s.shutdown( socket.SHUT_RDWR );
		self.s.close()
	
	
	def clientthread(self, conn ):
		conn.send(("Welcome to the server. Type something and hit enter\n").encode())
		
		endpoint = self.cm.register_endpoint( conn )
		
		#First notify the unique ID to the client
		sid = "ID " + str(endpoint.id) + "\n"
		conn.sendall(sid.encode())
		
		while True:
			try:
				data = conn.recv(1024)
				if not data: 
					break
				sdata = str(data.decode())[:-2]
			except:
				print("Failled to decode/recv data")
			
			#for i in range(0, len(sdata)):
			#	print(str(i) + ":" + sdata[i])
			
			reply = "(nill)"
			
			if sdata.find("RESET") != -1 :
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
						reply="INF Back to " + ss[1] + "\n"
					else:
						reply="ERR No such endpoint " + ss[1] + "\n"
			elif sdata == "CLOSE":
				reply = 'INF OK...' + sdata + "\n"
				print("CLOSING Connection")
				conn.shutdown(socket.SHUT_RDWR)
				conn.close()
				self.sockets.remove( conn )
				endpoint.disconnect_client_notify()
				return
			elif sdata == "END":
				reply = 'INF OK...' + sdata + "\n"
				print("CLOSING Server")
				self.end_server()
				return
			else:
				reply = endpoint.process_command( sdata )

			conn.sendall(reply.encode())
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
			print("Connected with " + addr[0] + ":" + str(addr[1]))
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

	def get_endpoint( self, id ):
		for i in range( 0, len( self.endpoints ) ):
			if id == str(self.endpoints[i].get_id()):
				return self.endpoints[i]
		return None
	


conn = ConnectionManager()



class ExaShell(cmd.Cmd):
	intro = 'Welcome to the InSitu ExaStamp SHELL\n'
	prompt = '(exashell) '
	file = None

	def do_status(self, arg):
		'Display ExaStamp Processes Status'
		conn.list_endpoints();
	def do_prune(self, arg):
		'Prune all live connections'
		conn.prune_endpoints();
	def do_exit(self, arg):
		'Quit the ExaStamp Shell'
		print('Exitting...')
		conn.close()
		return True

if __name__ == '__main__':
	ExaShell().cmdloop()
