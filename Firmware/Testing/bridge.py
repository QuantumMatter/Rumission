from typing import Tuple, List

import time
import traceback

from multiprocessing import Process
from multiprocessing import Event as CreateEvent
from multiprocessing.synchronize import Event
from argparse import ArgumentParser

import socket
import select

POLL_TIMEOUT = 0.25

def bridge(exit_event: Event, server: Tuple[int, socket.socket], clients: List[Tuple[int, socket.socket]]):

    print(f'Broadcasting port {server[0]} to { ", ".join([str(c[0]) for c in clients]) }')

    try:

        while not exit_event.is_set():
            ready = select.select([server[1]], [], [] , POLL_TIMEOUT)
            if ready[0]:
                data = server[1].recv(1024)
                if not data:
                    break
                elif len(data) == 0:
                    continue
                else:
                    print(f'Forwarding data from port { server[0] }: { data }')
                    for (_, client) in clients:
                        client.sendall(data)

    except Exception as e:
        traceback.print_exc()
        print(f'Error on port { server[0] }')




def open_socket(port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('127.0.0.1', port))
    return sock

if __name__ == '__main__':
    parser = ArgumentParser(description='Bridge between a server and a client')

    # Let the user specify any number of ports to connect to each other
    parser.add_argument('ports', metavar='port', type=int, nargs='+',
                        help='port to connect to')
    
    args = parser.parse_args()
    
    exit_event = CreateEvent()
    exit_event.clear()

    ports = [(port, open_socket(port)) for port in args.ports]
    processes = []
    
    for (port_idx, server) in enumerate(ports):
        p = Process(target=bridge, args=(exit_event, server, ports[:port_idx] + ports[port_idx+1:]))
        processes.append(p)
        p.start()

    # Wait for the user to press Ctrl+C to exit
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        exit_event.set()
        for p in processes:
            p.join()
        for (_, sock) in ports:
            sock.close()