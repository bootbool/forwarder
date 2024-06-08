from . import config
import socket

# a connection socket
class Socket:
    source_ip = "0.0.0.0"
    source_port = 0
    dest_ip = "0.0.0.0"
    dest_port = 0
    sock = 0
    def __init__(self, sock:socket.socket):
        self.dest_port = config.get_os_port()
        self.sock = sock  
    def print_me(self):
        print("{} {}:{} -> {}:{}".format(socket, source_ip, source_port, dest_ip, dest_port) )

class SocketPair():
    def __init__(self, client:socket.socket, server:socket.socket):
        self.client = Socket(client)
        self.server = Socket(server)
    
    def print_me(self):
        self.client.print_me()
        self.server.print_me()

    def add_to_forwarder(self):
        string = "{} {}".format(self.client.sock.fileno(), self.server.sock.fileno())
        print("write to forward", string)
        config.add_to_forwarder(string)

        