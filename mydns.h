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
#define BUF_SIZE 65536


struct header_dns{
  //identification number
  unsigned short id;
  //flag of recursion
  unsigned char rd;
  //flag of truncation
  unsigned char tc;
  //authoritive answer
  unsigned char aa;
  //operation code
  unsigned char opcode;
  //query response flag
  unsigned char qr;
  //response code
  unsigned char rcode;
  //checking code
  unsigned char cd;
  //authenticated data
  unsigned char ad;
  //Z value
  unsigned char z;
  //recursion availability
  unsigned char ra;
  //number of question entries
  unsigned short q_count;
  //number of answer entries
  unsigned short ans_count;
  //number of authority entries
  unsigned short auth_count;
  //number of resource entries
  unsigned short add_count;
};

//query structure
struct question{
  unsigned short qtype;
  unsigned short qclass;
};

//resource record structure's data fields
struct resource_data{
  unsigned short type;
  unsigned short class;
  unsigned int ttl;
  unsigned short data_len;
};

struct resource_record{
  unsigned char* name;
  struct resource_data* resource;
  unsigned char* resource_data;
};

//define query
typedef struct{
  unsigned char* name;
  struct question* ques;
}query;




