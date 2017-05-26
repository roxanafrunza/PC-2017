#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"
#include "messages.h"

#define HOST "127.0.0.1"
#define PORT 10001

// send ACK for message of type S
kermit send_ACK_S(int *current_seq, int SEQ)
{
    msg t;
    kermit pckR;

    memset(&t, 0, sizeof(t));
    pckR = initYS(); // confirmation package

    // update SEQ 
    (*current_seq)++; // increment
    pckR.SEQ = (*current_seq) % 64; // set seq for response

    // send confirmation
    memcpy(&t.payload, &pckR, sizeof(pckR));
    t.len = sizeof(kermit);
    send_message(&t);

    return pckR;
}
// send ACK for message 
kermit send_ACK(int *current_seq, int SEQ)
{
    msg t;
    kermit pckR;

    memset(&t, 0, sizeof(t));
    pckR = initY(); // confirmation package

    // update SEQ 
    (*current_seq) += 2; // increment
    pckR.SEQ = (*current_seq) % 64; // set seq for response

    // send confirmation
    memcpy(&t.payload, &pckR, sizeof(pckR));
    t.len = sizeof(kermit);
    send_message(&t);

    return pckR;
}
// send NAK for message
kermit send_NAK(int *current_seq, int SEQ)
{
    msg t;
    kermit pckR;
    
    memset(&t, 0, sizeof(t));
    pckR = initN();
            
    // update SEQ
    (*current_seq) += 2; // increment
    pckR.SEQ = (*current_seq) % 64; // set seq for respionse

    // send NAK package
    memcpy(&t.payload, &pckR, sizeof(kermit));
    t.len = sizeof(kermit);
    send_message(&t);

    return pckR;
}

int main(int argc, char** argv) {
    msg *r, t;
    msg *header;
    int fd;
    kermit pck;
    kermit pckR;
    int current_seq = 0;
    char name_buffer[100];
    unsigned short crc;
    int len_sum;

    init(HOST, PORT);


    // receiving a package of type S
    header = receive_message_timeout(5000);
    if(header == NULL)
    {
        header = receive_message_timeout(5000);
        if(header == NULL)
        {
            header = receive_message_timeout(5000);
            if(header == NULL)
            {
                printf("[%s] exiting with timeout\n", argv[0]);
                return -1;
            }
        }
    }

    // save payload in kermit
    memcpy(&pck, header->payload, header->len);
    int data_len = calculate_data_len(pck);

    // calculate control sum
    len_sum = calculate_check_len(pck, data_len);
    crc = crc16_ccitt(header->payload, len_sum);

    // all data is correct
    if(crc == pck.check)
    {
        pckR = send_ACK_S(&current_seq, pck.SEQ);
    }   
    else  // data is modified
    {
        // sending NAK
        pckR = send_NAK(&current_seq, pck.SEQ);
        memcpy(&(t.payload), &pckR, sizeof(kermit));
        t.len = sizeof(pckR);

        //keep resending NAK until all data is correct
        while(pckR.type == 'N')
        {
            // receiving message from sender
            header = receive_message_timeout(5000);
            
            if(header == NULL)
            {
                header = resend_timeout(t);
                if(header == NULL)
                {
                    printf("[%s] exiting with timeout\n", argv[0]);
                    return -1;
                }
            }

            // save in kermit and get control sum
            memcpy(&pck, header->payload, header->len);
            data_len= calculate_data_len(pck);
            len_sum = calculate_check_len(pck, data_len);
            crc = crc16_ccitt(header->payload, len_sum);

            // a correct package was received
            if(crc == pck.check && pck.SEQ == (current_seq + 1))
            {
                // send ACK
                pckR = send_ACK(&current_seq, pck.SEQ);
                pckR.type = 'Y';
            }
            else //data modified
            {
                // send NAK
                pckR = send_NAK(&current_seq, pck.SEQ);
            }
        }

    }      

    // keep waiting for packages until EOT is sent
    while(1)
    {
        // wait for message
        r = receive_message_timeout(5000); 
        if (r == NULL) // no response in 5 seconds
        {
            r = resend_timeout(t); // resend last message max. 3 times
            if (r == NULL) //interrupt connection if timeout
            {
                printf("[%s] exiting with timeout\n", argv[0]);
                return -1;
            }
        }
        
        // copy message in kermit
        memset(&pck, 0, sizeof(pck));
        memcpy(&pck, &(r->payload), r->len);


        // check if received package has the corresponding SEQ
        // if not, wait for the next file
        if(pck.SEQ % 64 != (current_seq + 1) % 64)
            continue;

        //calculate control sum
        data_len = calculate_data_len(pck);
        len_sum = calculate_check_len(pck, data_len);
        crc = crc16_ccitt(&pck, len_sum);

        if(crc == pck.check) //correct information
        {
            pckR = send_ACK(&current_seq, pck.SEQ); // send ACK
        } 
        else // information is modified
        {
            // sending NAK
            pckR = send_NAK(&current_seq, pck.SEQ);

            // check response
            memcpy(&(t.payload), &pckR, sizeof(kermit));
            t.len = sizeof(pckR);

            //keep resending NAK until all data is correct
            while(pckR.type == 'N')
            {
                // receiving message from sender
                header = receive_message_timeout(5000);
                if(header == NULL) // no responde after 5 seconds
                {
                    //resend last message max. 3 times
                    header = resend_timeout(t); 
                    if(header == NULL) //interrupt connection if timeout
                    {
                        printf("[%s] exiting with timeout\n", argv[0]);
                        return -1;
                    }
                }


                // save in kermit and get control sum
                memcpy(&pck, header->payload, header->len);
                data_len = calculate_data_len(pck);
                len_sum = calculate_check_len(pck, data_len);
                crc = crc16_ccitt(header->payload, len_sum);

                // a correct package was received
                if(crc == pck.check)
                {
                    // send ACK
                    pckR = send_ACK(&current_seq, pck.SEQ);
                    pckR.type = 'Y';
                }
                else //data modified
                {
                    // send NAK
                    pckR = send_NAK(&current_seq, pck.SEQ);
                }
            }
        }

        switch(pck.type){
            case 'F': // package of type F : open file
                strcpy(name_buffer, "recv_");
                strcat(name_buffer, pck.data);
                fd = open(name_buffer, O_WRONLY | O_CREAT, 0777);
                break;
            case 'D': // package of type D: write data in file
                data_len = calculate_data_len(pck);
                write(fd, pck.data, data_len);
                break;
            case 'Z': // package of type Z: close file
                close(fd);
                break;
            case 'B': // package of type B : end the connection
                return 0;
            }
    }

	return 0;
}
