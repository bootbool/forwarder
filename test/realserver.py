import socket

# 服务器地址和端口
server_address = ('192.168.2.6', 6666)

def handle_client( client:socket.socket ):
    while True:
        try:
            buff = client.recv(100)
            print(len(buff), "Recving...  ", buff)
            client.send(  "ok-> ".encode() )
        except Exception as e:
            print(e)
            client.close()
            print("===========================")
            return

def run_server():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    address = ( server_address )
    server_socket.bind(address)
    server_socket.listen(5)
    print('Server listening on {}:{}'.format(*address))

    while True:
        # Accept a client connection
        client_socket, client_address = server_socket.accept()
        print('New client:', client_address, server_address)

        # Handle the client request
        handle_client(client_socket)

if __name__ == '__main__':
    run_server()