#define BUFLEN 256

typedef struct {
	char last_name[12];
	char first_name[12];
	char card_number[7]; 
	char pin[5]; 
	char password[16];
	double sold;
	int is_blocked;
	int login_failed;
} TClient;

typedef struct {
	char card_number[6];
	double current_sold;
	int is_blocked; 
	int socket;
} TSession;
