#include "nim_protocol.h"


int msg_fully_sent[NUM_CLIENTS];
int cmsg_fully_recieved[NUM_CLIENTS];


//check if all piles are empty
int empty_piles(int n_a,int n_b,int n_c) {
	return (n_a == 0) && (n_b == 0) && (n_c == 0);
}

/*
 * recieving a client message. return -1 on error, 0 on success.
 */
int recieveClientMessage(int player_turn,int sock,int* off,client_msg c_msg_recieved[NUM_CLIENTS]){
	int size;
	int ret_val;
	if (*off==0){

		memset(&(c_msg_recieved[player_turn]),0,sizeof(client_msg));
	}

	size =sizeof(client_msg)-*off;
	ret_val = recv(sock,&(c_msg_recieved[player_turn])+*off,size,0);

	if (ret_val ==-1){
		return -1;
	}
	else{
		if (ret_val<size){
			cmsg_fully_recieved[player_turn]=0;
			*off+=ret_val;
		}
		else {

			cmsg_fully_recieved[player_turn]=1;
			*off=0;
		}
	}
	return 0;
}


int main(int argc , char** argv) {
	const char hostname[] = DEFAULT_HOST;
	char* port = DEFAULT_PORT;

	int listening_sock;
	unsigned int size;
	int new_sock,sock;
	int max_fd;
	int client_sockets[NUM_CLIENTS+1]; //needs another socket for rejected client
	double timer[NUM_CLIENTS];
	clock_t clk;

	fd_set read_fds;
	fd_set write_fds;

	client_msg c_msg;
	server_msg s_msg[NUM_CLIENTS];
	after_move_msg am_msg;
	init_server_msg init_s_msg;
	struct addrinfo  hints;
	struct addrinfo * my_addr , *rp;
	struct sockaddr_in client_adrr;

	int i=0;
	int offset[NUM_CLIENTS];
	int c_offset[NUM_CLIENTS];
	memset(client_sockets,0,sizeof(client_sockets));

	if (argc < 4) {
		// Error.
		return EXIT_FAILURE;
	}

	// Initializing piles.
	int n_a =(int)strtol(argv[1], NULL, 10);
	int n_b = (int)strtol(argv[2], NULL, 10);
	int n_c = (int)strtol(argv[3], NULL, 10);

	s_msg[0].n_a = n_a;
	s_msg[0].n_b = n_b;
	s_msg[0].n_c = n_c;
	s_msg[0].winner = NO_WIN;
	s_msg[0].player_turn=0;
	s_msg[0].legal=INIT_CHAR;

	s_msg[1].n_a = n_a;
	s_msg[1].n_b = n_b;
	s_msg[1].n_c = n_c;
	s_msg[1].winner = NO_WIN;
	s_msg[1].player_turn=0;
	s_msg[1].legal=INIT_CHAR;


	if (argc > 4) {
		port = argv[4];
	}

	// Obtain address(es) matching host/port
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	int status = getaddrinfo(hostname,port,&hints,&my_addr);
	if (status!=0){
		printf("getaddrinfo error: %s\n", strerror(status));
		return EXIT_FAILURE;
	}


	// loop through all the results and connect to the first we can
	for (rp=my_addr ; rp!=NULL ; rp=rp->ai_next){
		if (rp->ai_family!=PF_INET) {
			continue;
		}
		//opening a new socket
		listening_sock = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
		if (listening_sock==-1) {
			continue;
		}
		if (bind(listening_sock,rp->ai_addr,rp->ai_addrlen)!=-1){
			break ; //successfuly binded
		}
		close(listening_sock);
	}

	// No address succeeded
	if (rp == NULL) {
		fprintf(stderr, "Server:failed to bind\n");
		close(listening_sock);
		freeaddrinfo(my_addr);
		return EXIT_FAILURE;
	}
	freeaddrinfo(my_addr);

	if (listen(listening_sock, 5) < 0) {
		printf("problem while listening for an incoming call : %s\n",strerror(errno));
		close(listening_sock);
		return EXIT_FAILURE;
	}

	int ret_val=0;
	int player_turn =0; // player 0 or 1
	int clients_connected=0;
	msg msgs_to_send[NUM_CLIENTS][MSG_NUM];
	int queue_sent[NUM_CLIENTS];
	int queue_head[NUM_CLIENTS];
	msg current_msg[NUM_CLIENTS];
	short init_msg_sent[NUM_CLIENTS];
	client_msg c_msg_recieved[NUM_CLIENTS];
	int start_game=0;
	struct timeval time;
	short first_smsg_recv[NUM_CLIENTS];
	short cmsg_recv[NUM_CLIENTS]; //if there is a message sent from client(does not have to be a full msg)

	//initilizing arrays to all 0
	cmsg_fully_recieved[0] = 1;
	cmsg_fully_recieved[1] = 1;

	queue_sent[0] = -1;
	queue_sent[1] = -1;

	offset[0] = -1;
	offset[1] = -1;

	memset(init_msg_sent,0,NUM_CLIENTS);
	memset(first_smsg_recv,0,NUM_CLIENTS);
	memset(cmsg_recv,0,NUM_CLIENTS);
	memset(queue_head,0,NUM_CLIENTS);
	memset(msg_fully_sent,0,NUM_CLIENTS);
	memset(c_offset,0,NUM_CLIENTS);

	timer[0] = (double) clock() / CLOCKS_PER_SEC;
	// Main loop.
	while (1) {


		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);

		//add listening socket to read fds set
		FD_SET(listening_sock,&read_fds);
		max_fd=listening_sock;

		for (i=0;i<NUM_CLIENTS;i++){
			int fd =client_sockets[i];

			//if valid socket descriptor, add to read and write sets.
			if (fd>0){
				FD_SET(fd,&read_fds);
				FD_SET(fd,&write_fds);
			}
			if (fd>max_fd){
				max_fd=fd;
			}
		}

		time.tv_sec=4;
		time.tv_usec=0;
		//return read-ready sockets
		ret_val= select(max_fd+1,&read_fds,NULL,NULL,&time);

		if (ret_val<0){
			printf("Server:failed using select function: %s\n",strerror(errno));
			break;
		}
		//listening socket is read-ready - can accept
		if (FD_ISSET(listening_sock,&read_fds)){

			size = sizeof(struct sockaddr_in);
			new_sock = accept(listening_sock, (struct sockaddr*)&client_adrr, &size);
			if (new_sock<0){
				printf("problem while trying to accept incoming call : %s\n",strerror(errno));
				continue;
			}
			init_s_msg.client_num= (short) clients_connected++;
			client_sockets[init_s_msg.client_num]=new_sock;
			FD_SET(new_sock,&write_fds);
			max_fd = (new_sock>max_fd)? new_sock : max_fd;

			//setting the message we want to send;
			if (clients_connected<=2 && !init_msg_sent[init_s_msg.client_num]){

				msgs_to_send[init_s_msg.client_num][0].type=INIT_MSG;
				msgs_to_send[init_s_msg.client_num][0].data.init_msg=init_s_msg;
				queue_head[init_s_msg.client_num]++; // Added message - moving queue head.
			}

			//try letting the 3rd client know he is rejected.


			if (clients_connected>2){
				ret_val= select(max_fd+1,&read_fds,NULL,NULL,&time);
				if (ret_val<0){
					break;
				}
				if (FD_ISSET(new_sock,&write_fds)){
					msg temp_msg;
					temp_msg.type=REJECTED_MSG;
					memset(&temp_msg.data,0,sizeof(data_union));
					size = sizeof(msg);
					if (send(new_sock,&temp_msg,size,0)==-1){
						printf("Error sending message to client #%d: %s\n",clients_connected,strerror(errno));
					}
					close(new_sock);
					client_sockets[clients_connected--]=-1;
				}
			}


		}

		//its an IO operation. all sockets are connected.
		else  if (clients_connected>0){


			int opp_player = player_turn == 0 ? 1 : 0 ;
			timer[player_turn] = -1*timer[player_turn] +(double) clock()/ CLOCKS_PER_SEC ;

			//player disconnected
			if (timer[player_turn]>=60){

				close(client_sockets[player_turn]);
				client_sockets[player_turn]=-1;

				i = queue_head[opp_player];
				queue_head[opp_player]++;
				queue_head[opp_player] %= MSG_NUM;
				msg temp_msg;
				//send a message to the client and then a server msg , letting him know he won.
				temp_msg.type=CHAT_MSG;
				char* chat_msg;
				chat_msg = (player_turn==0)? "client 1 disconnected from the server": "client 2 disconnected from the server";
				memcpy(&temp_msg.data.chat.msg,chat_msg,MSG_MAX_SIZE);
				msgs_to_send[opp_player][i]=temp_msg;

				memset(&temp_msg,0,sizeof(msg)); //initilizing the msg for re-use.
				temp_msg.type=SERVER_MSG;
				temp_msg.data.s_msg.winner=CLIENT_WIN;
				temp_msg.data.s_msg.player_turn=opp_player;
				msgs_to_send[opp_player][queue_head[opp_player]]=temp_msg;
				queue_head[opp_player]++;
				queue_head[opp_player] %= MSG_NUM;

				player_turn = player_turn==0 ? 1: 0;
				continue;
			}



			if ((cmsg_fully_recieved[player_turn]) && (queue_sent[player_turn] < queue_head[player_turn]-1)) {

				// There is no partially received message, and there are messages to send in the queue.

				if (offset[player_turn] == -1) {

					memset(&current_msg[player_turn],0,sizeof(current_msg[player_turn]));
					// No partially sent message. Getting next msg from queue and initializing offset.
					memcpy(&current_msg[player_turn] , &(msgs_to_send[player_turn][queue_sent[player_turn] + 1]),sizeof(msg));
					offset[player_turn] = 0;
				}


				sock=client_sockets[player_turn];

				ret_val=select(max_fd+1,NULL,&write_fds,NULL,NULL);
				if (ret_val< 0) {
					printf("Server:failed using select function: %s\n",strerror(errno));
					break;
				}


				//trying to send the server msg or any previous server msg to client.
				if (FD_ISSET(sock,&write_fds)){


					if (start_game || (current_msg[player_turn].type == INIT_MSG && !init_msg_sent[player_turn]) ) {



						size=sizeof(current_msg[player_turn])-offset[player_turn]; //decrease the num of bytes already sent.

						//first byte sent is the type and it will be sent for sure(at least one byte is sent).
						ret_val = send(sock, &(current_msg[player_turn])+offset[player_turn], size,0);
						if (ret_val ==-1){
							printf("Error sending message to client #%d: %s\n",player_turn+1,strerror(errno));
							break;
						}
						//part of msg was sent. update the offset to start next send from bytes that haven't been sent yet.
						else if (ret_val<size) {

							offset[player_turn] += ret_val;
							msg_fully_sent[player_turn]=0;
						}
						//finished sending the message
						else {

							offset[player_turn] = -1;
							msg_fully_sent[player_turn]=1;
							queue_sent[player_turn]++;
							queue_sent[player_turn] %= MSG_NUM;
							switch (current_msg[player_turn].type){
							case (AM_MSG):
									player_turn= player_turn==0 ? 1 : 0;
							continue;

							case (INIT_MSG):

							init_msg_sent[player_turn]=1;

							// Adding initial server message.
							msgs_to_send[player_turn][queue_head[player_turn]].type=SERVER_MSG;
							msgs_to_send[player_turn][queue_head[player_turn]].data.s_msg=s_msg[player_turn];
							queue_head[player_turn]++;
							queue_head[player_turn] %= MSG_NUM;
							break;

							case(SERVER_MSG):

							if (!first_smsg_recv[player_turn]){
								//first server msg was sent
								first_smsg_recv[player_turn]=1;


							}
							if (current_msg[player_turn].data.s_msg.winner != NO_WIN){
								close(client_sockets[player_turn]);
								clients_connected--;
								client_sockets[player_turn]=-1;
								player_turn= (player_turn==0)? 1: 0;
								continue;
							}
							break;

							default:
								break;
							}
						}

					}
				}
			}

			//only opp player's socket can be -1. if we close the player's turn socket, we change the player.
			if (client_sockets[opp_player]==-1){
				continue;
			}

			//send message to opp client.There is no partially received message, and there are messages to send in the queue.
			if ((cmsg_fully_recieved[opp_player]) && (queue_sent[opp_player] < queue_head[opp_player]-1)) {

				if (offset[opp_player] == -1) {
					// No partially sent message. Getting next msg from queue and initializing offset.
					memset(&current_msg[opp_player],0,sizeof(current_msg[opp_player]));
					memcpy(&current_msg[opp_player],&(msgs_to_send[opp_player][queue_sent[opp_player] + 1]),sizeof(msg));
					offset[opp_player] = 0;
				}

				i= queue_head[opp_player];
				if (current_msg[opp_player].type==CHAT_MSG || (current_msg[opp_player].type==INIT_MSG && !init_msg_sent[opp_player])
						|| (current_msg[opp_player].type==SERVER_MSG && !first_smsg_recv[opp_player])){

					sock=client_sockets[opp_player];

					ret_val=select(max_fd+1,NULL,&write_fds,NULL,NULL);
					if (ret_val< 0) {
						printf("Server:failed using select function: %s\n",strerror(errno));
						break;
					}
					//trying to send the server msg or any previous server msg to client.
					if (FD_ISSET(sock,&write_fds)){


						size=sizeof(current_msg[opp_player])-offset[opp_player]; //decrease the num of bytes already sent.
						//first byte sent is the type and it will be sent for sure(at least one byte is sent).
						ret_val = send(sock, &(current_msg[opp_player])+offset[opp_player], size,0);



						if (ret_val ==-1){
							printf("Error sending message to client #%d: %s\n",player_turn-1,strerror(errno));
							break;
						}
						//part of msg was sent. update the offset to start next send from bytes that haven't been sent yet.
						else if (ret_val<size) {
							offset[opp_player] +=ret_val ;
							msg_fully_sent[opp_player]=0;
						}
						//finished sending the message
						else {

							offset[opp_player] = -1;
							msg_fully_sent[opp_player]=1;
							queue_sent[opp_player]++;
							queue_sent[opp_player] %= MSG_NUM;


							if (current_msg[opp_player].type==INIT_MSG){
								init_msg_sent[opp_player]=1;
								msgs_to_send[opp_player][queue_head[opp_player]].type=SERVER_MSG;
								msgs_to_send[opp_player][queue_head[opp_player]].data.s_msg=s_msg[opp_player];
								queue_head[opp_player]++;
								queue_head[opp_player] %= MSG_NUM;


							}
							else if (current_msg[opp_player].type==SERVER_MSG){
								first_smsg_recv[opp_player]=1;
							}

						}

					}
				}
			}

			start_game = (init_msg_sent[player_turn] && init_msg_sent[opp_player]) ;
			if (!start_game){
				continue;
			}


			//a full message was sent to the client, now we want to recieve a reply.
			if (msg_fully_sent[player_turn] && first_smsg_recv[player_turn]){
				//time.tv_sec=20;
				ret_val=select(max_fd+1,&read_fds,NULL,NULL,&time);


				if (ret_val< 0) {
					printf("Server:failed using select function: %s\n",strerror(errno));
					break;
				}
				sock=client_sockets[player_turn];
				if (FD_ISSET(sock,&read_fds)){


					ret_val=recieveClientMessage(player_turn,sock,&c_offset[player_turn],c_msg_recieved);
					if (ret_val<0){
						printf("Failed recieving message from client #%d: %s.\n",player_turn+1,strerror(errno));
						break;
					}
					cmsg_recv[player_turn]=1;
					timer[player_turn]=(double) clock() / CLOCKS_PER_SEC;
				}
				else {
					if (cmsg_fully_recieved[player_turn]){
						cmsg_recv[player_turn]=0; //no message was recieved.
					}
				}


			}

			//checks if the second client is read-ready. If it is, try to read from it.
			sock=client_sockets[opp_player];
			if (msg_fully_sent[opp_player] && first_smsg_recv[opp_player] && FD_ISSET(client_sockets[opp_player],&read_fds)){


				ret_val=recieveClientMessage(opp_player,sock,&c_offset[opp_player],c_msg_recieved);
				cmsg_recv[opp_player]=1;
				if (ret_val<0){
					printf("Failed recieving message from client #%d: %s.\n",opp_player+1,strerror(errno));
					break;
				}
				clk=clock();
				timer[opp_player]= (double) clk / CLOCKS_PER_SEC;
			}
			else {
				//no message or part of message are pending for now
				if (cmsg_fully_recieved[opp_player]){
					cmsg_recv[opp_player]=0;
				}

			}


			//opponent message wad fully recieved.
			if (cmsg_fully_recieved[opp_player] && cmsg_recv[opp_player]){
				i = queue_head[player_turn];
				queue_head[player_turn]++;
				queue_head[player_turn] %= MSG_NUM;


				if (c_msg_recieved[opp_player].type==CHAT_MSG){
					msgs_to_send[player_turn][i].data.chat=c_msg_recieved[opp_player].data.chat;
					msgs_to_send[player_turn][i].type=CHAT_MSG;
				}
				else {


					if (c_msg_recieved[opp_player].data.client_move.heap_name=='Q'){
						//disconnect from client
						close(client_sockets[opp_player]);
						client_sockets[player_turn]=-1;
						clients_connected--;
						s_msg[player_turn].winner=CLIENT_WIN; // no need to update rest of the message. only matters is that he won.

						msgs_to_send[player_turn][i].type=SERVER_MSG;
						msgs_to_send[player_turn][i].data.s_msg=s_msg[player_turn];
						player_turn = (player_turn==0) ? 1 : 0;
						continue; //no need to add a message to the opponent's queue. He quit the game.
					}


				}


			}

			//if we haven't yet sent the full message to the client, we don't have any server/after move message to send him.
			if (!msg_fully_sent[player_turn] || !cmsg_recv[player_turn]){
				continue;
			}


			//finally, the current player's message was recieved.
			if (cmsg_fully_recieved[player_turn]){


				c_msg=c_msg_recieved[player_turn];
				if (c_msg.type==CHAT_MSG){
					i = queue_head[opp_player];

					queue_head[opp_player]++;
					queue_head[opp_player] %= MSG_NUM;
					msgs_to_send[opp_player][i].data.chat=c_msg.data.chat;
					msgs_to_send[opp_player][i].type=CHAT_MSG;
					continue;
				}


				//if we got here, we have a client move waiting.
				short num_cubes=c_msg.data.client_move.num_cubes_to_remove;
				char heap_name = c_msg.data.client_move.heap_name;

				if (heap_name==QUIT_CHAR){
					//disconnect from client
					close(client_sockets[player_turn]);
					client_sockets[player_turn]=-1;
					clients_connected--;
					s_msg[opp_player].winner=CLIENT_WIN;
					i = queue_head[opp_player];
					queue_head[opp_player]++;
					queue_head[opp_player] %= MSG_NUM;
					msgs_to_send[opp_player][i].data.s_msg=s_msg[opp_player];
					msgs_to_send[opp_player][i].type=SERVER_MSG;
					player_turn= player_turn==0 ? 1 : 0;
					continue;
				}
				else {
					// Do move.
					am_msg.legal = LEGAL_MOVE;


					if (num_cubes > 0 && (heap_name == PILE_A_CHAR) && (num_cubes <= n_a)) {
						n_a -= num_cubes;

					}
					else if (num_cubes > 0 && (heap_name == PILE_B_CHAR) && (num_cubes <= n_b)) {
						n_b -= num_cubes;
					}
					else if (num_cubes > 0 && (heap_name == PILE_C_CHAR) && (num_cubes <= n_c)) {
						n_c -= num_cubes;
					}
					else {
						// Illegal move (trying to get more cubes than available).
						am_msg.legal = ILLEGAL_MOVE;

					}

					//informing the player of the updated heaps sizes only if it is a legal move.
					if (am_msg.legal == ILLEGAL_MOVE){
						am_msg.n_a=-1;
						am_msg.n_b=-1;
						am_msg.n_c=-1;
					}
					else {
						am_msg.n_a=n_a;
						am_msg.n_b=n_b;
						am_msg.n_c=n_c;
					}


					//adding am_msg to the current player's msgs queue.
					i = queue_head[player_turn];
					queue_head[player_turn]++;
					queue_head[player_turn] %= MSG_NUM;
					msgs_to_send[player_turn][i].data.am_msg=am_msg;
					msgs_to_send[player_turn][i].type=AM_MSG;

					s_msg[opp_player].legal = am_msg.legal;
					s_msg[opp_player].n_a=n_a;
					s_msg[opp_player].n_b=n_b;
					s_msg[opp_player].n_c=n_c;
					s_msg[opp_player].cubes_removed= (am_msg.legal == ILLEGAL_MOVE)? 0 : num_cubes;
					s_msg[opp_player].heap_name=heap_name;
					s_msg[opp_player].player_turn=opp_player;

					// Checking if client won.
					if (empty_piles(n_a,n_b,n_c)) {
						// "player_turn" client won.
						s_msg[opp_player].winner = CLIENT_LOST;

						memcpy(&s_msg[player_turn],&s_msg[opp_player],sizeof(s_msg));
						s_msg[player_turn].winner= CLIENT_WIN;

						//adding the server msg to the current player's turn. letting him know he won.
						i = queue_head[player_turn];
						queue_head[player_turn]++;
						queue_head[player_turn] %= MSG_NUM;
						msgs_to_send[player_turn][i].data.s_msg=s_msg[player_turn];
						msgs_to_send[player_turn][i].type=SERVER_MSG;
					}
					else {
						s_msg[opp_player].winner=NO_WIN;
					}

					//adding the server msg to the next player's msgs queue.
					i = queue_head[opp_player];
					queue_head[opp_player]++;
					queue_head[opp_player] %= MSG_NUM;
					msgs_to_send[opp_player][i].data.s_msg=s_msg[opp_player];
					msgs_to_send[opp_player][i].type=SERVER_MSG;

				}
			}
		}
		else {
			if (start_game){
				break;
			}
		}
	}


	for (i=0; i<NUM_CLIENTS;i++){
		if (client_sockets[i]>0){
			close(client_sockets[i]);
		}
	}


	close(listening_sock);
	return (ret_val == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


