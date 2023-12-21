import argparse
import socket
import select

import rpc_pb2 as RPC

argparser = argparse.ArgumentParser(description='Nanopb Server')
argparser.add_argument('--port', type=int, help='Port to listen on', default=7777)
argparser.add_argument('--host', type=str, help='Host to listen to', default='127.0.0.1')


def ping(arg: RPC.InOut, out: RPC.InOut):
    print('Ping')

    out.integer = arg.integer + 1

    return 0


def post_record(arg: RPC.InOut, out: RPC.InOut):
    print('Post record')

    out.integer = 0

    return 0


if __name__ == '__main__':
    args = argparser.parse_args()
    (host, port) = (args.host, args.port)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))
    # s.bind((host, port))
    # s.listen(1)

    print('Connected to %s:%d' % (host, port))

    # while True:
        # (client, address) = s.accept()

        # Loop until the client disconnects
    while True:
        # data = s.recv(1024)
        # if not data:
        #     print('Client disconnected')
        #     exit(0)

        # print(data)

        response = b''
        has_data = False
        while True:
            ready = select.select([s], [], [], 0.1)
            if ready[0]:
                has_data = True
                response += s.recv(1024)
            else:
                break

        if not has_data:
            continue

        print(f'Data: { response }')

        message = RPC.Message()
        message.ParseFromString(response)

        print('Received message: ')
        print(message)

        if message.WhichOneof('data') == 'request':
            request = message.request

            out_message = RPC.Message()
            out_message.version = 1
            out_message.m_id = 1
            out_message.rpc_id = message.rpc_id
            out_message.source = message.target
            out_message.target = message.source
            out_message.checksum = 100
            response = out_message.response

            if request.func == RPC.FUNCTION.FUNC_PING:
                response.type = RPC.ResponseType.SUCCESS
                ping(request.args, response.result)
            elif request.func == RPC.FUNCTION.FUNC_POST_RECORD:
                response.type = RPC.ResponseType.SUCCESS
                post_record(request.args, response.result)
            else:
                response.type = RPC.ResponseType.ERROR
                response.result.error.code = -1
                response.result.error.message = 'Unknown function'

            out_data = out_message.SerializeToString()
            s.send(out_data)
                    

        
