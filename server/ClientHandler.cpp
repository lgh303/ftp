#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include "ClientHandler.h"
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

using namespace std;

ClientHandler::ClientHandler(pthread_t *p_thread, int id, int fd, const string& ip)
	 : p_client_thread(p_thread), client_id(id), client_fd(fd)
{
	 is_passive = false;
	 curr_dir = "./";
	 local_host_ip = parse_ip(ip);
	 cout << "Client id : " << client_id << endl;
}

ClientHandler::~ClientHandler()
{
	 close(client_fd);
	 delete p_client_thread;
}

unsigned ClientHandler::parse_ip(const string& ip)
{
	 string ipstr = ip + '.';
	 int h[4];
	 int count = 0, lastsep = -1;
	 for (int i = 0; i < ipstr.size(); ++i)
		  if (ipstr[i] == '.')
		  {
			   string tmp = ipstr.substr(lastsep + 1, i - lastsep - 1);
			   h[count] = atoi(tmp.c_str());
			   lastsep = i;
			   ++count;
		  }
	 unsigned ret = h[0];
	 for (int i = 1; i < 4; ++i)
		  ret = (ret << 8) + h[i];
	 return ret;
}

int ClientHandler::send_response(const string& code, const string& info)
{
	 string str = code + ' ' + info;
	 cout << "Response : " << str << endl;
	 int ret = send(client_fd, (str + "\r\n").c_str(), str.length() + 2, 0);
	 return ret;
}

void ClientHandler::process()
{
	 int ret = send_response("220", "FTP Server is ready!");
	 if (ret < 0)
	 {
		  cout << "Client " << client_id << " quit!";
		  return;
	 }
	 while (true)
	 {
		  char buf[256];
		  int len = recv(client_fd, buf, 256, 0);
		  if (len <= 0)
		  {
			   cout << "Client " << client_id << " Quit." << endl;
			   break;
		  }
		  if (buf[len - 2] == '\r' && buf[len - 1] == '\n')
		  {
			   buf[len - 2] = '\0';
			   string str(buf);
			   string cmd, args;
			   int pos = str.find_first_of(" ");
			   if (pos == string::npos)
			   {
					cmd = str;
					args = "";
			   }
			   else
			   {
					cmd = str.substr(0, pos);
					args = str.substr(pos + 1, str.length() - pos - 1);
			   }
			   int ret = exec(cmd, args);
			   if (ret == 1)
			   {
					cout << "Client " << client_id << " Quit." << endl;
					break;
			   }
		  }
		  else
		  {
			   cout << "Bad Command, can not find line break" << endl;
		  }
	 }
}

int ClientHandler::exec(const string& cmd, const string& args)
{
	 cout << "Client " << client_id << " : [" << cmd << ' ' << args << ']' << endl;
	 if (cmd == "USER")
	 {
		  send_response("230", "Everyone is Welcomed");
		  return 0;
	 }
	 else if (cmd == "SYST")
	 {
		  send_response("215", "UNIX Type: L8");
		  return 0;
	 }
	 else if (cmd == "PWD")
	 {
		  send_response("257", curr_dir.substr(1, curr_dir.size() - 1).c_str());
	 }
	 else if (cmd == "TYPE")
	 {
		  send_response("200", "Binary mode");
		  return 0;
	 }
	 else if (cmd == "PASV")
	 {
		  int ret = enter_pasv_mode();
		  if (ret < 0)
		  {
			   cout << "Enter PASV Mode Error." << endl;
			   return 1;
		  }
		  return 0;
	 }
	 else if (cmd == "SIZE")
	 {
		  string size;
		  string real_path = get_real_path(args);
		  if (get_file_size(real_path.c_str(), size) < 0)
		  {
			   cout << "Size Error" << endl;
			   send_response("550", "No File access");
		  }
		  else
			   send_response("213", size);
		  return 0;
	 }
	 else if (cmd == "CWD")
	 {
		  string real_path = get_real_path(args);
		  if (!opendir(real_path.c_str()))
		  	   send_response("550", "Can not Change DIR");
		  else
		  {
			   if (real_path[real_path.size() - 1] == '/')
					curr_dir = real_path;
			   else
					curr_dir = real_path + '/';
		  	   send_response("250", "Directory changed.");
		  }
		  return 0;
	 }
	 else if (cmd == "LIST")
	 {
		  data_conn_fd = get_data_fd();
		  if (data_conn_fd < 0)
		  {
			   cout << "Data Connection Accept Error" << endl;
			   return 0;
		  }
		  if (send_list_data(curr_dir) < 0)
		  {
			   cout << "Send List Data Error" << endl;
			   return 0;
		  }
		  close(data_conn_fd);
		  send_response("226", "Data Connection Closed.");
		  return 0;
	 }
	 else if (cmd == "RETR")
	 {
		  string real_path = get_real_path(args);
		  FILE *file = fopen(real_path.c_str(), "rb");
		  if (!file)
		  {
			   send_response("550", "Fail to Open Input File");
			   return 0;
		  }
		  data_conn_fd = get_data_fd();
		  if (data_conn_fd < 0)
		  {
			   cout << "Data Connection Accept Error" << endl;
			   return 0;
		  }
		  cout << "Transfering..." << endl;
		  if (send_file_data(file) < 0)
		  {
			   cout << "Send File Data Error" << endl;
			   return 0;
		  }
		  cout << "Transfer Completed." << endl;
		  fclose(file);
		  close(data_conn_fd);
		  send_response("226", "Data Connection Closed.");
		  return 0;
	 }
	 else if (cmd == "STOR")
	 {
		  string real_path = get_real_path(args);
		  FILE *file = fopen(real_path.c_str(), "wb");
		  if (!file)
		  {
			   send_response("553", "Fail to Open Output File");
			   return 0;
		  }
		  data_conn_fd = get_data_fd();
		  if (data_conn_fd < 0)
		  {
			   cout << "Data Connection Accept Error" << endl;
			   return 0;
		  }
		  cout << "Transfering..." << endl;
		  if (recv_file_data(file) < 0)
		  {
			   cout << "Receive File Data Error" << endl;
			   return 0;
		  }
		  cout << "Transfer Completed." << endl;
		  close(data_conn_fd);
		  fclose(file);
		  send_response("226", "Data Connection Closed.");
		  return 0;
	 }
	 else if (cmd == "QUIT")
	 {
		  send_response("221", "Bye~");
		  return 1;
	 }
	 else
	 {
		  send_response("502", "Not implemented");
		  return 0;
	 }
	 return 0;
}

int ClientHandler::enter_pasv_mode()
{
	 // old listen socket cannot be used again.
	 if (!is_passive)
		  close(data_listen_fd);
	 if (init_data_listen_fd() < 0)
		  return -1;

	 int port = get_listen_port();
	 if (port < 0) return -1;
	 unsigned addr = local_host_ip;

	 char addr_port_buf[128];
	 sprintf(addr_port_buf, "(%d,%d,%d,%d,%d,%d).",
	 		 (addr >> 24) & 0xFF,
	 		 (addr >> 16) & 0xFF,
	 		 (addr >> 8) & 0xFF,
	 		 addr & 0xFF,
	 		 (port >> 8) & 0xFF,
	 		 port & 0xFF);
	 send_response("227", "Entering Passive Mode " + string(addr_port_buf));
	 is_passive = true;
	 return 0;
}

int ClientHandler::get_listen_port()
{
	 struct sockaddr_in local_addr;
	 struct sockaddr* local_addr_ptr = (struct sockaddr*)&local_addr;
	 socklen_t addrlen = sizeof(local_addr);
	 memset(&local_addr, 0, addrlen);
	 if (getsockname(data_listen_fd, local_addr_ptr, &addrlen))
		  return -1;

	 int port = ntohs(local_addr.sin_port);
	 return port;
}

string ClientHandler::get_real_path(const string& args)
{
	 if (args[0] == '/')
		  return "." + args;
	 if (args == ".")
		  return "./";
	 if (args[0] == '.' && args[1] == '/')
		  return curr_dir + args.substr(2, args.size() - 2);
	 if (args[0] == '.' && args[1] == '.' && args[2] == '/')
	 {
		  if (curr_dir == "./")
			   return curr_dir;
		  int pos = curr_dir.substr(0, curr_dir.size() - 1).rfind('/');
		  return curr_dir.substr(0, pos + 1) + args.substr(3, args.size() - 3);
	 }
	 return curr_dir + args;
}

int ClientHandler::get_file_size(const char* fpath, string& size)
{
	 struct stat stat;
	 if (::stat(fpath, &stat))
		  return -1;
	 char buf[64];
	 sprintf(buf, "%d", (int)stat.st_size);
	 size = string(buf);
	 return 0;
}

int ClientHandler::get_data_fd()
{
	 if (!is_passive)
	 {
		  send_response("425", "Not in PASV mode");
		  return -1;
	 }
	 socklen_t sin_size = sizeof(struct sockaddr_in);
	 struct sockaddr_in data_addr;
	 int data_fd = accept(data_listen_fd, (struct sockaddr*)&data_addr, &sin_size);
	 if (data_fd < 0) return -1;
	 close(data_listen_fd);
	 is_passive = false;
	 send_response("125", "Data Connection Open");
	 return data_fd;
}

int ClientHandler::send_list_data(const string& path)
{
	 int pipefd[2];
	 if (pipe(pipefd)) return -1;

	 pid_t chpid = fork();
	 if (chpid < 0) return -1;
	 if (!chpid)
	 {
		  close(pipefd[0]);
		  if (dup2(pipefd[1], fileno(stdout)) < 0)
			   return -1;
		  if (dup2(pipefd[1], fileno(stderr)) < 0)
			   return -1;
		  execl("/bin/ls", "ls", "-l", path.c_str(), NULL);
		  exit(0);
	 }

	 close(pipefd[1]);
	 int buf_max_size = 1024;
	 char buf[buf_max_size + 1];
	 while (true)
	 {
		  int size = read(pipefd[0], buf, buf_max_size);
		  buf[size] = '\0';
		  if (size <= 0)
		  {
			   int waiting = waitpid(chpid, NULL, 0);
			   if (waiting < 0) return -1;
			   if (waiting == chpid) break;
		  }
		  else
		  {
			   send_crlf(string(buf));
		  }
	 }
	 close(pipefd[0]);
}

int ClientHandler::send_crlf(const string& str)
{
//	 works well without \r, it seems...
//	 send(data_conn_fd, str.c_str(), str.length(), 0);
//	 return 0;
	 int last_pos = 0;
	 for (int i = 1; i < str.size(); ++i)
	 {
		  if (str[i - 1] != '\r' && str[i] == '\n')
		  {
			   string data = str.substr(last_pos, i - last_pos) + "\r\n";
			   send(data_conn_fd, data.c_str(), data.length(), 0);
			   last_pos = i + 1;
		  }
	 }
	 if (last_pos < str.size())
	 {
		  string data = str.substr(last_pos, str.size() - last_pos);
		  send(data_conn_fd, data.c_str(), data.length(), 0);
	 }
	 return 0;
}

int ClientHandler::init_data_listen_fd()
{
	 data_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	 if (data_listen_fd < 0) return -1;
	 
	 struct sockaddr_in svrdata_addr;
	 struct sockaddr* svrdata_addr_ptr = (struct sockaddr*)&svrdata_addr;
	 memset(&svrdata_addr, 0, sizeof(svrdata_addr));
	 svrdata_addr.sin_family = AF_INET;
	 svrdata_addr.sin_addr.s_addr = INADDR_ANY;
	 svrdata_addr.sin_port = htons(0);
	 int optval = 1;
	 if (setsockopt(data_listen_fd, SOL_SOCKET, SO_REUSEADDR,
					(const char *)&optval, sizeof(optval)))
		  return -1;

	 if (bind(data_listen_fd, svrdata_addr_ptr, sizeof(svrdata_addr)) < 0)
		  return -1;

	 if (listen(data_listen_fd, 5))
		  return -1;
	 return 0;
}

int ClientHandler::send_file_data(FILE *pFile)
{
	 int buf_max_size = 1024;
	 char buf[buf_max_size + 1];
	 while (true)
	 {
	 	  int size = fread(buf, 1, buf_max_size, pFile);
	 	  if (size <= 0)
	 		   return 0;
	 	  buf[size] = '\0';
	 	  if (send(data_conn_fd, buf, size, 0) < 0)
	 	  {
	 		   cout << "Send Error" << endl;
	 		   return -1;
	 	  }
	 }
	 return 0;
}

int ClientHandler::recv_file_data(FILE *pFile)
{
	 int buf_max_size = 1024;
	 char buf[buf_max_size + 1];
	 while (true)
	 {
		  int size = recv(data_conn_fd, buf, buf_max_size, 0);
		  if (size <= 0) break;
		  buf[size] = '\0';
		  if (fwrite(buf, 1, size, pFile) != size)
			   break;
	 }
	 return 0;
}
