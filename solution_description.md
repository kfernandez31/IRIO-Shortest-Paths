# Solution description

## General description
The core of the project has been written in C++, for high performance and advanced synchronization features. Program is divided into *Processing sets*. In each *processing set* there is one main processing node and a number of worker nodes equal to number of regions the graph was divided into. At the beginning of the program main waits for workers to be assigned to it. When there is a complete set of workers present main starts serving client queries. At any time each *processing set* processes at most one client query.

---
## The algorithm
After receiving client query *processing set* starts processing. Main node sends special grpc message to the worker node corresponding to region, where source from client query is located.
Whole processing algorithm is generally divided into 3 phases. Transitions between phases are centrally coordinated by main.

### Local phase
Goal of this phase is to improve paths locally in each region.
In this phase each worker node runs standard Dijkstra algorithm with one modification. Each worker node has it's own priority queue, just like in standard Dijkstra, and it processes subsequent nodes taken from that queue and adds new nodes to the queue if necessary.
The only alteration with respect to the original Dijkstra algorithm is that when some edge leaves region assigned to a worker, it is not added to the priority queue, but instead it is remembered on separate *outgoing-messages queue*. This phase of computation is done in loop, until there is nothing more on priority queue. Once that occurs, a **Global phase** of computation begins.

### Global phase
Goal of this phase is for the workers to exchange information about their best paths.
In this phase each worker node sends messages corresponding to edges coming out of it's region. Those messages have been stored in worker's *outgoing-messages queue* in previous phase. Messages are sent directly to neighbouring (in a sense of grahp regions) worker nodes. After receiving those messages worker nodes put entries corresponding to them to their priority queues. If any message has been sent in this phase in whole system then system transitions to next **Local phase**, otherwise, if no messages have been sent system tranbsitions to **Retrieval phase**.

### Retrieval phase
Goal of this phase is to retrieve optimal path.
In this phase main queries subsequent region workers about optimal path fragments located in their regions (starting from destination region). After receiving all answers main puts whole path together and sends it back to frontend, to be presented to a client.
After that system is read to serve next client query.

---
## Additional information
* At the beginning each worker node is assigned region by main. Each worker downloads it's corresponding region graph from databse, but it doesn't need to know whole graph. This allows to process graphs that are to big to fit in one machine's memory.
* System is designed, so that graph can be partitioned into any number of regions, additionaly the way paritions are calculated doesn't matter for correctness of the algorithm (as long as parition is correct, and edges are given correct masks). Different paritioning methods may result in speed of the algorithm changing.
* All communication is done using GRPC.
* In current implementation the system is **NOT** fault tolerant, but it can be relatively easily extended to be so (for example by using some form of stable-storage).


