#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "utils.h"

#define MAX 50
#define BUFLEN 5000

void error(char *msg)
{
    perror(msg);
    exit(1);
}
// given an url, return the port specified or 80 otherwise
int getPort(char* buffer)
{
	char* copy = strdup(buffer);
	char* p ;
	int begin = 7;
	int port;
	if(copy == NULL) 
		return -1;
	copy = strstr(copy, "http://");
	copy = copy + begin; // skipping beginning (http://)

	if(strstr(copy, ":") == NULL) //there is no port, use port 80
		return 80;
	p = strtok(copy, ":"); // remove everything until :
	p = strtok(NULL, "."); // get everything until .
	port = atoi(p);
	
	return port;
}
// get the command from request line
char* get_command(char* buffer)
{
	char* copy = strdup(buffer);
	if(copy ==  NULL)
		return NULL;
	char* p;
	p = strtok(copy, " ");
	return p;
}
// get the host if url is given in absolute form
char* getHost_absolute(char* buffer)
{
	char* copy = strdup(buffer);
	char* p;
	int begin = 7; 
	if(copy == NULL) 
		return NULL;
	copy = strstr(copy, "http://");
	copy = copy + begin; // skipping beginning (http://)
	p = strtok(copy, ":/"); // get everything until port or resource
	return p;
}
// get the host if url is given in relative form
char* getHost_relative(char* buffer)
{
	char* copy =  strdup(buffer);
	char* p;
	if(copy == NULL)
		return NULL;
	copy = strstr(copy, "Host:");
	if(copy == NULL)
		return NULL;
	p = strtok(copy, "\n:"); // remove Host:
	p = strtok(NULL, "\n\r ");
	return p;
}
// get full url 
char* get_url(char* first_line, char* second_line)
{
	char* copy_first = strdup(first_line);
	if(copy_first == NULL)
		return NULL;
	char* copy_second = strdup(second_line);
	if(copy_second == NULL)	{
		free(copy_first);
		return NULL;
	}
	char* p;
	if(strstr(copy_first, "http://") != NULL) { // absolute form
		copy_first = strstr(copy_first, "http://");
		copy_first = copy_first + 7; // skipping beginning (http://)
		p = strtok(copy_first, " ");
		return p;
	}
	char* aux; // relative form
	char* url = malloc(BUFLEN * sizeof(char));
	aux = getHost_relative(second_line); // get host
	strcpy(url, aux);
	p = strtok(copy_second, " "); // get resource
	p = strtok(NULL, " ");
	strcat(url, p); // concat result
	return url;
}
// check if server send 200 message
int check_response(char* buffer)
{
	char* copy = strdup(buffer);
	char* p = strtok(copy, " ");
	p = strtok(NULL, " ");
	if(strcmp(p, "200") == 0)
		return 1;
	return 0;
}
// create cache for given command, link and filename
Cache* create_cache(char* command, char* link, char* filename)
{
	Cache* aux = calloc(sizeof(Cache), 1);
	if(!aux)
		return NULL;
	aux->command = strdup(command);
	if(!aux->command) {
		free(aux);
		return NULL;
	}
	aux->url = strdup(link);
	if(!aux->url) {
		free(aux);
		free(aux->command);
		return NULL;
	}
	aux->filename = strdup(filename);
	if(!aux->filename){
		free(aux->command);
		free(aux->url);
		free(aux);
		return NULL;
	}
	return aux;
}
// get cache's filename for given command and link
char* get_cache(char* command, char* link, Cache** saved, int count)
{
	int i = 0;
	for(i = 0; i < count; i++) {
		if(strcmp(saved[i]->command, command) == 0 
			&& strcmp(saved[i]->url, link) == 0)
			return saved[i]->filename;
	}
	return NULL;
}
// add new cache to list of cache files
void update_cache(Cache **saved, Cache* new, int count, int* total)
{
	if(count == *total) { // realloc 
		Cache** aux = realloc(saved, 2 * (*total));
		if(!aux)
			saved = aux;
		*total *= 2;
	}
	saved[count] = new;
}
int main(int argc, char *argv[])
{
	// used for select
	fd_set read_fds;
	fd_set tmp_fds;
	int fdmax;

	// used to send/receive messages from sockets
	char buffer[BUFLEN];
	char recvbuf[BUFLEN];

	// used to connect to client or http server
	char host_ip[32];
	char* hostname;
	struct sockaddr_in sockaddr;
	struct sockaddr_in client_addr;
	struct sockaddr_in http_addr;
	struct hostent *host;
	socklen_t aux;
	int httpsock;
	int port;
	int count;

	// used for the list of cache files
	char template[] = "/tmp/cacheXXXXXX";
	Cache** cache_list = calloc(sizeof(Cache*), MAX);
	if(!cache_list)
		error("Out of memory");
	int cache_count = 0;
	int total = MAX;
	char* command;
	int skip_cache = 0;
	char* name;

	int err, opt = 0; // used for debugging

	if(argc != 2) { 
		printf("Usage: %s <port>\n", argv[0]);
		exit(1);
	}
	//create read_fsd and tmp_fds  
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // opening TCP socket for listening
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		error("Error opening socket");
	}

	//setting struct sockaddr_in to listen to given port
	memset(&sockaddr, 0, sizeof(struct sockaddr_in));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(atoi(argv[1]));

	// bind socket to struct sockaddr_in
	err = bind(sockfd, (struct sockaddr*)&(sockaddr), sizeof(struct sockaddr_in));
	if(err < 0) {
		error("Error on binding");
	}

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *) &opt, sizeof(opt));

	listen(sockfd, MAX);

	// add new file descriptor to read_fds
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	while(1)	{
		tmp_fds = read_fds;
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) 
			error("ERROR in select");

		int i;
		for(i = 0; i <= fdmax; i++) {
			if(FD_ISSET(i, &tmp_fds)) {
				if(i == sockfd) { // a request to connect
					int newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &aux); 
					if (newsockfd < 0) {
						error("ERROR in accept");
					} else {
						//add new socket to read_fds
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}				
				} else { // a request from connected client
					memset(buffer, 0, BUFLEN);
					memset(&http_addr, 0, sizeof(struct sockaddr_in));
					// save the request in a temporary file
					char* fname = strdup(template);
					int aux = mkstemp(fname); // create and open temp file
					close(aux);
					FILE *requestfd = fopen(fname, "w");
					count = recv(i, buffer, BUFLEN, 0);
					while(count > 0) { // keep receiving data from client
						fputs(buffer, requestfd);
						if(strstr(buffer, "\r\n\r\n") != NULL) // received end of request
							break;
						memset(buffer, 0, BUFLEN);
						count = recv(i, buffer, BUFLEN, 0);
					}
					fclose(requestfd);
					requestfd = fopen(fname, "r");
					if(requestfd == NULL)
						error("Error opening file");
					fgets(buffer, BUFLEN, requestfd); // read request line
					char* first_line = strdup(buffer);
					char second_line[BUFLEN];
					if(strstr(buffer, "http://") == NULL) // url is in relative form
						fgets(second_line, BUFLEN, requestfd); // read host line in file received
					char* url = get_url(first_line, second_line); // get url
					command = get_command(first_line); // get command
					// check if http command is valid
					if(strncmp(buffer, "GET", 3) != 0 
						&& strncmp(buffer, "HEAD", 4) != 0
						&& strncmp(buffer, "POST", 4) != 0)	{
						// send error: command not implemented
						memset(buffer, 0, BUFLEN);
						sprintf(buffer, "501 Not Implemented\n");
						err  = send(i, buffer, strlen(buffer), 0);
						if(err < 0)
							error("ERROR in send");
					} else if ((name = get_cache(command, url, cache_list, cache_count)) != NULL) {
						// request is in cache
						FILE* fd = fopen(name, "r");
						if(fd == NULL)
							error("Error opening file");
						memset(buffer, 0, BUFLEN);
						while(fgets(buffer, BUFLEN,fd) != NULL) // send data back to client
							send(i, buffer, strlen(buffer), 0);
						close(i); // close connection with client
						FD_CLR(i, &read_fds);
					} else {

						// open TCP socket to send HTTP request
						if((httpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
					        printf("Eroare la  creare socket.\n");
					        exit(-1);
    					}
    					if(strcmp(command, "POST") == 0) // for POST, cache can't be saved
							skip_cache = 1; 
						// check if command used absolute path
						if(strstr(buffer, "http://") != NULL) {
							// get port
							port = getPort(buffer);
							http_addr.sin_family = AF_INET;
	    					http_addr.sin_port = htons(port);
							// get host
	    					hostname = getHost_absolute(buffer);
							host = gethostbyname(hostname);
							// get host's ip
							strcpy(host_ip, inet_ntoa((*(struct in_addr*) host->h_addr)));
							if (inet_aton(host_ip, & http_addr.sin_addr) <= 0) {
						        error("IP adress invalid");
						    }
						} else { // command used relative path
							http_addr.sin_family = AF_INET;
	    					http_addr.sin_port = htons(80);
							hostname = getHost_relative(second_line);
							if(hostname == NULL) {// if there is no Host: command is not valid
								memset(buffer, 0, BUFLEN);
								sprintf(buffer, "400 Bad Request\n");
								err = send(i, buffer, strlen(buffer), 0);
								if(err < 0)
									error("ERROR in send");
							}
							// get host
							host = gethostbyname(hostname);
							// get host's ip
							strcpy(host_ip, inet_ntoa((*(struct in_addr*) host->h_addr)));
							if (inet_aton(host_ip, & http_addr.sin_addr) <= 0) {
						        error("IP adress invalid");
						    }
						}

						// connect to http server
						err = connect(httpsock, (struct sockaddr * ) &http_addr, sizeof(http_addr));
						if (err < 0) {
				        	error("Error connecting to http server");
					    }
					    fseek(requestfd, 0, 0); // move to request file beginning
					    // send command to server
					    while(fgets(buffer, BUFLEN, requestfd) != NULL) {
						    err = send(httpsock, buffer, strlen(buffer), 0);
						    if(err < 0)
						    	error("Error sending to http server");
						}

					    count = recv(httpsock, recvbuf, BUFLEN, 0);
					    // save cache only if response is 200
					    if(check_response(recvbuf) != 1) { 
					    	skip_cache = 1;
					    }
					    // create a temporary file for cache
					    fname = strdup(template);
						aux = mkstemp(fname);
						close(aux);
						FILE *cachefd = fopen(fname, "w");
						while(count > 0) { // send data from server to client
							if( strstr(buffer, "Cache-Control: private") != NULL
								|| strstr(buffer, "no-cache") != NULL)
								skip_cache = 1;
							if(skip_cache != 1) // save in cache if allowed
								fputs(recvbuf, cachefd);
							err = send(i, recvbuf, count, 0);
							memset(recvbuf, 0, BUFLEN);
							count = recv(httpsock, recvbuf, BUFLEN, 0); 
						}
						if(skip_cache != 1) { // add cache in cache files list
							Cache* aux = create_cache(command, url, fname);
							if(!aux)
								error("Out of memory");
							update_cache(cache_list, aux, cache_count, &total);
							cache_count++;
						} 
						// close files
						fclose(cachefd);
						// close http socket
						close(httpsock);
						// close socket to client
						close(i);
						FD_CLR(i, &read_fds);
					}
				}
			}
		}
	}
}