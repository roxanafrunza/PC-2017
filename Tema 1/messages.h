#include "lib.h"

typedef struct {
	unsigned char SOH;
	unsigned char len;
	unsigned char SEQ;
	unsigned char type;
	char data[250];
	unsigned short check;
	unsigned char mark;
} kermit;

typedef struct {
	char MAXL;
	char TIME;
	char NPAD;
	char PDAC;
	char EOL;
	char QCTL;
	char QBIN;
	char CHKT;
	char REPT;
	char CAPA;
	char R;
} data_S;

data_S initS_data();
kermit initS();
kermit initF(char* filen_name, int count);
kermit initD(char data[], int count);
kermit initZ();
kermit initB();
kermit initY();
kermit initN();
kermit initYS();
int calculate_len(kermit pkg, int data_len);
int calculate_check_len(kermit pkg, int data_len);
int calculate_data_len(kermit pkg);
kermit set_crc(kermit pkg);
msg* resend_timeout(msg message);