#!/bin/python3

import psycopg2
import sys
import os

def print_usage():
    print(
        'Usage: ' + os.path.basename(sys.argv[0]) +
        'database' + 
        'user' +
        'password' +
        'host' +
        'port'
    )

def copy_table(cursor, filename, table_name):
    f = open(filename)
    sql = 'COPY ' + table_name + ' FROM stdin WITH CSV HEADER'
    cursor.copy_expert(sql=sql, file=f)

if (len(sys.argv) != 6):
    print_usage()
    sys.exit(1)

conn = psycopg2.connect(
    database=sys.argv[1], 
    user=sys.argv[2], password=sys.argv[3], 
    host=sys.argv[4], port=sys.argv[5]
)

# conn = psycopg2.connect(
#     database='postgres',
#     user='postgres', password='postgres', 
#     host='127.0.0.1', port='5432'
# )

conn.autocommit = True
cursor = conn.cursor()

cursor.execute(open('create_tables.sql', 'r').read())

copy_table(cursor, 'nodes_out.csv', 'node')
copy_table(cursor, 'edges_out.csv', 'edge')
copy_table(cursor, 'direct_connections_out.csv', 'direct_connection')
copy_table(cursor, 'optimal_connections_out.csv', 'optimal_connection')

conn.commit()
conn.close()
