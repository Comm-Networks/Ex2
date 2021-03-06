#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/select.h>


#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT "6444"
#define PILE_A_CHAR 'A'
#define PILE_B_CHAR 'B'
#define PILE_C_CHAR 'C'
#define QUIT_CHAR 'Q'
#define MSG_CHAR 'M'
#define CLIENT_LOST 'l'
#define CLIENT_WIN 'w'
#define NO_WIN 'n'
#define LEGAL_MOVE 'g'
#define ILLEGAL_MOVE 'b'
#define INIT_CHAR 'i'
#define NUM_CLIENTS 2
#define MSG_NUM 50
#define MSG_MAX_SIZE 255





#pragma pack(push,1)
	typedef enum {
		INIT_MSG,
		SERVER_MSG,
		CHAT_MSG,
		CLIENT_MOVE_MSG,
		AM_MSG,
		REJECTED_MSG
	}msg_type;
#pragma pack(pop)


#pragma pack(push,1)
	typedef struct{
		short client_num; //informs the client with his number.
	}init_server_msg;
#pragma pack(pop)

//message from the server - heaps' size info.
#pragma pack(push, 1)
	typedef struct {
		char  winner; /* w-this client won/ l- this client lost/ n- no winner yet */
		short n_a; /* heap A size */
		short n_b; /* heap B size */
		short n_c; /* heap C size */
		char legal ; // g - good b- illegal i-first message after init message
		short player_turn; /* turn of player num 1 or 2*/
		short cubes_removed; /* num of cubes removed by opponent*/
		char  heap_name; /*name of heap , the opponent removed cubes from*/
	}server_msg;
#pragma pack(pop)

#pragma pack(push,1)
	typedef struct {
		short sender_num;
		char  msg[MSG_MAX_SIZE]; /* sending the msg from one client to another*/
	} chat_msg;
#pragma pack(pop)

#pragma pack(push, 1)
	typedef struct  {
		short num_cubes_to_remove; /* 0 if we want only to send message */
		char  heap_name; /* the heap we remove cubes from*/
	} client_move_msg;
#pragma pack(pop)

#pragma pack(push,1)
	typedef union {
		chat_msg chat;
		client_move_msg client_move;
	} client_data_union;
	typedef struct {
		msg_type type;
		client_data_union data;
	} client_msg;
#pragma pack(pop)

#pragma pack(push, 1)
	typedef struct  {
		char legal ; // g - good b- illegal
		short n_a;
		short n_b;
		short n_c;
	}after_move_msg;
#pragma pack(pop)


#pragma pack(push,1)
	typedef union {
		init_server_msg init_msg;
		server_msg s_msg;
		chat_msg chat;
		after_move_msg am_msg;
	} data_union;
	typedef struct {
		msg_type type;
		data_union data;
	} msg;
#pragma pack(pop)

