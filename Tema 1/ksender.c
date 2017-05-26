#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"
#include "messages.h"

#define HOST "127.0.0.1"
#define PORT 10000

// function to resend message until receiver send ACK
int resend_message(int *current_seq, kermit *pck, kermit *pckR)
{
    msg header;
    msg *r;

    // update SEQ
    (*current_seq) += 2;
    pck->SEQ = *current_seq % 64;
    *pck = set_crc(*pck);

    // send message
    memcpy(&(header.payload), pck, sizeof(kermit));
    header.len = sizeof(kermit);
    send_message(&header);

    // wait for response
    r = receive_message_timeout(15000);
    if( r == NULL) // no response in 5 seconds
    {
        // resend last message max. 3 times
        r = resend_timeout(header);
        if(r == NULL) //timeout: interrupt the connection
            return -1;
    }

    // save message in kermit
    memcpy(pckR, r->payload, sizeof(kermit));

    // if response's SEQ isn't the one expected, wait for correct SEQ
    while(pckR->SEQ % 64 != (*current_seq  + 1) % 64)
    {
        r = receive_message_timeout(5000);
        if (r == NULL) // timeout
        {
            // resend the message maximum 3 times
            r = resend_timeout(header);

            if(r == NULL) // interrupt connection
            {    
                printf("[sender] exiting with timeout\n");
                return -1;
            }            
            memset(pckR, 0, sizeof(*pckR));
            memcpy(pckR, r->payload, sizeof(kermit));
        }

        // save payload in kermit
        memset(pckR, 0, sizeof(*pckR));
        memcpy(pckR, r->payload, sizeof(kermit));
    }

    // if response is still NAK
    while(pckR->type == 'N')
    {
        // update SEQ
        (*current_seq) += 2;
        pck->SEQ = *current_seq % 64;
        *pck = set_crc(*pck);

        // resend message
        memcpy(&(header.payload), pck, sizeof(kermit));
        header.len = sizeof(kermit);
        send_message(&header);

        // wait for response
        r = receive_message_timeout(5000);
        if( r == NULL) // no response in 5 seconds
        {
            //send again last message max. 3 times
            r = resend_timeout(header); 
            if(r == NULL) //if timeout, interrupt the connection
                return -1;
        }

        // save in kermit response
        memcpy(pckR, r->payload, sizeof(kermit));

        // if response's SEQ isn't the one expected, wait for correct SEQ
        while(pckR->SEQ % 64 != (*current_seq  + 1) % 64)
        {
            r = receive_message_timeout(5000);
            if (r == NULL) // timeout
            {
                // resend the message maximum 3 times
                r = resend_timeout(header);

                if(r == NULL) // interrupt connection
                {        
                    printf("[sender] exiting with timeout\n");
                    return -1;
                }
                    
                memset(pckR, 0, sizeof(*pckR));
                memcpy(pckR, r->payload, sizeof(kermit));
            }

            // save payload in kermit
            memset(pckR, 0, sizeof(*pckR));
            memcpy(pckR, r->payload, sizeof(kermit));
        }
    }

    return 0;
}

int main(int argc, char** argv) {

    msg header;
    int log_count;
    int current_seq = 0;
    int i = 1;
    msg file;

    kermit pck;
    kermit pckR;
    kermit file_pck, file_name;;
    struct stat aux;

    int fd; 
    long file_dimension;
    int read_dimension;
    char buffer[250];

    init(HOST, PORT);

    // initially we create a package of type S
    pck = initS();
    pck = set_crc(pck);

    // sending package S
    memcpy(&(header.payload), &pck, sizeof(kermit));
    header.len = sizeof(kermit);    
    send_message(&header);

    // waiting for response
    msg *r = receive_message_timeout(15000);
    if( r == NULL) // no response after 5 seconds
    {
        // resend last message maxi. 3 times
        r = resend_timeout(header);
        if(r == NULL) // interrupt connection if timeout
        {
            printf("[%s] exiting with timeout\n", argv[0]);
            return -1;
        }
    }

    memcpy(&pckR, r->payload, sizeof(kermit));

    if(pckR.type == 'Y') // received ACK
    {
        current_seq += 2;
    } 
    else // data modifies
    {
        // resend until ACK is received
        log_count = resend_message(&current_seq,&pck, &pckR);
        if(log_count == -1) // timeout
            return -1; // interrupt the connection
        current_seq += 2;
    }

    for(i = 1; i < argc; i++)
    {
        // create kermit with file name
        memset(&(file.payload),0, sizeof(file.payload));
        memset(&file_name, 0, sizeof(kermit));
        file_name = initF(argv[i], strlen(argv[i]));
        file_name.SEQ = current_seq;
        file_name = set_crc(file_name); // get control sum

        // send file
        memcpy(&(file.payload), &file_name, sizeof(kermit));
        file.len = sizeof(kermit);
        send_message(&file);

        // waiting for response
        r = receive_message_timeout(5000); 
        if (r == NULL) //timeout
        {
            // resend the message maximum 3 times
            r = resend_timeout(file);

            if(r == NULL) //interrupt connection
            {
                printf("[%s] exiting with timeout\n", argv[0]);
                return -1;
            }

        }

        // save response in kermit
        memset(&pckR, 0, sizeof(kermit));
        memcpy(&pckR, r->payload, sizeof(kermit));

        if(pckR.type == 'Y') // received ACK
        {
            current_seq += 2;
        } 
        else //if NAK is received, resend message until ACK
        {
            log_count = resend_message(&current_seq,&pck, &pckR);
            if(log_count == -1)
            {
                printf("[%s] exiting with timeout\n", argv[0]);
                return -1;
            }
            current_seq += 2;
        }

        fd = open(argv[i], O_RDONLY); // open file
        if(fd < 0)
        {
            printf("Error opening file!\n");
            return -1;
        }

        fstat(fd,&aux);
        file_dimension = aux.st_size; // get file dimension


        while(file_dimension > 0)
        {
            // read data from file in buffer
            memset(buffer, 0, sizeof(buffer));
            memset(&file, 0, sizeof(file));
            memset(&file_pck, 0, sizeof(kermit));
            read_dimension = read(fd, buffer, sizeof(buffer));

            // create a package of type D
            file_pck = initD(buffer, read_dimension);
            file_pck.SEQ = current_seq % 64;
            file_pck = set_crc(file_pck);

            // create message
            memcpy(&file.payload, &file_pck, sizeof(file_pck));
            file.len = sizeof(file_pck);

            // send message
            send_message(&file);

            // waiting for response
            r = receive_message_timeout(5000); 
            if (r == NULL) // timeout
            {
                // resend the message maximum 3 times
                r = resend_timeout(file);

                if(r == NULL) // interrupt connection
                {
                    printf("[%s] exiting with timeout\n", argv[0]);
                    return -1;
                }
            }

            // save payload in kermit
            memset(&pckR, 0, sizeof(pckR));
            memcpy(&pckR, r->payload, sizeof(kermit));

            // if SEQ received isn't correct, wait for a correct package
            while(pckR.SEQ % 64 != (current_seq  + 1) % 64)
            {
                r = receive_message_timeout(5000);
                if (r == NULL) // timeout
                {
                    // resend the message maximum 3 times
                    r = resend_timeout(file);

                    if(r == NULL) // interrupt connection
                    {
                        printf("[%s] exiting with timeout\n", argv[0]);
                        return -1;
                    }

                    memset(&pckR, 0, sizeof(pckR));
                    memcpy(&pckR, r->payload, sizeof(kermit));
                }

                // save payload in kermit
                memset(&pckR, 0, sizeof(pckR));
                memcpy(&pckR, r->payload, sizeof(kermit));
            }

            if(pckR.type == 'Y') // received ACK
            {                
                //current_seq = pckR.SEQ % 64;
                current_seq += 2;
            } 
            else //if NAK is received, resend message until ACK
            {
                //current_seq += 2;
                log_count = resend_message(&current_seq,&file_pck, &pckR);
                if(log_count == -1)
                    return -1;
                current_seq += 2;
            }

            file_dimension -= read_dimension;

        }

        // create EOF package
        kermit eof_pck = initZ();
        eof_pck.SEQ = current_seq % 64;
        eof_pck = set_crc(eof_pck);

        // create message
        memcpy(&(file.payload), &eof_pck, sizeof(kermit));
        file.len = sizeof(kermit);

        // send message
        send_message(&file);

        // waiting for response
        r = receive_message_timeout(5000); 
        if (r == NULL) //timeout
        {
            // resend the message maximum 3 times
            r = resend_timeout(file);

            if(r == NULL) //interupt connection
            {
                printf("[%s] exiting with timeout\n", argv[0]);
                return -1;
            }

        }

        memset(&pckR, 0, sizeof(kermit));
        memcpy(&pckR, r->payload, sizeof(kermit));

        if(pckR.type == 'Y')
        {
            //current_seq = pckR.SEQ % 64;
            current_seq += 2;
        } 
        else //if NAK is received, resend message until ACK
        {
            log_count = resend_message(&current_seq,&eof_pck, &pckR);
            current_seq += 2;
            if(log_count == -1)
            {
                printf("[%s] exiting with timeout\n", argv[0]);
                return -1;
            }
        }


    }

    // create kermit of type B
    kermit eot_pck = initB();
    eot_pck.SEQ = current_seq % 64 ;
    eot_pck = set_crc(eot_pck);

    // create message
    memcpy(&(file.payload), &eot_pck, sizeof(kermit));
    file.len = sizeof(kermit);

    // send message
    send_message(&file);

    // waiting for response
    r = receive_message_timeout(5000); 
    if (r == NULL) //timeout
    {
            // resend the message maximum 3 times
            r = resend_timeout(file);

            if(r == NULL) //interrupt connection
                return -1;

        }

        memset(&pckR, 0, sizeof(kermit));
        memcpy(&pckR, r->payload, sizeof(kermit));

        if(pckR.type == 'Y')
        {
            //current_seq = pckR.SEQ % 64;
            current_seq += 2;
        } 
        else //if NAK is received, resend message until ACK
        {
            log_count = resend_message(&current_seq,&eot_pck, &pckR);
            current_seq += 2;
            if(log_count == -1) // timeout
            {
                printf("[%s] exiting with timeout\n", argv[0]);
                return -1;
            }
        }


    return 0;
}
