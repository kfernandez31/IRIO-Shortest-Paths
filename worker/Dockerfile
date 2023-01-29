FROM grpc/cxx:1.12.0

WORKDIR /data

COPY ./worker.cc .
COPY ./worker_server.cc .
COPY ./worker_client.cc . 
COPY ./common.cc .
COPY ./*.hh .
COPY ./*.proto .
COPY Makefile .

RUN make
