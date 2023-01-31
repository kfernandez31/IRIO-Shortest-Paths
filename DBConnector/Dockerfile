FROM python:3

WORKDIR data/

COPY ./connector.py .
COPY ./shortestpaths.proto .
COPY ./requirements.txt .


RUN pip install -r requirements.txt
RUN python -m grpc_tools.protoc -I. --python_out=. --pyi_out=. --grpc_python_out=. shortestpaths.proto
