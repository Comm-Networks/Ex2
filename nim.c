
#include "nim_protocol.h"

#define INPUT_SIZE 2



int main(int argc , char** argv){
	char * hostname = DEFAULT_HOST;
	char* port = DEFAULT_PORT;
	struct addrinfo  hints;
	struct addrinfo * dest_addr , *rp;
	int sockfd;
	int size;
	char pile;
	short number;

	short end_game = 0;


	client_msg msg_queue[MSG_NUM];
	short queue_head = 0;
	short queue_sent = -1;

	client_msg current_msg;
	int current_msg_offset = -1;

	msg current_s_msg;
	int current_s_msg_offset = 0;

	if (argc==3) {
		hostname=argv[1];
		port=argv[2];
	}
	else if (argc==2) {
		hostname=argv[1];
	}

	short client_num;
	short my_turn;
	short msg_fully_recieved;

	struct timeval zero_time;
	zero_time.tv_sec = 0;
	zero_time.tv_usec = 0;

	// For select.
	fd_set stdin_set;
	FD_ZERO(&stdin_set);
	FD_SET(fileno(stdin), &stdin_set);
	fd_set read_set,write_set;
	FD_ZERO(&read_set);
	FD_ZERO(&write_set);


	// Obtain address(es) matching host/port
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	int status =getaddrinfo(hostname,port,&hints,&dest_addr);
	if (status!=0){
		printf("getaddrinfo error: %s\n", strerror(status));
		return EXIT_FAILURE;
	}

	// loop through all the results and connect to the first we can
	for (rp=dest_addr ; rp!=NULL ; rp=rp->ai_next){
		if (rp->ai_family!=AF_INET) {
			continue;
		}
		//opening a new socket
		sockfd = socket(hints.ai_family,hints.ai_socktype,hints.ai_protocol);
		if (sockfd==-1) {
			continue;
		}
		if (connect(sockfd,rp->ai_addr,rp->ai_addrlen)!=-1){
			break ; //successfuly connected
		}
		close(sockfd);
	}
	// No address succeeded
	if (rp == NULL) {
		printf("Client:failed to connect: %s.\n",strerror(errno));
		close(sockfd);
		freeaddrinfo(dest_addr);
		return EXIT_FAILURE;
	}
	freeaddrinfo(dest_addr);

	// Adding socket to read and write sets.
	FD_SET(sockfd, &read_set);
	//FD_SET(sockfd, &write_set);

	int ret_val=0;
	short game_started = 0;

	// Getting init msg.
	size = sizeof(current_s_msg);

	//first of all get all init message.
	while (1){

		ret_val=select(sockfd + 1, &read_set, NULL, NULL, NULL);

		if (ret_val==-1){
			printf("failed using select function: %s\n",strerror(errno));
			close(sockfd);
			return EXIT_FAILURE;
		}
		if (FD_ISSET(sockfd,&read_set)){
			if (DEBUG){
				printf("ready to recieve\n");
			}
			ret_val = recv(sockfd, &current_s_msg + current_s_msg_offset, size, 0);
			if (ret_val==-1){
				printf("Client:failed to read from server: %s\n",strerror(errno));
				return EXIT_FAILURE;
			}
			else if (ret_val<size) {
				// Message received partially.
				if (DEBUG){
					printf("recieved only part of msg\n");
				}
				size -= ret_val;
				current_s_msg_offset += ret_val;

			}
			else {
				if (DEBUG){
					printf("all init msg recieved\n");
				}
				current_s_msg_offset = 0; // Resetting for next use.
				break;
			}
		}

	}

	if (current_s_msg.type == REJECTED_MSG) {
		// Server notified the client that it was rejected.
		printf("Client rejected\n");
		close(sockfd);
		return EXIT_SUCCESS;

	}
	else if (current_s_msg.type == INIT_MSG) {
		// Fetch client num and display messages.
		client_num = current_s_msg.data.init_msg.client_num;
		my_turn = !(client_num == 1); //client number 1 is represented by 0 here
		printf("You are client %hd\n", (short)(client_num+1));
		if (client_num == 0) {
			printf("Waiting to client 2 to connect\n");
		}
	}

	while (1) {

		FD_ZERO(&read_set);
		FD_ZERO(&write_set);

		//add client socket to read fds set
		FD_SET(sockfd,&read_set);
		FD_SET(sockfd,&write_set);

		// Checking if server is ready to send.
		ret_val=select(sockfd + 1, &read_set, NULL, NULL, &zero_time);
		if (ret_val == -1) {
			printf("failed using select function: %s\n",strerror(errno));
			end_game=1;
			continue;

		}

		else if (FD_ISSET(sockfd,&read_set)) {

			// Recv data from server.
			size = sizeof(msg) - current_s_msg_offset;
			ret_val = recv(sockfd, &current_s_msg + current_s_msg_offset, size, 0);
			if (ret_val == -1) {
				printf("Error recieving data from server: %s\n",strerror(errno));
				break;
			}
			else if (ret_val < size) {
				// Message partially received. Updating offset.
				current_s_msg_offset += ret_val;
				msg_fully_recieved=0;
			}
			else {
				// Message fully received. Resetting offset and processing.
				current_s_msg_offset = 0;
				msg_fully_recieved=1;
				data_union* data = &current_s_msg.data;

				switch (current_s_msg.type) {
				case SERVER_MSG:
					if (data->s_msg.winner != NO_WIN) {
						char * result = data->s_msg.winner == CLIENT_WIN ? "win" : "lose" ;
						printf("You %s!\n", result);
						end_game=1;
					}
					else {
						// Game continues.
						if (data->s_msg.player_turn == client_num) {
							// Other player made a move.Give this client  the right message. Now it is his turn.
							if (data->s_msg.legal == LEGAL_MOVE) {
								printf("Client %hd removed %hd cubes from heap %c\n", data->s_msg.player_turn,\
										data->s_msg.cubes_removed, data->s_msg.heap_name);
								printf("Heap A: %d\nHeap B: %d\nHeap C: %d\n",\
										data->s_msg.n_a, data->s_msg.n_b, data->s_msg.n_c);
							}
							else if (data->s_msg.legal == ILLEGAL_MOVE) {
								printf("Client %hd made an illegal move\n", data->s_msg.player_turn);
							}
							else {
								printf("Heap A: %d\nHeap B: %d\nHeap C: %d\n",\
										data->s_msg.n_a, data->s_msg.n_b, data->s_msg.n_c);
							}
							my_turn=1;
							printf("Your turn:\n");

						} else if (!game_started) {
							// Just show initial piles.
							printf("Heap A: %d\nHeap B: %d\nHeap C: %d\n",\
									data->s_msg.n_a, data->s_msg.n_b, data->s_msg.n_c);
						}
					}
					game_started = 1;

					break;

				case AM_MSG:
					if(1){
						char * am_print = data->am_msg.legal == ILLEGAL_MOVE ? "Illegal move" : "Move accepted";
						printf("%s\n",am_print);

						if (data->am_msg.legal == LEGAL_MOVE) {
							// Print updated piles list after legal move.
							printf("Heap A: %d\nHeap B: %d\nHeap C: %d\n", data->am_msg.n_a, data->am_msg.n_b, data->am_msg.n_c);
						}
						my_turn=0;
					}

					break;

				case CHAT_MSG:
					printf("Client %hd: %s\n", data->chat.sender_num, data->chat.msg);
					break;

				default:
					break;
				}
				if (end_game==-1){
					break;
				}

			}

		}

		if (!game_started) {
			// Can't read input from user if game not started yet.
			continue;
		}

		//we can recieve input from stdin all the time , even if it is not the player's turn
		ret_val=select(fileno(stdin),&read_set,NULL,NULL, &zero_time);
		if (ret_val == -1) {
			printf("failed using select function: %s\n",strerror(errno));
			break;
		}

		//checks if it is ok to read from stdin
		if (FD_ISSET(fileno(stdin),&read_set)){

			scanf(" %c", &pile); // Space before %c is to consume the newline char from the previous scanf.
			if (pile == QUIT_CHAR) {
				if (my_turn){
					msg_queue[queue_head].type = CLIENT_MOVE_MSG;
					msg_queue[queue_head].data.client_move.heap_name = pile;
					msg_queue[queue_head].data.client_move.num_cubes_to_remove = 0;
					queue_head++;
				}
				else {
					printf("You can quit only on your turn.\n");
				}
				/*TOMER: No need to shutdown. We have select. Server will not be able to send us message and close
				         the socket after 1 minute.*/
			}
			else if (pile == MSG_CHAR) {
				// Client wants to send a message. Getting the message.
				scanf("SG");
				fgets(msg_queue[queue_head].data.chat.msg, MSG_MAX_SIZE, stdin);
				msg_queue[queue_head].type = CHAT_MSG;
				msg_queue[queue_head].data.chat.sender_num = client_num;
				queue_head++;

			}
			else {
				// Client wants to make his move. Getting the pile number.
				scanf(" %hd", &number);
				if (my_turn && msg_fully_recieved){
					//sending the move to the server
					msg_queue[queue_head].type = CLIENT_MOVE_MSG;
					msg_queue[queue_head].data.client_move.heap_name = pile;
					msg_queue[queue_head].data.client_move.num_cubes_to_remove = number;
					queue_head++;
				}
				else {
					printf("Wait for your turn to submit a move.\n");
				}
			}

			if (queue_sent < queue_head) {
				// Still have msgs to send.
				if (current_msg_offset == -1) {
					// No partially sent message - getting next one from the queue.
					current_msg = msg_queue[queue_sent + 1];
					current_msg_offset = 0;
				}

				// Trying to send.
				size = sizeof(current_msg) - current_msg_offset;

				// Checking if server is ready to recv.
				ret_val=select(sockfd,NULL,&write_set,NULL,NULL);
				if (ret_val == -1) {
					printf("failed using select function: %s\n",strerror(errno));
					break;
				}
				if (FD_ISSET(sockfd,&write_set)) {
					// Sending data to server.
					ret_val = send(sockfd, &current_msg + current_msg_offset, size, 0);
					if (ret_val == -1) {
						printf("Error sending message to server: %s\n",strerror(errno));
						break;
					}
					else if (ret_val < size) {
						// Message partially sent. Updating offset of current message.
						current_msg_offset += ret_val;
					}
					else {
						// Message fully sent. Updating queue.
						current_msg_offset = -1;
						queue_sent++;
					}
				}

			}
		}
	}

	close(sockfd);
	return (ret_val == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
