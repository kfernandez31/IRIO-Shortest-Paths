from concurrent import futures
import grpc
import logging
import shortestpaths_pb2
import shortestpaths_pb2_grpc
import psycopg2
import sys

class ConnectorServer(shortestpaths_pb2_grpc.DBConnector):
    def get_region_info(self, request, context):
        region_id = request.region_id
        return self.get_region_info_inner(region_id)


    def get_region_info_inner(self, region_id):
        cursor = self.conn.cursor()
        cursor.execute("SELECT id, region FROM node")
        all_verticies = cursor.fetchall()
        region_set = set([reg for (_, reg) in all_verticies])
        all_verticies = {v_id: reg for (v_id, reg) in all_verticies}

        # print(region_id)
        cursor.execute(f"SELECT id, region FROM node WHERE region={region_id}")
        # print(f"SELECT id, region FROM node WHERE region={region_id}")
        verticies = cursor.fetchall()
        # print(verticies)

        e_info = []
        v_info = []
        for (vertex, _) in verticies:
            cursor.execute(f"SELECT * FROM edge WHERE source = {vertex}")
            edges = cursor.fetchall()
            cursor.execute(f"SELECT * FROM optimal_connection WHERE source = {vertex}")
            mappings = cursor.fetchall()

            mappings_set = set(mappings)

            v_info.append(shortestpaths_pb2.VertexInfo(vertex_id = vertex))

            
            for (s, t, length) in edges:
                t_reg = all_verticies[t]
                e_info_inner = shortestpaths_pb2.EdgeInfo(start_vertex_id = vertex, end_vertex_id = t, end_vertex_region=t_reg, weight=int(length))
                e_mask = []
                for region_num in range(len(region_set)):
                    if (s, t, region_num) in mappings_set:
                        e_mask.append(shortestpaths_pb2.Mask(is_optimal=True))
                    else:
                        e_mask.append(shortestpaths_pb2.Mask(is_optimal=False))
                
                e_info_inner.mask.extend(e_mask)

                e_info.append(e_info_inner)
        
        r_info = shortestpaths_pb2.RegionInfo(verticies = v_info, edges = e_info)

        return r_info

    def get_region_neighbours(self, request, context):
        region_borders = self.get_borders()
        region_borders = [shortestpaths_pb2.RegionBorder(region_id1 = x, region_id2 = y) for (x,y) in region_borders]
        result = shortestpaths_pb2.RegionIds()
        result.region_border.extend(region_borders)

        return result


    def get_borders(self):
        cursor = self.conn.cursor()
        cursor.execute("SELECT * FROM direct_connection")
        result = cursor.fetchall()
        # print(result)
        return result


    def make_db(self, db_addr, db_port):
        self.conn = psycopg2.connect(
        database="postgres", 
        user="postgres", password="postgrespw", 
        host=db_addr, port=db_port)

        self.conn.autocommit = True


def serve(db_addr, db_port):
    s = ConnectorServer()
    s.make_db(db_addr, db_port)
    print("Connected to DB")
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    shortestpaths_pb2_grpc.add_DBConnectorServicer_to_server(
        s, server)
    server.add_insecure_port('0.0.0.0:5005')
    server.start()
    server.wait_for_termination()



if __name__ == '__main__':
    db_addr = sys.argv[1]
    db_port = sys.argv[2]
    logging.basicConfig()
    serve(db_addr, db_port)

