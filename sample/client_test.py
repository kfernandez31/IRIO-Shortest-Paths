from flask import Flask, Response, request
from concurrent import futures
from threading import Event, Thread
import json
import sys
import os
import subprocess
import psycopg2
import grpc
import logging
import shortestpaths_pb2
import shortestpaths_pb2_grpc
import sys

query_events = {}
query_results = {}


main_addr = sys.argv[1]
self_addr = sys.argv[2]
db_addr = sys.argv[5]
db_port = int(sys.argv[6])


def get_region(vertex):
    conn = psycopg2.connect(
    database="postgres", 
    user="postgres", password="postgrespw", 
    host=db_addr, port=db_port)
    conn.autocommit = True

    cursor = conn.cursor()

    cursor.execute(f"SELECT (id, region) FROM node WHERE id = {vertex}")
    res = cursor.fetchone()

    if res is None:
        return None
    
    res = eval(res[0])
    logging.warning(res)
    return int(res[1])


class ClientServer(shortestpaths_pb2_grpc.ClientService):
    def send_result(self, request, context):
        number = request.client_number
        logging.warning(f"client number{number}")
        distance = request.total_distance
        logging.warning(f"distance {distance}")
        verticies = [] 
        for vertex in request.verticies:
            verticies.append((vertex.vertex_id, vertex.distance))
        query_results[number] = (verticies, distance)
        query_events[number].set()

        return shortestpaths_pb2.Ok()


def serve():
    s = ClientServer()

    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    shortestpaths_pb2_grpc.add_ClientServiceServicer_to_server(
        s, server)
    server.add_insecure_port('0.0.0.0:5006')
    server.start()
    server.wait_for_termination()




def run_grpc_query(start_vertex, start_vertex_region, end_vertex, end_vertex_region ):
    logging.warning("RUNNING GRPC REQUEST")

    number = len(query_events)
    with grpc.insecure_channel(main_addr) as channel:
        stub = shortestpaths_pb2_grpc.ShortestPathsMainServiceStub(channel)
        logging.warning((type(start_vertex), start_vertex))
        logging.warning((type(start_vertex_region), start_vertex_region))  
        logging.warning((type(end_vertex), end_vertex))  
        logging.warning((type(end_vertex_region), end_vertex_region))
        logging.warning((type(self_addr), self_addr))
        logging.warning((type(number), number))

        query = shortestpaths_pb2.ClientQuery()
        query.start_vertex = start_vertex
        query.start_vertex_region = start_vertex_region
        query.end_vertex = end_vertex
        query.end_vertex_region = end_vertex_region
        query.client_address = self_addr
        query.client_number = number

        stub.client_query(query)

    logging.warning("GRPC REQUEST SENT")

    query_events[number] = Event()

    return (number, query_events[number])
    


app = Flask(__name__)

@app.route('/', methods=['GET'])
def heatlh_check():
    body = {'ok': "Ok",}

    return Response(response=json.dumps(body), mimetype='application/json')

@app.route('/shortest_path', methods=['GET'])
def shortest_path():
    source = int(request.args.get('source'))
    destination = int(request.args.get('destination'))

    source_reg = get_region(source)
    destination_reg = get_region(destination)

    logging.warning(f"RETRIEVED REGIONS: {source_reg} {destination_reg}" )

    if source_reg is None or destination_reg is None:
        return Response(response="Invalid verticies", mimetype='application/json', status=400)

    number, ev = run_grpc_query(source, source_reg, destination, destination_reg)
    
    ev.wait()
    
    body = {'distance': query_results[number][0], 'path': query_results[number][1]}

    return Response(response=json.dumps(body), mimetype='application/json')


if __name__ == '__main__':
    if len(sys.argv) != 7:
        print('Usage: ' + os.path.basename(sys.argv[0]) + 'main_address ' + 'self_send_address ' + 'flask_host ' +  'flask_port' + 'db_address ' + 'db_port')
        exit(1)

    logging.basicConfig()
    Thread(target=serve).start()
    app.run(host=sys.argv[3], port=int(sys.argv[4]))
    

    

