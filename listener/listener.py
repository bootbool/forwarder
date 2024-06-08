from . import config
from . import tcp
import socket

def handle_client(client_socket):
    # Receive data from the client
    data = client_socket.recv(config.CLIENT_INSTRUCTION_LEN) 
    print('Received:', data)
    ret, ip_port = parse_instruction(data)
    if not ret:
        print("Fail to parse ip port : ", data)
        return 
    
    ret, server = connect_realserver(ip_port)
    if not ret:
        print("Fail to connect ip port : ", ip_port)
        return 

    sock_pair = tcp.SocketPair(client_socket, server)
    sock_pair.add_to_forwarder()

"""
instruction format: 
|-2B Len-|-Dest IP:Dest port-|-2B check num-|
Len = total length including len, ip, ip and check num
check num algorithm: add the char as number from the first byte of len field to the end of port field.
"""
def parse_instruction(inst:str):
    if len(inst) <= 4: #len field, 2byte, check num , 2byte
        print("Wrong instruction")
        return False,""
    length = len(inst)
    if length != int(inst[0:2]):
        print("Wrong length", length, inst[0:2])
        return False, ""
    ip_port = (inst[2:length-2]).split(b":")
    print("------> ", ip_port)
    return True, (ip_port[0], int(ip_port[1]))


def connect_realserver(ip_port):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        # 连接到服务器
        server_socket.connect(ip_port)
    except ConnectionError as e:
        print("Connection error:", e)
        return False, 0
    return True, server_socket