Project Structure
The project consists of the following source files:

client.c: The client program that interacts with the user and sends requests to the load balancer.
load_balancer.c: The load balancer that distributes requests to the appropriate servers.
primary_server.c: The primary server responsible for handling add and modify graph operations.
secondary_server.c: The secondary servers that handle DFS and BFS operations on graphs.
cleanup.c: A utility program to terminate the server processes.


Building and Running
To build and run the project, follow these steps:

Compile the source files using a C compiler (e.g., gcc):

gcc -o client client.c
gcc -o load_balancer load_balancer.c
gcc -o primary_server primary_server.c -lpthread
gcc -o secondary_server secondary_server.c -lpthread
gcc -o cleanup cleanup.c

Start the load balancer server:
./load_balancer

Start the primary server:
./primary_server

Start the secondary servers (provide 1 for odd and 2 for even):
./secondary_server
Enter secondary server type: 1  # For odd secondary server
./secondary_server
Enter secondary server type: 2  # For even secondary server

Run the client program:
./client




