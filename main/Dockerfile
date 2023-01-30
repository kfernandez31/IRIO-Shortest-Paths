FROM grpc/cxx:1.12.0

WORKDIR /data

COPY ./main.cc .
COPY ./main_server.cc .
COPY ./common.cc .
COPY ./*.hh .
COPY ./*.proto .
COPY Makefile .

RUN make