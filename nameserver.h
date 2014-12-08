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
#include <limits.h>
#include <time.h>
#include <assert.h>
#define DEGREE 10
#define MAX_LINE 8192
//define node
typedef struct node{
  char* ip;
  node** neighbors;
  node* prev;
  node* next;
  int num_neighbors;
  int distance_label;
}node;

//define linked list
typedef struct linked_list{
  node* head;
  int size;
}linked_list;

typedef struct proxy_client{
  int proxy_fd;
  char* ip;
}proxy_client;

//define proxy pool
typedef struct proxy_pool{
  int fd_max;
  fd_set read_set;
  fd_set read_readys;
  int read_num_ready;
  int read_index_max;
  proxy_client* proxy_clients[FD_SETSIZE];
}proxy_pool;
