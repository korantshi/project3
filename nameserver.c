/*
 *Yi Shi: andrew id: yishi
 */

#include "nameserver.h"
int main(int argc, char** argv){
  static proxy_pool pool_proxy;
  //client address
  struct sockaddr_in client_addr;
  socklen_t cli_size;
  //specify arguments:
  char* r_flag;
  char* log_file;
  char* ip;
  char* DNS_port;
  char* file_servers_list;
  char* file_LSA;
  r_flag = argv[1];
  log_file = argv[2];
  ip = argv[3];
  DNS_port = argv[4];
  file_servers_list = argv[5];
  file_LSA = argv[6];
  int proxy_socket = connect_proxy(DNS_port, ip);
  int client_fd;

  //set up the network:
  linked_list* list = initList();
  readLSA_store_all_ips(list, file_LSA);
  getALLNeighbors(list, file_LSA);

  while(1){
    pool_proxy.read_readys = pool_proxy.read_set;
    pool_proxy.read_num_ready = select(pool_proxy.fd_max + 1,
				       &(pool_proxy.read_readys),
				       NULL, NULL, NULL);
    if(pool_proxy.read_num_ready < 0){
      close(proxy_socket);
      fprintf(stderr, "select method error\n");
      free_proxy_clients(&pool_proxy);
      return EXIT_FAILURE;
    }
    //accept incoming connections from proxy:
    if(FD_ISSET(proxy_socket, &(pool_proxy.read_readys))){
      cli_size = sizeof(client_addr);
      if((client_fd = accept(proxy_socket, (struct sockaddr*) &client_addr, 
			     &cli_size)) == -1){
	close(proxy_socket);
	fprintf(stderr, "error in accepting incoming connections\n");
	free_proxy_clients(&pool_proxy);
	return EXIT_FAILURE;
      }
      //get ip of client
      int ipAddr = (&client_addr)->sin_addr.s_addr;
      char str_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &ipAddr, str_ip, INET_ADDRSTRLEN);
      add_connected_proxy(client_fd, str_ip, &pool_proxy);
      //check the client ip should be in the network
      if(searchNode(list, str_ip) == NULL){
	close(proxy_socket);
	close(client_fd);
	free_proxy_clients(&pool_proxy);
	fprintf(stderr, "error in network setup or proxy ip retrieval\n");
	return EXIT_FAILURE;
      }
    }
  }
}

//function to initialize the source node
void init_DJ_algorithm(linked_list* list, linked_list* unvisited,
		       char* proxy_ip){
  node* start = searchNode(list, proxy_ip);
  assert(start != NULL);
  start->distance_label = 0;
}

//function to find the node that is not in visited set and has the 
//minimum number of distance label
node* find_min(linked_list* visited, linked_list* list){
  int index;
  int min = INT_MAX;
  node* result = NULL;
  for(index = 0; index < (list->size); index++){
    node* element = getNodeAt(list, index);
    int distance = element->distance_label;
    if((distance < INT_MAX) && 
       (searchNode(visited, element->ip) == NULL)){
      min = distance;
      result = element;
    }
  }
  return result;
}



//function to implement the shortest path problem to get the shortest
//distance label
void DJ_algorithm(linked_list* list, linked_list* visited, 
		  node* current){
  node** adjacent_nodes = current->neighbors;
  int num_nodes_added = 0;
  int max = list->size;
  while(num_nodes_added < max){
    int index;
    for(index = 0; index < (current->num_neighbors); index++){
      node* target = adjacent_nodes[index];
      if(searchNode(visited, target->ip) != NULL){
	continue;
      }
      else{
	node* neighbor = searchNode(list, target->ip);
	if((1 + (current->distance_label)) < (neighbor->distance_label)){
	  neighbor->distance_label = 1 + (current->distance_label);
	}
      }
    }
    addNewNode(visited, current->ip, NULL, 0, current->distance_label);
    num_nodes_added = num_nodes_added + 1;
    current = find_min(visited, list);
    if(current == NULL){
      fprintf(stderr, "error in algorithm\n");
      return;
    }
    adjacent_nodes = current->neighbors;
  }
}

//function to find the list of content servers
void fill_content_servers(linked_list* content_servers, char* servers_name){
  FILE* servers = fopen(servers_name, "r");
  char* line = NULL;
  size_t len;
  ssize_t read_bytes;
  while((read_bytes = getline(&line, &len, servers)) != -1){
    addNewNode(content_servers, line, NULL, 0, INT_MAX);
  }
  fclose(servers);
}

//function to find the content server with shortest path
node* find_content_server_min_distance(linked_list* content_servers,
				       linked_list* visited){
  int min = INT_MAX;
  int index;
  node* result = NULL;
  for(index = 0; index < visited->size; index++){
    node* target =  getNodeAt(visited, index);
    if(searchNode(content_servers, target->ip) != NULL){
      if((target->distance_label) < min){
	min = target->distance_label;
	result = target;
      }
    }
  }
  return result;
}



//function to initalize the proxy client
proxy_client* init_proxy_client(int client_fd, char* ip){
  proxy_client* myproxy = malloc(sizeof(myproxy));
  myproxy->proxy_fd = client_fd;
  myproxy->ip = ip;
  return myproxy;
}


//function to initialize proxy pool
void init_proxy_pool(int proxy_socket, proxy_pool* pool){
  int itr;
  pool->read_index_max = -1;
  for(itr = 0; itr < FD_SETSIZE; itr++){
    (pool->proxy_clients)[itr] = init_proxy_client(-1, NULL);
  }
  pool->fd_max = proxy_socket;
  FD_ZERO(&(pool->read_set));
  FD_SET(proxy_socket, &(pool->read_set));
}

//function to free all proxy clients
void free_proxy_clients(proxy_pool* pool){
  int index = 0;
  client** myproxys = pool->proxy_clients;
  for(index = 0; index < FD_SETSIZE; index++){
    proxy_client* client_proxy = myproxys[index];
    free(client_proxy);
  }
}

//function to initialize the linked list:
linked_list* initList(){
  linked_list* result = malloc(sizeof(linked_list));
  result->head = NULL;
  result->size = 0;
  return result;
}

//function to delete a node from the linked list
void deleteNode(linked_list* list, node* target){
  node* node_check = target;
  node* head = list->head;
  if(head == NULL){
    return;
  }
  if(node_check->prev == NULL && node_check->next == NULL) {
        //Singleton
        list->head = NULL;
    } else if(node_check->prev == NULL && 
	      node_check->next != NULL) {
        //delete at the start
        list->head = node_check->next;
        node_check->next->prev = NULL;
    } else if(node_check->next == NULL) {
        //delete at the end
        node_check->prev->next = NULL;
    } else {
        node_check->prev->next = node_check->next;
        node_check->next->prev = node_check->prev;
    }
  free_neighbors(node->neighbors, list->num_neighbors);
  free(node_check);
  list->size = list->size - 1;
}

//function to free negihbor
void free_neighbors(node** neighbors, int num_neighbors){
  int index;
  for(index = 0; index < num_neighbors; index++){
    free(neighbors[index]);
  }
  free(neighbors);
}

//function to copy each neighboring node 
void copy_nodes(node** old_neighbors, int old_num, node** new_neighbors){
  int index;
  for(index = 0; index < old_num; index++){
    node* new = malloc(sizeof(node));
    new->ip = (old_neighbors[index])->ip;
    new_neighbors[index] = new;
  }
  //free old memory
  free_neighbors(old_neighbors, old_num);
}

//function to get node at the specified index:
node* getNodeAt(linked_list* list, int index){
  if(index < 0 || index >= list->size) {
        return NULL;
    } else {
        int itr;
        node *node_check = list->head;
        for(itr = 0; itr < index; itr++) {
	  node_check = node_check->next;
        }
        return node_check;
    }
}

//function to free the list
void freeList(linked_list* list){
  if(list != NULL){
    while((list->size) > 0){
      deleteNode(list, getNodeAt(list, 0));
    }
    free(list);
  }
}

//function to insert a new node to the list:
void addNewNode(linked_list* list, char* target_ip, node* adjacents, 
		int num, int distance){
  node* new = malloc(sizeof(node));
  new->ip = target_ip;
  new->neighbors = adjacents;
  new->num_neighbors = num;
  new->distance_label = distance;
  if (list->head == NULL) {
    new->prev = NULL;
    new->next = NULL;
    list->head = new;
  } else {
    node *ref = list->head;
    while(ref->next != NULL) {
      ref = ref->next;
    }
    ref->next = new;
    new->prev = ref;
    new->next = NULL;
  }
  list->size = list->size + 1;
}
//function to search for the target ip
node* searchNode(linked_list* list, char* target_ip){
  node* node_to_check = list->head;
  while(node_to_check != NULL){
    if(strcmp(target_ip, node_to_check->ip) == 0){
      return node_to_check;
    }
    else{
      node_to_check = node_to_check->next;
    }
  }
  return NULL;
}

//function to read LSA file to store all unqiue ips
void readLSA_store_all_ips(linked_list* list, char* LSA_file_name){
  FILE* LSA_file = fopen(LSA_file_name, "r");
  char* line = NULL;
  size_t len;
  ssize_t read_bytes;
  char sender[MAX_LINE];
  char seq_num[MAX_LINE];
  char neighbors[MAX_LINE];
  while((read_bytes = getline(&line, &len, LSA_file)) != -1){
    sscanf(line, "%s %s %s", sender, seq_num, neighbors);
    if(searchNode(list, sender)== NULL){
      addNewNode(list, sender, NULL, 0, INT_MAX);
    }
  }
  fclose(LSA_file);
}

//function to get neigbors for each ip
void getALLNeighbors(linked_list* list, char* LSA_file_name){
  int index;
  for(index = 0; index < list->size; index++){
    //read each node storing the ip
    node* target = getNodeAt(list, index);
    FILE* LSA_file = fopen(LSA_file_name, "r");
    int most_recent_seq_num = -1;
    char* line = NULL;
    size_t len;
    ssize_t read_bytes;
    char sender[MAX_LINE];
    char seq_num[MAX_LINE];
    char neighbors[MAX_LINE];
    char* target_neighbors;
    while((read_bytes = getline(&line, &len, LSA_file)) != -1){
      sscanf(line, "%s %s %s", sender, seq_num, neighbors);
      if(strcmp(sender, target->ip) == 0){
	int num_seq = atoi(seq_num);
	if(num_seq > most_recent_seq_num){
	  most_recent_seq_num = num_seq;
	  target_neighbors = neighbors;
	}
      }
    }
    fclose(LSA_file);

    //parse the target_neighbors separated by comma
    int length = DEGREE;
    node** node_neighbors = malloc(DEGREE * sizeof(node*));
    char* ptr = strtok(target_neighbors, ",");
    int neighbors_num = 0;
    while(ptr != NULL){
      if(neighbors_num >= length){
	node** temp = malloc(length * sizeof(node*));
	copy_nodes(node_neighbors, length, temp);
	node_neighbors = malloc(length * 2 * sizeof(node*));
	copy_nodes(temp, length, node_neighbors);
	length = length * 2;
      }
      node* toAdd = malloc(sizeof(node));
      toAdd->ip = ptr;
      node_neighbors[neighbors_num] = toAdd;
      neighbors_num = neighbors_num + 1;
      ptr = strtok(NULL, ",");
    }
    target->neighbors = node_neighbors;
    target->num_neighbors = neighbors_num;
  }
}


//function to add new connected proxy
void add_connected_proxy(int conn_fd, char* proxy_ip, proxy_pool* pool){
  int itr;
  pool->read_num_ready = (pool->read_num_ready) - 1;
  for(itr = 0; itr < FD_SETSIZE; itr++){
    //find an idle slot
    if(((pool->proxy_clients)[itr])->proxy_fd == -1){
      ((pool->proxy_clients)[itr])->proxy_fd = conn_fd;
      ((pool->proxy_clients)[itr])->ip = proxy_ip;
      //update the fd read set
      FD_SET(conn_fd, &(pool->read_set));
      //u[date max fd and max index
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
    fprintf(stderr, "cannot add more proxy connections\n");
  }
}

//function to give a proxy socket for listening incoming proxy connections
int connect_proxy(char* listen_port, char* ip){
  int proxy_socket;
  struct addrinfo hints;
  struct addrinfo* servinfo;
  struct addrinfo* pointer;
  int result;
  memset(&hints,0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  result = getaddrinfo(ip, listen_port, &hints, &servinfo);
  if(result != 0){
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
    exit(1);
  }
  //find and bind to incoming connection from proxy:
  for(pointer = servinfo; pointer != NULL; pointer = pointer->ai_next){
    proxy_socket = socket(pointer->ai_family, pointer->ai_socktype,
			  pointer->ai_protocol);
    if(proxy_socket == -1){
      perror("socket");
      continue;
    }
    if(bind(proxy_socket, pointer->ai_addr, pointer->ai_addrlen) == -1){
      close(proxy_socket);
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
  return proxy_socket;

}
