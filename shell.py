import cmd, sys
import socket
from _thread import *
import threading

HOST = ""
PORT = 8889

class ListenServ():
	def __init__(self):
		s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		print("Socket created")
		try:
			s.bind((HOST, PORT))
		except socket.error as msg:
			print("Bind failed. Error Code : " + str(msg[0]) + " Message " + msg[1])
			sys.exit()
		print("Socket bind complete")
		s.listen(10)
		print("Socket now listening")
		threading.Thread(target=self.serverthread,
				args=[s]).start()
		#start_new_thread(self.serverthread, (self,s,))
	
	def clientthread(self,conn):
		conn.send(("Welcome to the server. Type something and hit enter\n").encode()) 
		while True:
			data = conn.recv(1024)
			sdata = str(data.decode())[:-2]
			for i in range(0, len(sdata)):
				print(str(i) + ":" + sdata[i])
			if sdata == "END":
				print("CLOSING Connection")
				conn.shutdown()
				return
			reply = 'OK...' + sdata + "\n"
			if not data: 
				break
			conn.sendall(reply.encode())
		conn.shutdown()

	def serverthread(self,s):
		#now keep talking with the client
		while 1:
			#wait to accept a connection - blocking call
			conn, addr = s.accept()
			print("Connected with " + addr[0] + ":" + str(addr[1]))
			threading.Thread(target=self.clientthread, args=[conn]).start()
		s.close()
		print("Server Exited")





class OutgoingConnection():
	host=""
	port=0
	
	def init(thost, tport):
		host = thost
		port = tport


class ConnectionManager():
	def __init__(self):
		self.l = ListenServ()
		self.endoints = []
		


conn = ConnectionManager()



class ExaShell(cmd.Cmd):
	intro = 'Welcome to the InSitu ExaStamp SHELL\n'
	prompt = '(exashell) '
	file = None

	#def __init__(self):
	#	print("INIT")

	def do_status(self, arg):
		'Display ExaStamp Processes Status'
		print("STATUS")
		print("->" + arg + "<-")
	def do_exit(self, arg):
		'Quit the ExaStamp Shell'
		print('Exitting...')
		return True

if __name__ == '__main__':
	ExaShell().cmdloop()
