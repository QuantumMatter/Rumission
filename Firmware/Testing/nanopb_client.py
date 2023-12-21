# Usage
# $ cd Protos
# $ cat ../Testing/Message.text | protoc --encode=Message rpc.proto | python ../Testing/nanopb_client.py | protoc --decode=Message rpc.proto

import socket
import sys
import select

# Read all binary data piped in from stdin
data = sys.stdin.buffer.read()

HOST = '127.0.0.1'
PORT = 7778

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    s.sendall(data)

    response = b''

    while True:
        ready = select.select([s], [], [], 2)
        if ready[0]:
            response += s.recv(1024)
        else:
            break

    sys.stdout.buffer.write(response)
