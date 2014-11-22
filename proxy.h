/*
 *Yi Shi andrew id: yishi
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#define START_SIZE 10
#define MAX_LINE 8192
#define DEFAULT_PORT 80
#define BUF_SIZE 4096
#define NO_LIST_SIZE 7

static const int index_host = 0;
static const int index_accept = 1;
static const int index_accept_encoding = 2;
static const int index_connection = 3;
static const int index_proxy_connection = 4;
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
//define additional header fields
static const char *connection = "Connection: close\r\n";
static const char *proxy_connection = "Proxy-Connection: close\r\n";
FILE* mylog;
int num_requests;
int sum_throughput;


//define client
typedef struct {
  int num_bitrate_options;
  int bitrate_capacity;
  int next_index_to_use;
  int client_fd;
  double throughput;
  int* bitrates;
  int current_bitrate;
}client;

//define client pool
typedef struct{
  int fd_max;
  fd_set read_set;
  fd_set read_readys;
  int read_num_ready;
  int read_index_max;
  client* clients[FD_SETSIZE];
}client_pool;



//define an read_io object to help read a buffer
typedef struct {
  char* buf;
}read_io;

//function declarations
read_io* init_read_io(char* given_content);
void free_reader(read_io* reader);
void read_one_line(read_io* reader, char* one_line);
int get_chunk_length(char* line);
void get_path_f4m_nolist(char* f4m_path, char* new_request);
void update_bitrate_chunk_request(char* chunk_path, int new_bitrate);
int get_hostname_len(char* uri);
void get_host_name(char* hostname_raw, char* hostname);
void remove_port_field(char* hostname);
void parse_request_port_hostname(char* uri, char* hostname,
			         char* request_content,
				 int* server_port);
char* get_f4m_ptr(char* content_path);
int close_socket(int sock);
char* read_response_from_server(int server_socket, char* buf,  int* count);
char* read_request_from_fd(client_pool* pool,
			   client* myclient, char* buf, int* count,
			   int browser_socket);
void first_request_line_combine(char* method, char* content_path,
				char* version, char* request_content);
int get_header(char* buf, char* header);
void get_request_header(int* header_indicators, char* buf,
			char* request_header);
void refill_miss_header(int* header_indicators, char* request_header,
			char* hostname);
void fill_bitrates(client* myclient, char* filename);
void parseMedia(client* myclient, xmlDocPtr doc, xmlNodePtr node);
int get_lowest_bit_rate(client* myclient);
void reset_client(client* myclient);
int get_best_bitrate(client* myclient, double throughput);
int initLogFile(char* logFileName);
void logger(const char* format, ...);
void handle_client_request(client_pool* pool, int browser_socket,
			   char* ip, char* fake_ip, float alpha, time_t epoch, char* logName);
int connect_server(char* ip, char* fake_ip, int port);
int connect_browser(char* listen_port);
void free_clients(client_pool* pool);
client* init_client(int client_fd);
void init_client_pool(int browser_socket, client_pool* pool);
void connectClient_add(int conn_fd, client_pool* pool);
