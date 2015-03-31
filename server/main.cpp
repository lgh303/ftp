#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <cstring>
#include <cstdlib>
#include "ClientHandler.h"

using namespace std;

#define HOST_IP_DEFAULT "127.0.0.1"

int initSocket(int port, int backlog)
{
	 int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	 if (sockfd < 0) return -1;

	 struct sockaddr_in server_addr;
	 struct sockaddr* server_addr_ptr = (struct sockaddr*)&server_addr;
	 memset(&server_addr, 0, sizeof(server_addr));
	 server_addr.sin_family = AF_INET;
	 server_addr.sin_addr.s_addr = INADDR_ANY;
	 server_addr.sin_port = htons(port);

	 int optval = 1;

	 if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
					(const char *)&optval, sizeof(optval)))
		  return -1;

	 if (bind(sockfd, server_addr_ptr, sizeof(server_addr)) < 0)
		  return -1;

	 if (listen(sockfd, backlog))
		  return -1;

	 return sockfd;
}

void* handle_client(void* _handler)
{
	 ClientHandler *handler = (ClientHandler*)_handler;
	 handler->process();
	 delete handler;
	 return NULL;
}

unsigned get_addr(int sockfd)
{
	 struct sockaddr_in local_addr;
	 struct sockaddr* local_addr_ptr = (struct sockaddr*)&local_addr;
	 socklen_t addrlen = sizeof(local_addr);
	 memset(&local_addr, 0, addrlen);
	 if (getsockname(sockfd, local_addr_ptr, &addrlen))
		  return -1;
	 unsigned ip = ntohl(local_addr.sin_addr.s_addr);
	 return ip;
}

int main(int argc, char** argv)
{
	 if (argc != 3)
	 {
		  cerr << "Usage : ./main X.X.X.X Port" << endl;
		  cerr << "Note : Binding to port under 1024 requires root authority" << endl;
		  return -1;
	 }
	 string host_ip = HOST_IP_DEFAULT;
	 int port = 21;
	 if (argc == 3)
	 {
		  host_ip = argv[1];
		  port = atoi(argv[2]);
	 }
	 int backlog = 5;
	 int server_fd = initSocket(port, backlog);
	 if (server_fd < 0)
	 {
		  cout << "Init Socket Error" << endl;
		  return -1;
	 }
	 int client_id = -1;
	 socklen_t sin_size = sizeof(struct sockaddr_in);
	 cout << "listening..." << endl;
	 while (true)
	 {
		  ++client_id;
		  struct sockaddr_in client_addr;
		  int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &sin_size);
		  if (client_fd < 0)
		  {
			   cout << "Accept Error" << endl;
			   continue;
		  }
		  cout << "Connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << endl;
		  pthread_t *p_client_thread = new pthread_t;
		  ClientHandler *handler = new ClientHandler(p_client_thread, client_id, client_fd, host_ip);
		  pthread_create(p_client_thread, NULL, handle_client, (void*)handler);
		  pthread_detach(*p_client_thread);
	 }
}
