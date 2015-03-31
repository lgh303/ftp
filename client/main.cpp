#include <cstdio>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <cstring>
#include <cstdlib>

using namespace std;

int client_fd;

void strip(string& str)
{
	 int lpos = str.find_first_not_of(" \t");
	 int rpos = str.find_last_not_of(" \t");
	 if (lpos == string::npos && rpos == string::npos)
	 {
		  str = "";
		  return;
	 }
	 str = str.substr(lpos, rpos - lpos + 1);
}

int read_command(string& cmd, string& args)
{
	 string str;
	 if (!getline(cin, str)) return -1;
	 strip(str);
	 int pos = str.find_first_of(" ");
	 if (pos == string::npos)
	 {
		  cmd = str;
		  args = "";
	 }
	 else
	 {
		  cmd = str.substr(0, pos);
		  args = str.substr(pos + 1, str.size() - pos - 1);
		  strip(cmd);
		  strip(args);
	 }
	 for (int i = 0; i < cmd.size(); ++i)
		  if (cmd[i] >= 'A' && cmd[i] <= 'Z')
			   cmd[i] += 'a' - 'A';
	 return 0;
}

void send_command(const string& str)
{
	 cout << "Request : " << str << endl;
	 int ret = send(client_fd, (str + "\r\n").c_str(), str.length() + 2, 0);
	 if (ret < 0)
	 {
		  cout << "Server quit!" << endl;
		  exit(0);
	 }
}

void get_response(string& code, string& msg)
{
	 char buf[256];
	 int len = recv(client_fd, buf, 256, 0);
	 if (len < 0)
	 {
		  cout << "Server Error. Quit." << endl;
		  exit(0);
	 }
	 buf[len - 2] = '\0';
	 string str(buf);
	 int pos = str.find(' ');
	 code = str.substr(0, pos);
	 msg = str.substr(pos + 1, str.size() - pos - 1);
	 cout << "Response : " << str << endl;
}

int get_data_conn(const string& ip, int port)
{
	 int data_fd = socket(AF_INET, SOCK_STREAM, 0);

	 struct sockaddr_in data_addr;
	 struct sockaddr* data_addr_ptr = (struct sockaddr*)&data_addr;
	 memset(&data_addr, 0, sizeof(data_addr));
	 data_addr.sin_family = AF_INET;
	 data_addr.sin_addr.s_addr = inet_addr(ip.c_str());
	 data_addr.sin_port = htons(port);
	 if (data_fd < 0)
	 {
		  cout << "Init Data Socket Error" << endl;
		  return -1;
	 }
	 if (connect(data_fd, (struct sockaddr*)&data_addr, sizeof(struct sockaddr)))
	 {
		  cout << "Connect Error" << endl;
		  return -1;
	 }
	 return data_fd;
}

void get_pasv_ip_port(string msg, string &ip, int &port)
{
	 int lpos = msg.find('(') + 1;
	 int rpos = msg.rfind(')');
	 int count = 0, lastsep = -1;
	 for (int i = lpos; i <= rpos; ++i)
		  if (msg[i] == ',' || msg[i] == ')')
		  {
			   msg[i] = '.';
			   ++count;
			   if (count == 4)
					ip = msg.substr(lpos, i - lpos);
			   else if (count == 5)
					port = atoi(msg.substr(lastsep + 1, i - lastsep - 1).c_str());
			   else if (count == 6)
			   {
					int tmp = atoi(msg.substr(lastsep + 1, i - lastsep - 1).c_str());
					port = (port << 8) + tmp;
			   }
			   lastsep = i;
		  }
	 cout << "IP : " << ip << " Port : " << port << endl;
}

string get_filename(const string& path)
{
	 int pos = path.rfind('/');
	 if (pos == string::npos || pos == path.size() - 1)
		  return path;
	 return path.substr(pos + 1, path.size() - pos - 1);
}

int console_wait()
{
	 cout << " ftp> ";
	 string cmd, args;
	 string code, msg;
	 
	 if (read_command(cmd, args) < 0)
	 {
		  cout << "read command error" << endl;
		  return -1;
	 }
	 if (cmd == "ls")
	 {
		  send_command("PASV");
		  get_response(code, msg);
		  if (code != "227") return 0;
		  string ip;
		  int port;
		  get_pasv_ip_port(msg, ip, port);
		  int data_conn_fd = get_data_conn(ip, port);
		  if (data_conn_fd < 0) return 0;

		  send_command("LIST " + args);
		  get_response(code, msg);
		  if (code == "125")
		  {
			   int buf_max_size = 1024;
			   char buf[buf_max_size + 1];
			   string data;
			   while (true)
			   {
					int size = recv(data_conn_fd, buf, buf_max_size, 0);
					if (size <= 0) break;
					buf[size] = '\0';
					data += string(buf);
			   }
			   cout << data;
		  }
		  get_response(code, msg);
		  close(data_conn_fd);
	 }
	 else if (cmd == "cd")
	 {
		  send_command("CWD " + args);
		  get_response(code, msg);
	 }
	 else if (cmd == "pwd")
	 {
		  send_command("PWD");
		  get_response(code, msg);
	 }
	 else if (cmd == "get")
	 {
		  string filename = get_filename(args);
		  FILE *wfile = fopen(filename.c_str(), "wb");
		  if (!wfile)
		  {
			   cout << "Open output file Error!" << endl;
			   return 0;
		  }

		  send_command("PASV");
		  get_response(code, msg);
		  if (code != "227") return 0;
		  string ip;
		  int port;
		  get_pasv_ip_port(msg, ip, port);
		  int data_conn_fd = get_data_conn(ip, port);
		  if (data_conn_fd < 0) return 0;

		  send_command("RETR " + args);
		  get_response(code, msg);
		  if (code == "550")
		  {
			   unlink(filename.c_str());
			   return 0;
		  }
		  if (code == "125")
		  {
			   int buf_max_size = 1024;
			   char buf[buf_max_size + 1];
			   while (true)
			   {
					int size = recv(data_conn_fd, buf, buf_max_size, 0);
					if (size <= 0) break;
					buf[size] = '\0';
					if (fwrite(buf, 1, size, wfile) != size)
						 break;
			   }
			   fclose(wfile);
		  }
		  get_response(code, msg);
		  close(data_conn_fd);
	 }
	 else if (cmd == "put")
	 {
		  FILE *rfile = fopen(args.c_str(), "rb");
		  if (!rfile)
		  {
			   cout << "Open Input file Error!" << endl;
			   return 0;
		  }

		  send_command("PASV");
		  get_response(code, msg);
		  if (code != "227") return 0;
		  string ip;
		  int port;
		  get_pasv_ip_port(msg, ip, port);
		  int data_conn_fd = get_data_conn(ip, port);
		  if (data_conn_fd < 0) return 0;

		  send_command("STOR " + get_filename(args));
		  get_response(code, msg);
		  if (code == "553") return 0;
		  if (code == "125")
		  {
			   int buf_max_size = 1024;
			   char buf[buf_max_size + 1];
			   while (true)
			   {
					int size = fread(buf, 1, buf_max_size, rfile);
					if (size <= 0) break;
					buf[size] = '\0';
					if (send(data_conn_fd, buf, size, 0) < 0)
					{
						 cout << "Send Error" << endl;
						 return -1;
					}
			   }
			   fclose(rfile);
			   // sender close first
			   close(data_conn_fd);
		  }
		  get_response(code, msg);
	 }

	 else if (cmd == "quit")
	 {
		  send_command("QUIT");
		  get_response(code, msg);
		  return -1;
	 }
	 else
	 {
		  cout << "Commands available : ls cd pwd get put quit" << endl;
	 }
	 return 0;
}

int main(int argc, char** argv)
{
	 if (argc != 3)
	 {
		  cout << "Usage : ./main IP Port" << endl;
		  return 0;
	 }
	 
	 int port = atoi(argv[2]);
	 client_fd = socket(AF_INET, SOCK_STREAM, 0);
	 if (client_fd < 0) return -1;

	 struct sockaddr_in client_addr;
	 struct sockaddr* client_addr_ptr = (struct sockaddr*)&client_addr;
	 memset(&client_addr, 0, sizeof(client_addr));
	 client_addr.sin_family = AF_INET;
	 client_addr.sin_addr.s_addr = inet_addr(argv[1]);
	 client_addr.sin_port = htons(port);
	 if (client_fd < 0)
	 {
		  cout << "Init Socket Error" << endl;
		  return -1;
	 }
	 if (connect(client_fd, (struct sockaddr*)&client_addr, sizeof(struct sockaddr)))
	 {
		  cout << "Connect Error" << endl;
		  return -1;
	 }
	 cout << "Connection Established Successfully!" << endl;
	 cout << "Waiting for greeting..." << endl;
	 string code, msg;
	 get_response(code, msg);
	 if (code == "220")
	 {
		  cout << "Greeting from Server : " << msg << endl;
		  while (true)
		  {
			   int ret = console_wait();
			   if (ret < 0) break;
		  }
	 }
	 close(client_fd);
}
