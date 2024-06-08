import sys

IP = "0.0.0.0"
PORT = 0
ROOT_DIR = "/proc/forwarder/"
PORT_CONFIG = ROOT_DIR + "port"
PORWARD_CONFIG = ROOT_DIR + "forward"
SHOW_FORWARD = ROOT_DIR + "show"
MAX_THREADS = 100
CLIENT_INSTRUCTION_LEN = 64

def init_os_port():
    global PORT
    try:
        file = open(PORT_CONFIG, "r")
    except IOError:
        print("Fail to open port config file")
        sys.exit(1)
    content = file.read()
    print("read file ", PORT_CONFIG, " ", content)
    file.close() 
    PORT = int(content)
    if PORT == 0:
        print("Wrong port number")
        sys.exit(1)

def get_os_port():
    print("Port is ", PORT)
    return PORT

# format: fd1 fd2
def add_to_forwarder( fd_str:str ):
    fd_list = fd_str.split(" ")
    if len(fd_list) != 2:
        print("Error forwarder format: ", fd_str)
        sys.exit(1)
    file = open(PORWARD_CONFIG, "wt")
    file.write(fd_str)
    file.close()


init_os_port()
