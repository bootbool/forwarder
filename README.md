
# Forwarder

This project aim to forward data to any destination you want. Unlike kernel LVS, it makes forward decision from by your instruction. After connect to forwarder, the first package sent is used for make decision, then the following data is transmitted as usual.

# Work mechanism

## Connection procedure

Actor:   Client   --->   Forwarder  ---> Real Server

 - Forwarder create a user program, and listen on a tcp port waiting for client connection.
 - Client call "connect" system call to connect Forwarder, then send *Instruction* to Forwarder.
 - After receiving instruction, Forwarder record the connection file description(fd-c), and parse the instruction data. 
 - After parsing instruction data, Forwarder connect the Real Server, and record the connection file description(fd-s).
 - After the dual connection is finished, Forwarder build a data struct containing fd-c, fd-s, and the tcp sequence number difference value. Then a acknowledge response to sent back to Client.
 - Client receives the response,  start the normal data transmission.
 
 ## Program component
 
 - Connection listener, a daemon running on user mode, for accept  the connection from client and parse the instruction data. and initiate the connection to real server. 
 - Kernel forwarder, a netfilter module running on kernel mode,  modify the package data , and forward it to real server.
