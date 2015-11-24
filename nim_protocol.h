#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT "6444"
#define PILE_A_CHAR 'A'
#define PILE_B_CHAR 'B'
#define PILE_C_CHAR 'C'
#define QUIT_CHAR 'Q'
#define CLIENT_LOST 'l'
#define CLIENT_WIN 'w'
#define NO_WIN 'n'
#define LEGAL_MOVE 'g'
#define ILLEGAL_MOVE 'b'
#define NUM_CLIENTS 2
#define MSG_NUM 100

#pragma pack(push,1)
	typedef struct{
		short client_num; //informs the client with his number.
	}init_server_msg;
#pragma pack(pop)

//message from the server - start of a turn
#pragma pack(push, 1)
	typedef struct {
		short n_a; /* heap A size */
		short n_b; /* heap B size */
		short n_c; /* heap C size */
		char  winner; /* w-this client won/ l- this client lost/ n- no winner yet */
		short player_turn; /* turn of player num 1 or 2*/
		char  msg[1024]; /* sending the msg from one client to another*/
	}server_msg;
#pragma pack(pop)

#pragma pack(push, 1)
	typedef struct  {
		short num_cubes_to_remove; /* 0 if we want only to send message */
		char  heap_name; /* the heap we remove cubes from*/
		char msg[1024]; /* potential chat message from one client to another*/
	}client_msg;
#pragma pack(pop)

#pragma pack(push, 1)
	typedef struct  {
		char legal ; // g - good b- illegal
	}after_move_msg;
#pragma pack(pop)

