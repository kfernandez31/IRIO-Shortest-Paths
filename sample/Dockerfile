FROM python:3

WORKDIR data/

COPY ./client_test.py .
COPY ./shortestpaths.proto .


RUN pip install grpcio-tools
RUN pip install flask
RUN pip install psycopg2
RUN python -m grpc_tools.protoc -I. --python_out=. --pyi_out=. --grpc_python_out=. shortestpaths.proto
