#ifndef __CLIENT_HANDLER__
#define __CLIENT_HANDLER__

#include <pthread.h>
#include <string>
#include <cstdio>

class ClientHandler
{
	 pthread_t *p_client_thread;
	 int local_host_ip;
	 int client_id;
	 int client_fd;
	 int data_listen_fd;
	 int data_conn_fd;
	 bool is_passive;
	 std::string curr_dir;
	 unsigned parse_ip(const std::string&);
	 int send_response(const std::string&, const std::string&);
	 int enter_pasv_mode();
	 int get_listen_port();
	 std::string get_real_path(const std::string&);
	 int get_file_size(const char*, std::string&);
	 int get_data_fd();
	 int send_list_data(const std::string&);
	 int send_crlf(const std::string&);
	 int init_data_listen_fd();
	 int send_file_data(FILE*);
	 int recv_file_data(FILE*);
	 int exec(const std::string&, const std::string&);
public:
	 ClientHandler(pthread_t*, int, int, const std::string&);
	 ~ClientHandler();
	 void process();
};

#endif //__CLIENT_HANDLER__
