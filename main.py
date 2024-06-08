import listener.config as config
import listener.listener as listener
import socket


def run_server():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    address = (config.IP, config.PORT)
    server_socket.bind(address)
    server_socket.listen(config.MAX_THREADS)
    print('Server listening on {}:{}'.format(*address))

    while True:
        # Accept a client connection
        client_socket, client_address = server_socket.accept()
        print('New client:', client_address)

        # Handle the client request
        listener.handle_client(client_socket)

if __name__ == '__main__':
    run_server()