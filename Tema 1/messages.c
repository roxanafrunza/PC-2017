#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "messages.h"
#include "lib.h"

#define NO_SET 0x00
#define CR 0x0D

// calcute the number of bytes in pkg.data
int calculate_data_len(kermit pkg)
{
	int result = pkg.len;
	result -= (sizeof(pkg.SEQ) + sizeof(pkg.type)
			+ sizeof(pkg.check) + sizeof(pkg.mark));

	return result;
}

// calculate len value for a kermit with given data field length
int calculate_len(kermit pkg, int data_len)
{
	int result;
	result = sizeof(pkg.SEQ) + sizeof(pkg.type) + data_len
				+ sizeof(pkg.check) + sizeof(pkg.mark);
	return result;
}

// calculate len for check sum function
int calculate_check_len(kermit pkg, int data_len)
{
	int result;
	result = sizeof(pkg.SOH) + sizeof(pkg.len) + sizeof(pkg.SEQ)
			+ sizeof(pkg.type) + data_len;
	return result;
}
// calculate control sum for given kermit
kermit set_crc(kermit pck)
{
	int data_len;
    int len_sum;

    data_len = calculate_data_len(pck); // calculate data's length
    // calculate number of bytes considered in control sum
    len_sum = calculate_check_len(pck, data_len); 
   	pck.check = crc16_ccitt(&pck, len_sum); // set control sum

	return pck; // return modified package
}
// initializing the data for package of type S
data_S initS_data()
{
	data_S result;
	memset(&result, 0, sizeof(data_S));
	result.MAXL = 250;
	result.TIME = 5;
	result.NPAD = NO_SET;
	result.PDAC = NO_SET;
	result.EOL = CR;
	result.QCTL = NO_SET;
	result.QBIN = NO_SET;
	result.CHKT = NO_SET;
	result.REPT = NO_SET;
	result.CAPA = NO_SET;
	result.R = NO_SET;

	return result;
}

// initializing a package of type S
kermit initS()
{
	kermit result;
	data_S data = initS_data();

	memset(&result, 0, sizeof(kermit));
	result.SOH = 0x01; // SOH field is always 0x01
	result.SEQ = NO_SET; //SEQ is initially 0x00
	result.type = 'S'; // package type is S
	memcpy(&result.data, &data, sizeof(data)); // header defined in data_S
	result.len = calculate_len(result, sizeof(data)); // package_length - 2

	result.mark = CR; // implicitly is 0x0D;

	return result;
}

// initializing a package of type F
kermit initF(char* file_name, int count)
{
	kermit result;

	memset(&result, 0, sizeof(kermit));
	result.SOH = 0x01; // SOH field is always 0x01
	result.SEQ = NO_SET; //SEQ is initially 0x00
	result.type = 'F'; // package type is F
	memcpy(&result.data, file_name, count); // file name
	result.len = calculate_len(result,count); // package_length - 2
	result.mark = CR; // implicitly is 0x0D;

	return result;
}

// initializing a package of type D
kermit initD(char data[], int count)
{
	kermit result;

	memset(&result, 0, sizeof(kermit));
	result.SOH = 0x01; // SOH field is always 0x01
	result.SEQ = NO_SET; //SEQ is initially 0x00
	result.type = 'D'; // package type is D
	result.len = calculate_len(result, count);
	memcpy(&result.data, data, count);
	result.mark = CR;

	return result;
}

// initializing a package of type Z
kermit initZ()
{
	kermit result;

	memset(&result, 0, sizeof(kermit));
	result.SOH = 0x01; // SOH field is always 0x01
	result.SEQ = NO_SET; //SEQ is initially 0x00
	result.type = 'Z'; // package type is Z
	result.len = calculate_len(result, 0);
	result.mark = CR;

	return result;
}

// intializing a package of type B
kermit initB()
{
	kermit result;

	memset(&result, 0, sizeof(kermit));
	result.SOH = 0x01; // SOH field is always 0x01
	result.SEQ = NO_SET; //SEQ is initially 0x00
	result.type = 'B'; // package type is B
	result.len = calculate_len(result, 0);
	result.mark = CR;
	
	return result;
}

// initializing a package of type Y
kermit initY()
{
	kermit result;

	memset(&result, 0, sizeof(kermit));
	result.SOH = 0x01; // SOH field is always 0x01
	result.SEQ = NO_SET; //SEQ is initially 0x00
	result.type = 'Y'; // package type is B
	result.len = calculate_len(result, 0);

	result.mark = CR;
	
	return result;
}
// initializing a package of type Y when confirming a S package
kermit initYS()
{
	kermit result;
	data_S data = initS_data();

	memset(&result, 0, sizeof(kermit));
	result.SOH = 0x01; // SOH field is always 0x01
	result.SEQ = NO_SET; //SEQ is initially 0x00
	result.type = 'Y'; // package type is S
	memcpy(&result.data, &data, sizeof(data)); // header defined in data_S
	result.len = calculate_len(result, sizeof(data)); // package_length - 2
	result.mark = CR; // implicitly is 0x0D;

	return result;
}
// initializing a package of type N
kermit initN()
{
	kermit result;

	memset(&result, 0, sizeof(kermit));
	result.SOH = 0x01; // SOH field is always 0x01
	result.SEQ = NO_SET; //SEQ is initially 0x00
	result.type = 'N'; // package type is B
	result.len = calculate_len(result, 0);
	result.mark = CR; // implicitly is 0x0D

	return result;
}
msg* resend_timeout(msg message)
{
	int retry = 0;
	msg *r = NULL;

	while(retry < 3)
	{
		// resending the message
        send_message(&message);
        r = receive_message_timeout(5000);
        if(r != NULL)
            break;
        retry++;
	}

	if(retry == 3)
		return NULL;

	return r;
}