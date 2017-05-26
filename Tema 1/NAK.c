current_seq = pckR.SEQ % 64;
                current_seq++;
                pck.SEQ = current_seq;
                pck = set_crc(pck);

                memcpy(&(header.payload), &pck, sizeof(kermit));
                header.len = sizeof(kermit);
                printf("[sende] trimit mesaj\n");
                send_message(&header);
                printf("[sende] am trimis mesaj si astept r\n");
                r = receive_message_timeout(15000);
                if( r == NULL)
                {
                    printf("[sender] timeout\n");
                    r = resend_timeout(header);
                    if(r == NULL)
                        return -1;
                }

                memcpy(&pckR, r->payload, sizeof(kermit));

                while(pckR.type == 'N')
                {
                    current_seq = pckR.SEQ % 64;
                    current_seq++;
                    pck.SEQ = current_seq;
                    pck = set_crc(pck);

                    memcpy(&(header.payload), &pck, sizeof(kermit));
                    header.len = sizeof(kermit);
                    printf("[sender] retrimit\n");
                    send_message(&header);
                    printf("[sender] am retrimis, astept\n");
                    r = receive_message_timeout(15000);
                    printf("[sende] primit mesaj\n");
                    if( r == NULL)
                    {
                         printf("[sende] timeout mesaj\n");
                        r = resend_timeout(header);
                        if(r == NULL)
                            return -1;
                    }
                    printf("[sender] am primit corect\n");
                    memcpy(&pckR, r->payload, sizeof(kermit));
                }