/*
 *Yi Shi: andrew id: yishi
 */

#include "proxy.h"
int main(int argc, char** argv){
  num_requests = 0;
  sum_throughput = 0;
  //define pool of clients
  static client_pool pool_clients;
  //define client address
  struct sockaddr_in client_addr;
  socklen_t cli_size;
  //specify the arguments:
  char* log_name;
  float alpha;
  char* listen_port;
  char* fake_ip;
  //  char* DNS_ip;
  // int DNS_port;
  char* www_ip;
  log_name = argv[1];
  alpha = atof(argv[2]);
  listen_port = argv[3];
  fake_ip = argv[4];
  // DNS_ip = argv[5];
  //DNS_port= argv[6]
  www_ip = argv[7];
  time_t epoch;
  time(&epoch);
  int browser_socket = connect_browser(listen_port);
  init_client_pool(browser_socket, &pool_clients);
  //initialize client_fd
  int client_fd;
  while(1){
    pool_clients.read_readys = pool_clients.read_set;
    pool_clients.read_num_ready = select(pool_clients.fd_max + 1,
					 &(pool_clients.read_readys),
					 NULL, NULL, NULL);
    if(pool_clients.read_num_ready < 0){
      close(browser_socket);
      fprintf(stderr, "select method error\n");
      return EXIT_FAILURE;
    }
    //accept incoming connections from browser
    if(FD_ISSET(browser_socket, &(pool_clients.read_readys))){
      cli_size = sizeof(client_addr);
      if((client_fd = accept(browser_socket, (struct sockaddr*) &client_addr, 
			     &cli_size)) == -1){
	close(browser_socket);
	fprintf(stderr, "error in accepting incoming connections\n");
	return EXIT_FAILURE;
      }
      connectClient_add(client_fd, &pool_clients);
    }
    handle_client_request(&pool_clients, browser_socket,
			  www_ip, fake_ip, alpha, epoch, log_name);
  }
  free_clients(&pool_clients);
  close_socket(browser_socket);

}

//helper function to start a reader object of read_io
read_io* init_read_io(char* given_content){
  read_io* reader = malloc(sizeof(read_io));
  reader->buf = given_content;
  return reader;
}

void free_reader(read_io* reader){
  free(reader);
}
//helper function to read one line
void read_one_line(read_io* reader, char* one_line){
  int num_characters = 0;
  char* bufptr = reader->buf;
  while((*bufptr) != '\r'){
    num_characters = num_characters + 1;
    bufptr = bufptr + 1;
  }
  num_characters = num_characters + 3;
  memcpy(one_line, reader->buf, num_characters);
  one_line[num_characters] = '\r';
  one_line[num_characters + 1] = '\n';
  one_line[num_characters + 2] = '\0';
  reader->buf = bufptr + 2;
}

//helper function to get the requested chunk length
int get_chunk_length(char* line){
  char header_name[MAX_LINE];
  char length[MAX_LINE];
  sscanf(line, "%s %s", header_name, length);
  int index;
  int num_digits = 0;
  int total_len = strlen(length);
  for(index = 0; index < total_len; index++){
    if(isdigit(length[index])){
      num_digits++;
    }
  }
  char num[num_digits + 1];
  strncpy(num, length, num_digits);
  num[num_digits] = '\0';
  int result = atoi(num);
  return result;
}

//helper function to chnage "xx.f4m get request" to "xx_nolist.f4m get request"

void get_path_f4m_nolist(char* f4m_path, char* new_request){
  strcpy(new_request, f4m_path);
  char* f4mptr = strstr(new_request, ".f4m");
  strcpy(f4mptr, "_nolist.f4m");
}

//helper function to change the bit rate
void update_bitrate_chunk_request(char* chunk_path, int new_bitrate){
  int length = strlen(chunk_path);
  char* ptr1 = &(chunk_path[length - 1]);
  char* ptr2 = strstr(chunk_path, "Seq");
  while(ptr1[0] != '/'){
    ptr1 = ptr1 - 1;
  }
  ptr1 = ptr1 + 1;
  int num_suffix = 0;
  while(ptr2[0] != '\0'){
    ptr2 = ptr2 + 1;
    num_suffix++;
  }
  char suffix_store[num_suffix + 1];
  strncpy(suffix_store, strstr(chunk_path, "Seq"), num_suffix);
  suffix_store[num_suffix] = '\0';
  //convert the bit rate into a string
  char str[15];
  sprintf(str, "%d", new_bitrate);
  strcat(str, suffix_store);
  strcpy(ptr1, str);
}


//helper function to get the host name length
int get_hostname_len(char* uri){
  int index = 0;
  int count = 0;
  int count_slash = 0;
  while(uri[index] != '\0'){
    if(uri[index] == '/'){
      count_slash = count_slash + 1;
     }
    if(count_slash == 3){
      return count;
    }
    count = count + 1;
    index = index + 1;
  }
  return count;
}

//helper function to remove http:// or https:// prefix of the 
//host name address
void get_host_name(char* hostname_raw, char* hostname){
  int len_prefix = 0;
  int index = 0;
  //get the length of the prefix
  while(hostname_raw[index] != '/'){
    len_prefix = len_prefix + 1;
    index = index + 1;
  }
  len_prefix = len_prefix + 2;
  index = len_prefix;
  int count = 0;
  while(hostname_raw[index] != '\0'){
    hostname[index - len_prefix] = hostname_raw[index];
    index++;
    count++;
  }
  hostname[count] = '\0';
}

//helper function to remove the port number field 
void remove_port_field(char* hostname){
  int index = 0;
  while(hostname[index] != ':'){
    index = index + 1;
  }
  //manually add the null terminator
  hostname[index] = '\0';
}

//function to parse the uri to obtain server port and hostname
void parse_request_port_hostname(char* uri, char* hostname,
			         char* request_content,
				 int* server_port){
  //define raw hostname
  char hostname_raw[MAX_LINE];
  //parse the uri for host name
  int len_hostname = get_hostname_len(uri);
  assert(len_hostname < MAX_LINE);
  strncpy(hostname_raw, uri, len_hostname);
  hostname_raw[len_hostname] = '\0';
  //delete the http:// or https:// prefix
  get_host_name(hostname_raw, hostname);

  //check whether the server port is specified
  char* ptr = memchr(hostname, ':', strlen(hostname));
  if(ptr != NULL){
    *server_port = atoi(ptr + 1);
    remove_port_field(hostname);
  }
  else{
    *server_port = DEFAULT_PORT;
  }
  //get the content path

  strcpy(request_content, uri + len_hostname);
  if(request_content[0] != '/'){
    request_content[0] = '/';
    request_content[1] = '\0';
  }
}

//helper function to determine whether the request is for f4m file

char* get_f4m_ptr(char* content_path){
  char* ptr = strstr(content_path, ".f4m");
  return ptr;
}

//helper function to close the socket
int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

//helper function to read the whole response from server

char* read_response_from_server(int server_socket, char* buf,  int* count){
  int bytes_rec = 0;
  ssize_t read_bytes;
  int mycount = *count;
  while((read_bytes = recv(server_socket, buf + bytes_rec, 
			   ((BUF_SIZE * mycount) - bytes_rec),
			   MSG_DONTWAIT)) >= 1){
    bytes_rec += read_bytes;
    if(bytes_rec ==  BUF_SIZE * mycount){
      //dynamically allocate the buffer when the old buffer is
      //full
      char* temp = malloc(BUF_SIZE * mycount);
      memcpy(temp, buf, BUF_SIZE * mycount);
      free(buf);
      buf = malloc(BUF_SIZE * (mycount + 1));
      memcpy(buf, temp, BUF_SIZE * mycount);
      free(temp);
      mycount = mycount + 1;
    }
  }
  //padd the null terminator
  if(buf[bytes_rec] != '\0'){
    buf[bytes_rec] = '\0';
  }
  //update count
  *count = mycount;
 //encounter error from reading client input
  if ((read_bytes == -1) && (errno != EAGAIN) && (errno != EWOULDBLOCK)){
    close_socket(server_socket);
    free(buf);
    fprintf(stderr, "Error reading from server.\n");
    return NULL;
  }
  //client shuts down
  else if(read_bytes == 0){
    if (close_socket(server_socket)){
      free(buf);
      fprintf(stderr, "Error closing server socket.\n");
      return NULL;
    }
  }
  return buf;
}

//helper function to read the whole request from browser
char* read_request_from_fd(client_pool* pool,
			   client* myclient, char* buf, int* count,
			   int browser_socket){
  int conn_fd = myclient->client_fd;
  int bytes_rec = 0;
  ssize_t read_bytes;
  int mycount = *count;
  while((read_bytes = recv(conn_fd, buf + bytes_rec, 
			   ((BUF_SIZE * mycount) - bytes_rec),
			   MSG_DONTWAIT)) >= 1){
    bytes_rec += read_bytes;
    if(bytes_rec ==  BUF_SIZE * mycount){
      //dynamically allocate the buffer when the old buffer is
      //full
      char* temp = malloc(BUF_SIZE * mycount);
      memcpy(temp, buf, BUF_SIZE * mycount);
      free(buf);
      buf = malloc(BUF_SIZE * (mycount + 1));
      memcpy(buf, temp, BUF_SIZE * mycount);
      free(temp);
      mycount = mycount + 1;
    }
  }
  //padd the null terminator
  if(buf[bytes_rec] != '\0'){
    buf[bytes_rec] = '\0';
  }
  //update count
  *count = mycount;
  //encounter error from reading client input
  if ((read_bytes == -1) && (errno != EAGAIN) && (errno != EWOULDBLOCK)){
    close_socket(conn_fd);
    close_socket(browser_socket);
    myclient->client_fd = -1;
    free(buf);
    fprintf(stderr, "Error reading from client.\n");
    return NULL;
  }
  //client shuts down
  else if(read_bytes == 0){
    myclient->client_fd = -1;
    FD_CLR(conn_fd, &(pool->read_set));
    if (close_socket(conn_fd)){
      close_socket(browser_socket);
      free(buf);
      fprintf(stderr, "Error closing client socket.\n");
      return NULL;
    }
  }
  return buf;
}

//helper function to combine the first request line
void first_request_line_combine(char* method, char* content_path,
				char* version, char* request_content){
  strcpy(request_content, method);
  strcat(request_content, " ");
  strcat(request_content, content_path);
  strcat(request_content, " ");
  strcat(request_content, version);
  assert(strchr(version, '\r') != NULL);
  assert(strchr(version, '\n') != NULL);
}

//function to get the header from client input
//return 1 if there is a header in the client request
//return 0 if not
int get_header(char* buf, char* header){
  if(strchr(buf, ':') != NULL){
    size_t len = (strchr(buf, ':') - buf);
    memcpy(header, buf, len);
    header[len] = '\0';
    return 1;
  }
  else{
    return 0;
  }
}

//function to obtain request headers and fill in header_indicators
//to indicate whether any header has been found or not
void get_request_header(int* header_indicators, char* buf,
			char* request_header){
  char header[MAX_LINE];
  int has_header = get_header(buf, header);
  if(has_header == 1){
    if(strcmp("Host", header) == 0){
      header_indicators[index_host] = 1;
      strcat(request_header, buf);
    }
    else if(strcmp("Accept", header) == 0){
      header_indicators[index_accept] = 1;
      strcat(request_header, accept_hdr);
    }
    else if(strcmp("Accept-Encoding", header) == 0){
      header_indicators[index_accept_encoding] = 1;
      strcat(request_header, accept_encoding_hdr);
    }
    else if(strcmp("Connection", header) == 0){
      header_indicators[index_connection] = 1;
      strcat(request_header, connection);
    }
    else if(strcmp("Proxy-Connection", header) == 0){
      header_indicators[index_proxy_connection] = 1;
      strcat(request_header, proxy_connection);
    }
    else{
      strcat(request_header, buf);
    }
  }
}

//function to check whether all required headers are filled, if not
//fill the header fields
void refill_miss_header(int* header_indicators, char* request_header,
			char* hostname){
  char header_buf[MAX_LINE];
  memset(header_buf, 0, sizeof(header_buf));
  if(header_indicators[index_host] == 0){
    strcat(header_buf, "Host: ");
    strcat(header_buf, hostname);
    strcat(header_buf, "\r\n");
    strcat(request_header, header_buf);
  }
  if(header_indicators[index_accept] == 0){
    strcat(request_header, accept_hdr);
  }
  if(header_indicators[index_accept_encoding] == 0){
    strcat(request_header, accept_encoding_hdr);
  }
  if(header_indicators[index_connection] == 0){
    strcat(request_header, connection);
  }
  if(header_indicators[index_proxy_connection] == 0){
    strcat(request_header, proxy_connection);
  }
}

//helper function to obtain bit rates of a manifest file
void fill_bitrates(client* myclient, char* filename){
  xmlDocPtr doc = xmlParseFile(filename);
  if(doc == NULL){
    fprintf(stderr, "Document not parsed successfully\n");
    exit(EXIT_FAILURE);
  }
  xmlNodePtr current_node = xmlDocGetRootElement(doc);
  if(current_node == NULL){
    fprintf(stderr, "document has zero length\n");
    exit(EXIT_FAILURE);
  }
  current_node = current_node->xmlChildrenNode;
  while(current_node != NULL){
  //find the media element
    if((!xmlStrcmp(current_node->name, (const xmlChar *)("media")))){
      parseMedia(myclient, doc, current_node);
    }
    current_node = current_node->next;
  }

}

void parseMedia(client* myclient, xmlDocPtr doc, xmlNodePtr node){
  xmlChar* key;
  node = node->xmlChildrenNode;
  while(node != NULL){
    if((!xmlStrcmp(node->name, (const xmlChar*)("bitrate")))){
      key = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
      int bitrate = atoi((char*)key);
      xmlFree(key);
      //store the bit rate to the client
      if((myclient->num_bitrate_options) < (myclient->bitrate_capacity)){
	int index = myclient->next_index_to_use;
	(myclient->bitrates)[index] = bitrate;
	myclient->next_index_to_use++;
	myclient->num_bitrate_options++;
      }
      else{
	size_t num_bytes = (myclient->bitrate_capacity) * sizeof(int);
	int* temp = malloc(num_bytes);
	memcpy(temp, myclient->bitrates, num_bytes);
	free(myclient->bitrates);
	myclient->bitrate_capacity = myclient->bitrate_capacity + START_SIZE;
	myclient->bitrates = malloc(
				    (myclient->bitrate_capacity) * 
				    sizeof(int));
	memcpy(myclient->bitrates, temp, num_bytes);
	free(temp);
	int index = myclient->next_index_to_use;
	(myclient->bitrates)[index] = bitrate;
	(myclient->next_index_to_use)++;
	(myclient->num_bitrate_options)++;
      }
    }
    node = node->next;
  }
}

//helper function to find the lowest bit rate at the start of steaming
int get_lowest_bit_rate(client* myclient){
  int* integer_array = myclient->bitrates;
  int index;
  int min = integer_array[0];
  for(index = 0; index < myclient->next_index_to_use; index ++){
    int element = integer_array[index];
    if(element < min){
      min = element;
    }
  }
  return min;
}

//helper function to reset fields of a client when it requests f4m file
void reset_client(client* myclient){
  myclient->num_bitrate_options = 0;
  int total = myclient->bitrate_capacity;
  myclient->bitrate_capacity = START_SIZE;
  myclient->throughput = 0.0;
  myclient->current_bitrate = -1;
  myclient->next_index_to_use = 0;
  free(myclient->bitrates);
  myclient->bitrates = malloc(START_SIZE * sizeof(int));
  int index;
  for(index = 0; index < total; index++){
    (myclient->bitrates)[index] = -1;
  }
}

//helper function to select the highest supported bit rate given the
//throughput
int get_best_bitrate(client* myclient, double throughput){
  double cutoff = throughput / 1.5;
  int max = -1;
  int* integer_array = myclient->bitrates;
  int index;
  for(index = 0; index < myclient->next_index_to_use; index ++){
    int element_int = (integer_array[index]);
    double element = (double)(integer_array[index]);
    if((element > (double)(max)) && (element <= cutoff)){
      max = element_int;
    }
  }
  return max;

}

//helper function to initiate the log file
int initLogFile(char* logFileName){
  mylog = fopen(logFileName, "w");
  if(mylog == NULL){
    return -1;
  }
  else{
    return 0;
  }
}

//helper function to enter entry in log file
void logger(const char* format, ...){
  va_list args;
  va_start(args, format);
  vfprintf(mylog, format, args);
  fflush(mylog);
  va_end(args);
}

//helper function to handle incoming client requests
void handle_client_request(client_pool* pool, int browser_socket,
			   char* ip, char* fake_ip, float alpha, time_t epoch,
			   char* log_name){
  int itr = 0;
  int conn_fd;
 
  //initiate log file
  if(initLogFile("log_name") == -1){
    fprintf(stderr, "log file fails to create\n");
    exit(EXIT_FAILURE);
  }
  while((itr <= (pool->read_index_max)) && ((pool->read_num_ready) > 0)){
    conn_fd = ((pool->clients)[itr])->client_fd;
    if((conn_fd > 0) && (FD_ISSET(conn_fd, &(pool->read_readys)))){
      char* buf = malloc(BUF_SIZE);
      int count = 1;
      buf = read_request_from_fd(pool, ((pool->clients)[itr]), buf, &count,
				 browser_socket);
    
      if(buf == NULL){
	exit(1);
      }
      client* myclient = (pool->clients)[itr];
      //parse the client input
      int header_indicators[5] = {0, 0, 0, 0, 0};
      read_io* reader = init_read_io(buf);
      char line[MAX_LINE];
      read_one_line(reader, line);
      char method[MAX_LINE];
      char uri[MAX_LINE];
      char version[MAX_LINE];
      char hostname[MAX_LINE];
      char content_path[MAX_LINE];
      char request_content[MAX_LINE];
      char request_headers[MAX_LINE];
      sscanf(line, "%s %s %s", method, uri, version);
      int server_port;
      parse_request_port_hostname(uri, hostname, content_path,
				  &server_port);
      first_request_line_combine(method, content_path,
				 version, request_content);
      char* f4m_ptr = get_f4m_ptr(content_path);
      //read each line of the request message
      while(((reader->buf)[0]) != '\r'){
	read_one_line(reader, line);
        get_request_header(header_indicators, line, request_headers);
      }
      //fill missing headers if there is any
      refill_miss_header(header_indicators, request_headers, hostname);
      //concat the content message body of request if there is any
      strcat(request_headers, reader->buf);

      //deal with manifest file request
      if(f4m_ptr != NULL){
	//reset client fields because f4m request indicates a new stream
	//is going to start
	reset_client(myclient);
	//firstly send "get f4m file" request to the server
	int server_socket = connect_server(ip, fake_ip, server_port);
	//send the message to the server
	fd_set write_set;
	FD_ZERO(&write_set);
	select(server_socket + 1, NULL, &write_set, NULL, NULL);
	if(FD_ISSET(server_socket, &write_set)){
	  char message[MAX_LINE];
	  strcpy(message, request_content);
	  strcat(message, request_headers);
	  int len_sent = send(server_socket, message, strlen(message) + 1,
			      0);
	  if(len_sent < 0){
	    close_socket(server_socket);
	    free(buf);
	    free_reader(reader);
	    fprintf(stderr, "error sending data to server\n");
	    exit(EXIT_FAILURE);
	  }
	  else{
	    //free the old memory
	    free(buf);
	    free_reader(reader);
	    //read from server response
	    fd_set read_set;
	    FD_ZERO(&read_set);
	    select(server_socket + 1, &read_set, NULL, NULL, NULL);
	    if(FD_ISSET(server_socket, &read_set)){
	      char* response = malloc(BUF_SIZE);
	      int count = 1;
              response = read_response_from_server(server_socket, response,  
						   &count);
	      if(response == NULL){
		exit(EXIT_FAILURE);
	      }
	      //we have to fetch the content message and parse the xml
	      //to get the bit rates
	      read_io* reader_response = init_read_io(response);
	      while((reader_response->buf)[0] != '\r'){
		read_one_line(reader_response, line);
	      }

	      //get the buf pointer to the content message
	      reader_response->buf = reader_response->buf + 2;
	      //write the content to a file
	      FILE* f4mfile = fopen("manifest.f4m", "w+");
	      if(fputs(reader_response->buf, f4mfile) < 0){
		free(response);
		free(reader_response);
		fprintf(stderr, "error in writing to f4m file\n");
		exit(EXIT_FAILURE);
	      }
	      else{
		//parse the xml file to obtain bitrates
		fill_bitrates(myclient, "manifest.f4m");
		int lowest_bitrate = get_lowest_bit_rate(myclient);
		myclient->current_bitrate = lowest_bitrate;
		//need to fetch the nolist f4m file
		int new_length = strlen(content_path) + NO_LIST_SIZE;
		char new_request[new_length + 1];
		get_path_f4m_nolist(content_path, new_request);
                first_request_line_combine(method, new_request,
					   version, request_content);
		//close the f4m file
		fclose(f4mfile);
		//recombine the message and send to server
		fd_set write_set;
		FD_ZERO(&write_set);
		select(server_socket + 1, NULL, &write_set, NULL, NULL);
		if(FD_ISSET(server_socket, &write_set)){
		  char message[MAX_LINE];
		  strcpy(message, request_content);
		  strcat(message, request_headers);
	          int len_sent = send(server_socket, message, 
				      strlen(message) + 1,
				      0);
		  if(len_sent < 0){
		    close_socket(server_socket);
		    free(response);
		    free_reader(reader_response);
		    fprintf(stderr, "error sending data to server\n");
		    exit(EXIT_FAILURE);
		  }
		  //free old memory
		  free(response);
		  free_reader(reader_response);
		  //read from server response
		  fd_set read_set;
		  FD_ZERO(&read_set);
		  select(server_socket + 1, &read_set, NULL, NULL, NULL);
		  if(FD_ISSET(server_socket, &read_set)){
		    char* second_response = malloc(BUF_SIZE);
		    int count = 1;
                    second_response = read_response_from_server(server_socket, 
								second_response,
								&count);
		    //this time, just send to client
		    int len_sent = send(conn_fd, second_response, 
					strlen(second_response) + 1,
					0);
		    if(len_sent < 0){
		      close_socket(server_socket);
		      free(second_response);
		      fprintf(stderr, "error sending data to client\n");
		      exit(EXIT_FAILURE);
		    }
		    free(second_response);
		    close_socket(server_socket);//end of iteration
		  }
		  else{
		    close_socket(server_socket);
		    fprintf(stderr, 
			    "select returns but server is not ready to read\n");
		    exit(EXIT_FAILURE);
		  }
		}
		else{
		  close_socket(server_socket);
		  free(response);
		  free(reader_response);
		  fprintf(stderr, 
                          "select returns but socket server is not ready to \
                          write\n");
		  exit(EXIT_FAILURE);
		}
	      }
	    }
	    else{
	      close_socket(server_socket);
	      fprintf(stderr, 
		      "select returns but socket server is not ready to read\n");
	      exit(EXIT_FAILURE);
	    }
	  }
	}
	else{ 
	  close_socket(server_socket);
	  free(buf);
	  free(reader);
	  fprintf(stderr, 
		  "select returns but socket server is not ready to write\n");
	  exit(EXIT_FAILURE);
	}

      }
      else{
	//we deal with get request for video chunk
	int server_socket = connect_server(ip, fake_ip, server_port);
	//get the start time
	time_t start;
        time(&start);
	//want to know the current bit rate
	int bitrate_current = myclient->current_bitrate;
	assert(bitrate_current > 0);
	update_bitrate_chunk_request(content_path, bitrate_current);
	//send message to server
	fd_set write_set;
	FD_ZERO(&write_set);
	select(server_socket + 1, NULL, &write_set, NULL, NULL);
	if(FD_ISSET(server_socket, &write_set)){
	  char message[MAX_LINE];
	  first_request_line_combine(method, content_path, version,
				     request_content);
	  strcpy(message, request_content);
	  strcat(message, request_headers);
	  int len_sent = send(server_socket, message, strlen(message) + 1,
			      0);
	  if(len_sent < 0){
	    close_socket(server_socket);
	    free(buf);
	    free_reader(reader);
	    fprintf(stderr, "error sending data to server\n");
	    exit(EXIT_FAILURE);
	  }
	  //free old memory
	  free(buf);
	  free(reader);
	  //read from server response
	  fd_set read_set;
	  FD_ZERO(&read_set);
	  select(server_socket + 1, &read_set, NULL, NULL, NULL);
	  if(FD_ISSET(server_socket, &read_set)){
	    char* response = malloc(BUF_SIZE);
	    int count = 1;
            response = read_response_from_server(server_socket, response,  
						 &count);
	    time_t final;
	    time(&final);

	    if(response == NULL){
	      exit(EXIT_FAILURE);
	    }
	    //get the chunk size by parsing the response
	    read_io* reader_response = init_read_io(response);
	    read_one_line(reader_response, line);
	    while(strstr(line, "Content-Length") == NULL){
	      read_one_line(reader_response, line);
	    }
	    //get the chunk size
	    int chunk_size = get_chunk_length(line);
	    double duration = difftime(start, final);
	    double T = ((double)chunk_size)/duration;
	    if(myclient->throughput == 0.0){
	      myclient->throughput = T;
	    }
	    else{
	      myclient->throughput = (((double)alpha) * T) +
		((1- (double)alpha) * myclient->throughput);
	    }
	    num_requests = num_requests + 1;
	    sum_throughput = sum_throughput + myclient->throughput;
	    double average = ((double)(sum_throughput)) / 
	      ((double)(num_requests));
	    //update bitrate
	    int new_bitrate = get_best_bitrate(myclient, 
					       myclient->throughput);
	    myclient->current_bitrate = new_bitrate;
	    int len_sent = send(conn_fd, response, strlen(response) + 1,
				0);
	    if(len_sent < 0){
	      close_socket(server_socket);
	      free(response);
	      fprintf(stderr, "error sending to client");
	      exit(EXIT_FAILURE);
	    }
	    time_t now;
	    time(&now);
	    double time_elapsed = difftime(epoch, now);
	    // add entry to the log file
	    logger("time = %f, duration = %f, tput = %f, avg-tput = %f,\
                   bitrate = %d, server_ip = %s, chunkname = %s\n", 
		   time_elapsed, duration,
		   myclient->throughput, average, myclient->current_bitrate,
		   ip, content_path);
	    free(response);
	    close_socket(server_socket);//end of iteration
	  }
	  else{
	    close_socket(server_socket);
	    fprintf(stderr, "select returns but socket server\
                            is not ready to read\n");
	  }
	}
	else{
	  free(buf);
	  free_reader(reader);
	  close_socket(server_socket);
	  fprintf(stderr, 
		  "select returns but socket server is not ready to write\n");
	  exit(EXIT_FAILURE);
	} 
      }
    }
    itr++;
  }

}

//helper function to set up a connection with the content server
//with specified IP address and port number
int connect_server(char* ip, char* fake_ip, int server_port){
  int server_socket;
  struct addrinfo hints;
  struct addrinfo* servinfo;
  struct addrinfo* pointer;
  struct addrinfo* pointer_local;
  struct addrinfo* localinfo;
  int result1;
  int result2;
  memset(&hints, 0, sizeof(hints));
  //either IP6 or IP4 address
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;
  //convert the server port to a string
  char str[15];
  sprintf(str, "%d", server_port);
  result1 = getaddrinfo(ip, str, &hints, &servinfo);
  result2 = getaddrinfo(fake_ip, "0", &hints, &localinfo);
  if(result1 != 0){
    //error code
    fprintf(stderr, "getaddrinfo for host: %s\n", gai_strerror(result1));
    exit(1);
  }

  if(result2 != 0){
    //error code
    fprintf(stderr, "getaddrinfo for local: %s\n", gai_strerror(result2));
    exit(1);
  }

  //loop until we find the first connectable 
  for(pointer = servinfo; pointer != NULL; pointer = pointer->ai_next){
    server_socket = socket(pointer->ai_family, pointer->ai_socktype,
			   pointer->ai_protocol);
    if(server_socket == -1){
      perror("socket");
      continue;
    }
    //bind to the fake ip
    for(pointer_local = localinfo; pointer_local != NULL; pointer_local =
	  pointer_local->ai_next){
      if(bind(server_socket, pointer_local->ai_addr, 
	      sizeof(pointer_local->ai_addr)) < 0){
	continue;
      }
      else{
	break;
      }
    }

    if(pointer_local == NULL){
      fprintf(stderr, "binding to fake ip failed\n");
      exit(2);
    }

    if(connect(server_socket, pointer->ai_addr, pointer->ai_addrlen) == -1){
      close(server_socket);
      perror("connect");
      continue;
    }
    //successfully connected
    break;
  }
  if(pointer == NULL){
    fprintf(stderr, "connection failed\n");
    exit(2);
  }
  freeaddrinfo(servinfo);
  freeaddrinfo(localinfo);
  return server_socket;
}

//helper function to set up connection with browser

int connect_browser(char* listen_port){
  int browser_socket;
  struct addrinfo hints;
  struct addrinfo* servinfo;
  struct addrinfo* pointer;
  int result;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  result = getaddrinfo(NULL, listen_port, &hints, &servinfo);
  if(result != 0){
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
    exit(1);
  }
  //find the incoming connection and bind to it
  for(pointer = servinfo; pointer != NULL; pointer = pointer->ai_next){
    browser_socket = socket(pointer->ai_family, pointer->ai_socktype,
			    pointer->ai_protocol);
    if(browser_socket == -1){
      perror("socket");
      continue;
    }
    if(bind(browser_socket, pointer->ai_addr, pointer->ai_addrlen) == -1){
      close(browser_socket);
      perror("bind");
      continue;
    }
    //successful connection
    break;
  }
  if(pointer == NULL){
    fprintf(stderr, "connection failed\n");
    exit(2);
  }
  freeaddrinfo(servinfo);
  return browser_socket;
}

//helper function to free clients

void free_clients(client_pool* pool){
  int index = 0;
  client** myclients = pool->clients;
  for(index = 0; index < FD_SETSIZE; index++){
    client* myclient = myclients[index];
    free(myclient->bitrates);
    free(myclient);
  }
}

//helper function to initiate a client
client* init_client(int client_fd){
  client* myclient = malloc(sizeof(client));
  myclient->client_fd = client_fd;
  myclient->next_index_to_use = 0;
  myclient->throughput = 0.0;
  myclient->num_bitrate_options = 0;
  myclient->bitrate_capacity = START_SIZE;
  myclient->bitrates = malloc(START_SIZE * sizeof(int));
  int index;
  for(index = 0; index < START_SIZE; index++){
    (myclient->bitrates)[index] = -1;
  }
  myclient->current_bitrate = -1;
  return myclient;
}

//helper function to initialize the read pool
void init_client_pool(int browser_socket, client_pool* pool){
  int itr;
  pool->read_index_max = -1;
  for(itr = 0; itr < FD_SETSIZE; itr++){
    (pool->clients)[itr] = init_client(-1);
  }
  pool->fd_max = browser_socket;
  FD_ZERO(&(pool->read_set));
  FD_SET(browser_socket, &(pool->read_set));
}

//helper function to add new connected client
void connectClient_add(int conn_fd, client_pool* pool){
  int itr;
  pool->read_num_ready = (pool->read_num_ready) - 1;
  for(itr = 0; itr < FD_SETSIZE; itr++){
    //find an idle slot
    if(((pool->clients[itr])->client_fd) == -1){
      ((pool->clients)[itr])->client_fd = conn_fd;
      //update the fd readset
      FD_SET(conn_fd, &(pool->read_set));

      //update max fd and max index
      if(itr > (pool->read_index_max)){
	pool->read_index_max = itr;
      }

      if(conn_fd > (pool->fd_max)){
	pool->fd_max = conn_fd;
      }
      break;
    }
  }

  if(itr == FD_SETSIZE){
    fprintf(stderr, "the client list is full, cannot add more\n");
  }
}
