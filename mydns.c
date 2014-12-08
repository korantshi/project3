/*
 *Yi Shi: andrew id: yishi
 */
#include "mydns.h"
int resolve(const char* DNS_ip, const char* port_num_DNS, 
	    const struct addrinfo* hints,
	    struct addrinfo** result){
  unsigned char buf[BUF_SIZE];
  unsigned char* queryName;
  unsigned char* reader;
  struct sockaddr_in addr;
  struct resource_record response;
  struct sockaddr_in destination;
  struct header_dns* dns_header = NULL;
  struct question* ques_info = NULL;

  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  destination.sin_family = AF_INET;
  destination.sin_port = htones((uint16_t)(atoi(port_num_DNS)));
  destination.sin_addr.s_addr = inet_addr(DNS_ip);
  dns_header = (struct header_dns*)&buf;
  dns_header->id = (unsigned short)htons(getpid());
  dns_header->qr = 0;
  dns_header->opcode = 0;
  dns_header->aa = 0;
  dns_header->tc = 0;
  dns_header->rd = 0;
  dns_header->ra = 0;
  dns_header->z = 0;
  dns_header->ad = 0;
  dns_header->cd = 0;
  dns_header->rcode = 0;
  dns_header->q_count = htons(1);
  dns_header->ans_count = 0;
  dns_header->auth_count = 0;
  dns_header->add_count = 0;
  //point to the query section
  queryName = (unsigned char*)&buf[sizeof(struct header_dns)];
  getDNSName(queryName, "video.cs.cmu.edu");
  ques_info = (struct question*)&buf[sizeof(struct header_dns)+
				     (strlen((const char*)queryName) + 1)];
  ques_info->qtype = htons(1);
  ques_info->qclass = htons(1);
  int sendResult = sendto(sock, (char*)buf, sizeof(struct header_dns) + 
			  (strlen((const char*)queryName)+1) + 
			  sizeof(struct question), 0,
			  (struct sockaddr*)&destination, 
			  sizeof(destination));
  if(sendResult < 0){
    perror("error in send packet");
  }
}

//the function to convert domain name into dns format name
void getDNSName(unsigned char* dnsName, unsigned char* host){
  int lock = 0;
  int index = 0;
  strcat((char*)host, ".");
  for(index = 0; index < strlen((char*)host); index++){
    if(host[index] == "."){
      *dnsName++ = index - lock;
      for(;lock < index; lock++){
	*dnsName++ = host[lock];
      }
      lock++;
    }
  }
  *dnsName++ = '\0';
}
