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

int main(int argc, char *argv[])
{
	int clientConnected;
	int pinConnected;
	int getSum;
	double putSum;
	int is_connected = 0;
	int err;

	char buffer[BUFLEN];
	char operation[10];
	char** words;
	char aux[7];
    char card_aux[7];
    char pin_aux[5];

	struct sockaddr_in UDPsockaddr, TCPsockaddr;
	fd_set read_fds;
	fd_set tmp_fds;
	int fdmax = 5;

	if (argc < 3) {
    	fprintf(stderr,"Usage : %s IP port\n", argv[0]);
    	exit(1);
    }

    // opening log file
    char filename[50];
    sprintf(filename, "client-%d.log", getpid());
    FILE *out = fopen(filename, "w");

    //making read_fsd and tmp_fds  
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // add stdin to read_fsd
    FD_SET(0, &read_fds);

    // open UDP socket
    int UDPsockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if(UDPsockfd < 0) {
    	error("Error opening UDP socket");
    }

	// setting struct sockaddr_in to listen to given port
    memset(&UDPsockaddr, 0, sizeof(struct sockaddr_in));
	UDPsockaddr.sin_family = AF_INET;
	UDPsockaddr.sin_port= htons(atoi(argv[2]));
	inet_aton(argv[1], &UDPsockaddr.sin_addr);

	// add new file descriptor to read_fds
	FD_SET(UDPsockfd, &read_fds);

	// opening TCP socket
	int TCPsockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(TCPsockfd < 0) {
		error("Error opening TCP socket\n");
	}

	//setting struct sockaddr_in to listen to given port
	memset(&TCPsockaddr, 0, sizeof(struct sockaddr_in));
	TCPsockaddr.sin_family = AF_INET;
	TCPsockaddr.sin_port = htons(atoi(argv[2]));
	inet_aton(argv[1], &TCPsockaddr.sin_addr);

	err = connect(TCPsockfd, (struct sockaddr*) &TCPsockaddr, sizeof(TCPsockaddr));
	if(err < 0) {
		error("Error connecting");
	}

	// add new file descriptor to read_fds
	FD_SET(TCPsockfd, &read_fds);

	while(1)
	{
		tmp_fds = read_fds;
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) 
			error("ERROR in select");

		if(FD_ISSET(0,&tmp_fds))
        {
            // stdin socket is set
            // read operation
            memset(buffer, 0 , BUFLEN);
            scanf("%s", operation);

            if(strcmp(operation, "login") == 0) {
            	// for login operation, read card number and pin
            	scanf("%d %d", &clientConnected, &pinConnected);

            	// convert integer to string
            	memset(card_aux, 0, strlen(card_aux));
            	memset(pin_aux, 0, strlen(pin_aux));
            	sprintf(card_aux, "%d", clientConnected);
				card_aux[6] = '\0';
				sprintf(pin_aux, "%d", pinConnected);
				pin_aux[4] = '\0';

				// if the pin and card had beginning zero
				// add them to the string
				while(strlen(card_aux) < 6)
				{
					memset(aux,0,sizeof(aux));
					aux[0] = '0';
					strcat(aux, card_aux);
					strcpy(card_aux,aux);
				}
				while(strlen(pin_aux) < 4){

					memset(aux,0,sizeof(aux));
					aux[0] = '0';
					strcat(aux, pin_aux);
					strcpy(pin_aux,aux);
				}

				// create message for server
            	sprintf(buffer, "login %s %s", card_aux, pin_aux);

            	if(is_connected == 1) {
            		printf("-2 : Sesiune deja deschisa \n");
            		continue;
            	}
            }
            else if(strcmp(operation, "logout") == 0) {
            	// logout operation, no other parameters needed
            	sprintf(buffer, "logout");

            	if(is_connected == 0) { //client is not connected
            		printf("-1 : Clientul nu este autentificat \n");
            		continue;
            	}
            	// client is no longer connected
            	is_connected = 0;
            }
            else if(strcmp(operation, "listsold") == 0) {
            	// listsold operation, no other parameters needed
            	sprintf(buffer, "listsold");

            	if(is_connected == 0) { //client is not connected
            		printf("-1 : Clientul nu este autentificat \n");
            		continue;
            	}
            } 
            else if(strcmp(operation, "getmoney") == 0) {

            	// for getmoney operation, read value
            	scanf("%d", &getSum);
            	sprintf(buffer, "getmoney %d", getSum);

            	if(is_connected == 0) { //client is not connected
            		printf("-1 : Clientul nu este autentificat \n");
            		continue;
            	}
            }
            else if(strcmp(operation, "putmoney") == 0) {

            	// for putmoney operation, read value
            	scanf("%lf", &putSum);
            	sprintf(buffer, "putmoney %.2lf", putSum);

            	if(is_connected == 0) { //client is not connected
            		printf("-1 : Clientul nu este autentificat \n");
            		continue;
            	}
            }
            else if(strcmp(operation, "unlock") == 0) {
           		// send unlock + card_number
            	sprintf(buffer, "unlock %s", card_aux);
            	// send to server using UDP socker
            	err = sendto(UDPsockfd, buffer, BUFLEN, 0, (struct sockaddr *)&UDPsockaddr, sizeof(UDPsockaddr));
            	if(err < 0)
            		error("ERROR in send UDP");
            	continue;
            }
            else if(strcmp(operation, "quit") == 0) {
            	// send to server message
            	sprintf(buffer, "Client is disconnecting\n");
            	if( (err = send(TCPsockfd, buffer, BUFLEN, 0)) < 0) {
                	printf("ERROR in send\n");
            	}
            	// exit client
            	close(UDPsockfd);
            	close(TCPsockfd);
            	break;
            }
            else {
            	printf("Operation is not correct \n");
            	continue;
            }

            // send operation to server using TCP socket
            if( (err = send(TCPsockfd, buffer, BUFLEN, 0)) < 0) {
                printf("ERROR in send\n");
            }
        } 
        else if(FD_ISSET(UDPsockfd, &tmp_fds)) {
        	// a request on UDP socket was received
        	fflush(stdout);
        	socklen_t aux;
        	memset(buffer, 0, BUFLEN);
        	err = recvfrom(UDPsockfd, buffer, BUFLEN, 0, (struct sockaddr *)&UDPsockaddr, &aux);
        	if(err < 0)
        	 	error("ERROR in recv UDP\n");

        	// print server's response
        	printf("%s", buffer);
        	fflush(stdout);
        	fprintf(out, "%s", buffer);
        	fflush(out);

        	words = splitLine(buffer);

        	// server didn't ask for password
        	if(strcmp(words[1],"Trimite") != 0)
        		continue;

        	// read password from stdin
        	char password[16];
        	memset(buffer, 0, BUFLEN);
        	scanf("%s", password);
        	sprintf(buffer, "%d %s", clientConnected, password);

        	// send password to server
        	err = sendto(UDPsockfd, buffer, BUFLEN, 0, (struct sockaddr *)&UDPsockaddr, sizeof(UDPsockaddr));
            if(err < 0)
            	error("ERROR in send UDP");

            // wait for response from server
            memset(buffer, 0, BUFLEN);
            err = recvfrom(UDPsockfd, buffer, BUFLEN, 0, (struct sockaddr *)&UDPsockaddr, &aux);
        	if(err < 0)
        	 	error("ERROR in recv UDP\n");

        	// print server's response
           	printf("%s", buffer);
           	fflush(stdout);

        	fprintf(out, "%s", buffer);
        	fflush(out);
        }
        if(FD_ISSET(TCPsockfd, &tmp_fds)) {

            // server sent a message using TCP socket
            memset(buffer, 0 , BUFLEN);
            err = recv(TCPsockfd, buffer, BUFLEN, 0) ;
            if(err < 0)
              error("ERROR in recv TCP");

          	// print server's response
          	printf("%s", buffer);
          	fflush(stdout);
            fprintf(out, "%s", buffer);
            fflush(out);

            if(buffer != NULL) {
            	char **words = splitLine(buffer);
            	// got confirmation that login was successful
            	if(strcmp(words[1], "Welcome") == 0)
            		is_connected = 1;
            	// got a message that server is closing
            	if(strcmp(words[0], "Server") == 0) {
            		close(UDPsockfd);
            		close(TCPsockfd);
            		break;
            	}

        	}
        }
	}

}