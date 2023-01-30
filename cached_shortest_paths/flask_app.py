#!/bin/python3

from flask import Flask, Response, request
import json
import sys
import os
import subprocess

app = Flask(__name__)

@app.route('/', methods=['GET'])
def heatlh_check():
    return Response(status=200)

@app.route('/shortest_path', methods=['GET', 'POST'])
def shortest_path():
    if request.method == 'POST':
        source = request.values.get('source')
        destination = request.values.get('destination') 
    else:
        source = request.args.get('source')
        destination = request.args.get('destination')

    args = ("./client", source, destination)
    popen = subprocess.Popen(args, stdout=subprocess.PIPE)
    popen.wait()
    out = popen.stdout.readlines()

    distance = out[0].decode("utf-8") 
    path = list(map(lambda line: line.decode("utf-8"), out[1:]))
    body = {'distance': distance, 'path': path}

    return Response(response=json.dumps(body), mimetype='application/json')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: ' + os.path.basename(sys.argv[0]) + ' <host> <port>')
        exit(1)

    app.run(host=sys.argv[1], port=int(sys.argv[2]))
