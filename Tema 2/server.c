#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "utils.h"

void error(char *msg)
{
    perror(msg);
    exit(1);
}

char** splitLine(char* line)
{
	char aux[100];
	int len = strlen(line);
	strcpy(aux, line);
	aux[len] = ' ';
	int count = 0;
	char **solution = malloc(sizeof(char *) * 3);
	char *p = strtok (aux, " \n");

	while(p != NULL) {
		solution[count] = strdup(p);
		count++;
		p = strtok (NULL, " \n");
	}

	return solution;
}
// get client's index in clients array
int getIndex(TClient clients[], char* client_number, int count)
{
	int i = 0;

	for(i = 0; i < count; i++)	{
		if(strcmp(clients[i].card_number, client_number) == 0 )
			return i;
	}

	return -1;
}
// check if given password is correct
int checkPassword(TClient client, char *password)
{
	if(strcmp(client.password, password) == 0)
		return 0;
	return -1;
}
// get client's index from active clients list
int getBySocket(TSession clients[], int socket, int count) 
{
	int i = 0;

	for(i = 0; i < count; i++) {
		if(clients[i].socket == socket)
			return i;
	}

	return -1;
}
int checkSold(TSession client, int sold) 
{
	if(client.current_sold < sold)
		return -1;
	return 0;
}
// check if card number is in clients' list
int card_correct(TClient clients[], int count, char* card)
{
	int i = 0;
	for(i = 0; i < count; i++)
		if(strcmp(clients[i].card_number, card) == 0)
			return 0;
	return -1;
}
// check if pin is correct
int pin_correct(TClient client, char* pin)
{
	if(strcmp(client.pin, pin) == 0)	
		return 0;
	return -1;
}
// check if client is in active clients' list
int isActive(TClient client, TSession active_clients[], int count)
{
	int i = 0;
	for(i = 0; i < count; i++)
		if(strcmp(client.card_number, active_clients[i].card_number) == 0)
			return i;

	return -1;	
}

int main (int argc, char *argv[])
{

	int clients_number;
	int activecl_count = 0;
	int i, j;
	int err, opt = 0;

	char buffer[BUFLEN];
	char operation[10];
	char **words;

	struct sockaddr_in UDPsockaddr, TCPsockaddr;
	struct sockaddr_in client_addr;
	socklen_t aux;

	fd_set read_fds;
	fd_set tmp_fds;
	int fdmax;

	if (argc < 3) {
    	fprintf(stderr,"Usage : %s port file\n", argv[0]);
    	exit(1);
    }

	// opening file with clients' information
	FILE *input = fopen(argv[2], "r");
	if(input == NULL) {
		printf("Error opening %s\n", argv[2]);
	}

	//reading number of clients
	fscanf(input, "%d", &clients_number);

	// save clients information
	TClient clients[clients_number];
	TSession clients_active[clients_number];

	for(i  = 0 ; i < clients_number; i++) {
		int card_number2;
		int pin2;
		char aux[7];
		fscanf(input, "%s %s %d %d %s %lf", clients[i].last_name, 
										    clients[i].first_name, 
										    &card_number2, 
										   	&pin2, 
										    clients[i].password, 
										    &clients[i].sold);

		// convert card_number and pin from int to string
		sprintf(clients[i].card_number, "%d", card_number2);
		clients[i].card_number[6] = '\0';
		sprintf(clients[i].pin, "%d", pin2);
		clients[i].pin[4] = '\0';

		// if card or pin had beginning zero, add them to the string
		while(strlen(clients[i].card_number) < 6)
		{
			memset(aux,0,sizeof(aux));
			aux[0] = '0';
			strcat(aux, clients[i].card_number);
			strcpy(clients[i].card_number,aux);
		}
		while(strlen(clients[i].pin) < 4){

			memset(aux,0,sizeof(aux));
			aux[0] = '0';
			strcat(aux, clients[i].pin);
			strcpy(clients[i].pin,aux);
		}

		clients[i].is_blocked = 0;
		clients[i].login_failed = 0;
	}

	for(i = 0; i < clients_number; i++) {
		clients_active[i].current_sold = 0;
		clients_active[i].is_blocked = 0;
	}


	//making read_fsd and tmp_fds  
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // add stdin to read_fsd
    FD_SET(0, &read_fds);

	// opening UDP socket
    int UDPsockfd = socket(PF_INET,SOCK_DGRAM, 0);
    if(UDPsockfd < 0) {
    	error("Error opening UDP socket");
    }

    // setting struct sockaddr_in to listen to given port
    memset(&UDPsockaddr, 0, sizeof(struct sockaddr_in));
	UDPsockaddr.sin_family = AF_INET;
	UDPsockaddr.sin_port= htons(atoi(argv[1]));

	// bind socket to struct sockaddr_in
	err = bind(UDPsockfd, (struct sockaddr*)&(UDPsockaddr), sizeof(struct sockaddr_in));
	if(err < 0)	{
		error("Error on binding");
	}

	// add new file descriptor to read_fds
	FD_SET(UDPsockfd, &read_fds);
	fdmax = UDPsockfd;

	// opening TCP socket
	int TCPsockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(TCPsockfd < 0) {
		error("Error opening TCP socket");
	}

	//setting struct sockaddr_in to listen to given port
	memset(&TCPsockaddr, 0, sizeof(struct sockaddr_in));
	TCPsockaddr.sin_family = AF_INET;
	TCPsockaddr.sin_port = htons(atoi(argv[1]));

	// bind socket to struct sockaddr_in
	err = bind(TCPsockfd, (struct sockaddr*)&(TCPsockaddr), sizeof(struct sockaddr_in));
	if(err < 0) {
		error("Error on binding");
	}

	setsockopt(TCPsockfd,SOL_SOCKET,SO_REUSEADDR,(void *) &opt, sizeof(opt));

	listen(TCPsockfd, clients_number);

	// add new file descriptor to read_fds
	FD_SET(TCPsockfd, &read_fds);
	if(TCPsockfd > fdmax){
		fdmax = TCPsockfd;
	}


	while(1)
	{
		tmp_fds = read_fds;
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) 
			error("ERROR in select");

		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {

				if(i == 0) {
					// reading from stdin
					scanf("%s", operation);

					if(strcmp(operation, "quit") != 0) {
						printf("Operation invalid for server\n. Operations supported: quit\n");
						continue;
					}

					memset(buffer, 0, BUFLEN);
					sprintf(buffer, "Server is closing\n");

					// send message to all open socket that server is closing
					for(j = 1; j <= fdmax; j++)	{
						if (FD_ISSET(j, &read_fds) &&  j != UDPsockfd && j!= TCPsockfd) {
							err = send(j, buffer, BUFLEN, 0);
							if(err < 0)
								error("ERROR in send");
						}
					}

					close(UDPsockfd);
					close(TCPsockfd);
					return 0;
				} 
				else if (i == TCPsockfd) {

					// a new client has connected
					int newsockfd = accept(TCPsockfd, (struct sockaddr *)&client_addr, &aux); 
					if (newsockfd < 0) {
						error("ERROR in accept");
					} 
					else {
						//add new socket to read_fds
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}
					printf("A new client has connected with address: %s and socket: %d\n",
														inet_ntoa(client_addr.sin_addr),
														newsockfd);
				}
				else if (i == UDPsockfd) {
					// a request to UNLOCK was received
					memset(buffer, 0, BUFLEN);
					err = recvfrom(i, buffer, BUFLEN, 0, (struct sockaddr *)&UDPsockaddr, &aux);
					if(err < 0)
						error("ERROR receive UDP");

					// get card number
					words = splitLine(buffer);
					char* number = words[1];
					fflush(stdout);

					//get client index in clients list
					int index = getIndex(clients, number, clients_number);
					memset(buffer, 0, BUFLEN);

					// card number wasn't found on  clients' list
					if(index == -1) {
						sprintf(buffer, "UNLOCK> -4: Nu exista numar card\n");
						// send response to client
						err = sendto(UDPsockfd, buffer, BUFLEN, 0, (struct sockaddr *)&UDPsockaddr, sizeof(UDPsockaddr));
						if(err < 0)
							error("ERROR send UDP");
					}
					else if(clients[index].is_blocked == 0)	{
						sprintf(buffer, "UNLOCK> -6: Operatie esuata\n");
						// send response to client
						err = sendto(UDPsockfd, buffer, BUFLEN, 0, (struct sockaddr *)&UDPsockaddr, sizeof(UDPsockaddr));
						if(err < 0)
							error("ERROR send UDP");
					}
					else {

						// ask for password
						sprintf(buffer, "UNLOCK> Trimite parola secreta\n");
						err = sendto(UDPsockfd, buffer, BUFLEN, 0, (struct sockaddr *)&UDPsockaddr, sizeof(UDPsockaddr));
						if(err < 0)
							error("ERROR send UDP");

						memset(buffer, 0, BUFLEN);
						// receive password
						err = recvfrom(UDPsockfd, buffer, BUFLEN, 0, (struct sockaddr *)&UDPsockaddr, &aux);
						if(err < 0)
							error("ERROR receive UDP");

						//check if password is correct
						words = splitLine(buffer);
						char *password = words[1];
						clients[index].is_blocked = checkPassword(clients[index], password);
						if(clients[index].is_blocked == 0)	{ // correct password
							sprintf(buffer, "UNLOCK> Client deblocat\n");
						} 
						else { // wrong password
							sprintf(buffer, "UNLOCK> -7: Deblocare esuata\n");
						}

						err = sendto(UDPsockfd, buffer, BUFLEN, 0, (struct sockaddr *)&UDPsockaddr, sizeof(UDPsockaddr));
						if(err < 0)
							error("ERROR send UDP"); 
					}
				}
				else {
					// received a request from connected clients using TCP socket
					err = recv(i, buffer, BUFLEN, 0);
					if(err <= 0){
						if (err == 0) {
							//conexiunea s-a inchis
							printf("Socket %d hung up\n", i);
						} else {
							error("ERROR in recv");
						}
						close(i); 
						FD_CLR(i, &read_fds); 
					}
					else {

						words = splitLine(buffer);
						strcpy(operation, words[0]);
						fflush(stdout);

						memset(buffer, 0, BUFLEN);
						if(strcmp(operation, "login") == 0) {
							// received a login operation
							char card[6], pin[4];
							strncpy(card, words[1], 6);
							strncpy(pin, words[2], 4);
							pin[4] = '\0';
							int id = getIndex(clients, card, clients_number);
							// check if card exists
							if(card_correct(clients, clients_number, card) < 0) {
								sprintf(buffer, "ATM> -4 : Numar card inexistent\n");
							} 
							else if(pin_correct(clients[id], pin) < 0) { // check if pin is correct
								clients[id].login_failed++;
								if(clients[id].login_failed < 3) {
									sprintf(buffer, "ATM> -3 : Pin gresit\n");
								} 
								else { //after 3 failed logins, block card
									sprintf(buffer, "ATM> -5 : Card blocat\n");
									clients[id].is_blocked = 1;
								}
							}
							else if(clients[id].is_blocked == 1) { // check if card is blocked
								sprintf(buffer, "ATM> -5 : Card blocat\n");
							}
							else {
								// check if client already is connected
								int active = isActive(clients[id], clients_active, activecl_count);
								if(active >= 0) { 
									sprintf(buffer, "ATM> -2 : Sesiune deja deschisa\n");
								} 
								else {
									// add client to active clients' list
									clients[id].login_failed = 0; 
									strcpy(clients_active[activecl_count].card_number, clients[id].card_number);
									clients_active[activecl_count].current_sold = clients[id].sold;
									clients_active[activecl_count].socket = i;
									clients_active[activecl_count].is_blocked = 0;
									activecl_count++;
									sprintf(buffer, "ATM> Welcome %s %s\n", clients[id].last_name, clients[id].first_name);
								}
							}			

						}
						else if(strcmp(operation, "logout") == 0) {
							// logout operation
							int clientid = getBySocket(clients_active, i, activecl_count);
							int j;
							// remove client from active clients' list
							for(j = clientid; j < activecl_count - 1; j++) {
								clients_active[j] = clients_active[j+1];
							}
							activecl_count--;
							sprintf(buffer, "ATM> Deconectare de la bancomat\n");
						}
						else if(strcmp(operation, "listsold") == 0) {
							// listsold operation
							int id = getBySocket(clients_active, i, activecl_count);
							sprintf(buffer, "ATM> %.2lf\n", clients_active[id].current_sold);
						}
						else if(strcmp(operation, "getmoney") == 0) {
							// get money operation
							int sum = atoi(words[1]);
							int id = getBySocket(clients_active, i, activecl_count);
							int client = getIndex(clients, clients_active[id].card_number, clients_number);

							if(sum % 10 != 0) // sum must be multiple of 10
								sprintf(buffer, "ATM> -9: Suma nu este multiplu de 10\n");
							else if(clients_active[id].current_sold < sum) 
								sprintf(buffer, "ATM> -8: Fonduri insuficiente\n");
							else {
								sprintf(buffer, "ATM> Suma %d retrasa cu succes\n", sum);
								clients_active[id].current_sold -= sum;
								clients[client].sold -= sum;
							}
						}
						else if (strcmp(operation, "putmoney") == 0) {
							//put money operation
							double sum = atof(words[1]);
							int id = getBySocket(clients_active, i, activecl_count);
							int client = getIndex(clients, clients_active[id].card_number, clients_number);

							sprintf(buffer, "ATM> Suma %.2lf depusa cu succes\n", sum);
							clients_active[id].current_sold += sum;
							clients[client].sold += sum;
						}
						else {
							// received quitting operation
							int clientid = getBySocket(clients_active, i, activecl_count);
							int j;
							// remove client from active clients' list
							for(j = clientid; j < activecl_count - 1; j++) {
								clients_active[j] = clients_active[j+1];
							}
							activecl_count--;
							continue;
						}
						// send message to client
						err = send(i, buffer, BUFLEN, 0);
						if(err < 0)
							error("ERROR in send");
					}
				}
			}
		}
	}
     
}