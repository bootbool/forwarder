import socket
import time

# forwarder地址和端口
server_address = ('192.168.11.2', 9999)

# 创建套接字对象
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

try:
    # 连接到服务器
    client_socket.connect(server_address)
    local = client_socket.getsockname()
    print(local, server_address)
    

    # 发送数据
    pkg = "192.168.2.6:6666"+"00"
    length = len(pkg)+2
    pkg = "{}{}".format(length, pkg)
    time.sleep(1)

    print("sending... ", length, pkg)

    client_socket.send(pkg.encode())

    while( True ):
        str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()) 
        input("continue.....")
        client_socket.send(str.encode())
        respond = client_socket.recv(100)
        print("Server response.....  ", respond.decode())


except ConnectionError as e:
    print("Connection error:", e)
    exit(0)    