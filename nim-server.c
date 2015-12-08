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
		if (DEBUG){
			printf("off 0\n");
		}
		memset(&(c_msg_recieved[player_turn]),0,sizeof(client_msg));
	}

	size =sizeof(client_msg)-*off;
	ret_val = recv(sock,&(c_msg_recieved[player_turn])+*off,size,0);
	if (DEBUG){
		printf("received msg\n");
	}
	if (ret_val ==-1){
		return -1;
	}
	else{
		if (ret_val<size){
			cmsg_fully_recieved[player_turn]=0;
			*off+=ret_val;
		}
		else {
			if (DEBUG){
				printf("tomer3e\n");
			}
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
	if (DEBUG){
		printf("Omer 001\n");
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
	if (DEBUG){
		printf("Omer 002\n");
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
	if (DEBUG){
		printf("Omer 003\n");
	}

	int ret_val=0;
	int player_turn =0; // player 0 or 1
	int clients_connected=0;
	msg msgs_to_send[NUM_CLIENTS][MSG_NUM]; //void because it can be any kind of server msg.
	int queue_tail[NUM_CLIENTS];
	int queue_head[NUM_CLIENTS];
	short init_msg_sent[NUM_CLIENTS];
	client_msg c_msg_recieved[NUM_CLIENTS];
	msg_type types[NUM_CLIENTS];
	msg_type temp_type;
	int start_game=0;
	struct timeval time;
	short first_smsg_recv[NUM_CLIENTS];
	short cmsg_recv[NUM_CLIENTS]; //if there is a message sent from client(does not have to be a full msg)

	//initilizing arrays to all 0
	cmsg_fully_recieved[0] = 1;
	cmsg_fully_recieved[1] = 1;

	memset(init_msg_sent,0,NUM_CLIENTS);
	memset(first_smsg_recv,0,NUM_CLIENTS);
	memset(cmsg_recv,0,NUM_CLIENTS);
	memset(queue_tail,0,NUM_CLIENTS);
	memset(queue_head,0,NUM_CLIENTS);
	memset(msg_fully_sent,0,NUM_CLIENTS);
	memset(offset,0,NUM_CLIENTS);
	memset(c_offset,0,NUM_CLIENTS);

	if (DEBUG){
		printf("Omer 004\n");
	}

	timer[0] = (double) clock() / CLOCKS_PER_SEC;
	// Main loop.
	while (1) {
		if (DEBUG){
			printf("Omer 005\n");
		}

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
		if (DEBUG){
			printf("Omer 007\n");
		}
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
			if (clients_connected<=2){
				if (DEBUG){
					printf("adding init msg for client #%d\n",init_s_msg.client_num+1);
				}

				temp_type=INIT_MSG;
				msgs_to_send[init_s_msg.client_num][0].type=INIT_MSG;
				msgs_to_send[init_s_msg.client_num][0].data.init_msg=init_s_msg;
			}

			//try letting the 3rd client know he is rejected.


			if (clients_connected>2){
				if (FD_ISSET(new_sock,&write_fds)){
					temp_type=REJECTED_MSG;
					size = sizeof(temp_type);
					if (send(new_sock,&temp_type,size,0)==-1){
						printf("Error sending message to client #%d: %s\n",clients_connected,strerror(errno));
					}
					close(new_sock);
					client_sockets[clients_connected--]=-1;
				}
			}


		}

		//its an IO operation. all sockets are connected.
		else  if (clients_connected>0){

			if (DEBUG){
				printf("Hurray!\n");
			}
			timer[player_turn] = -1*timer[player_turn] +(double) clock()/ CLOCKS_PER_SEC ;

			if (timer[player_turn]>=60){
				close(client_sockets[player_turn]);
				player_turn = player_turn==0 ? 1: 0;
				continue;
			}

			printf("Omer 013\n");
			int opp_player = player_turn == 0 ? 1 : 0 ;
			//if player's message was not fully recieved, we won't send him a new message until we recieve full message
			if (cmsg_fully_recieved[player_turn]){

				printf("Omer 014\n");

				sock=client_sockets[player_turn];

				ret_val=select(max_fd+1,NULL,&write_fds,NULL,NULL);
				if (ret_val< 0) {
					printf("Server:failed using select function: %s\n",strerror(errno));
					break;
				}

				printf("Omer 015\n");
				//trying to send the server msg or any previous server msg to client.
				if (FD_ISSET(sock,&write_fds)){

					printf("Omer 016\n");
					i= queue_head[player_turn];
					if (DEBUG){
						printf("queue head:%d, client #%d\n",i,player_turn+1);
					}
					if (start_game || msgs_to_send[player_turn][i].type == INIT_MSG ) {

						printf("Omer 017\n");

						size=sizeof(msgs_to_send[player_turn][i])-offset[player_turn]; //decrease the num of bytes already sent.
						//first byte sent is the type and it will be sent for sure(at least one byte is sent).
						ret_val = send(sock, &(msgs_to_send[player_turn][i])+offset[player_turn], size,0);
						if (ret_val ==-1){
							printf("Error sending message to client #%d: %s\n",player_turn-1,strerror(errno));
							break;
						}
						//part of msg was sent. update the offset to start next send from bytes that haven't been sent yet.
						else if (ret_val<size) {

							printf("Omer 018\n");
							offset[player_turn] +=ret_val ;
							msg_fully_sent[player_turn]=0;
						}
						//finished sending the message
						else {

							queue_head[player_turn]++;
							queue_head[player_turn]%=MSG_NUM;
							offset[player_turn]=0;
							msg_fully_sent[player_turn]=1;
							switch (msgs_to_send[player_turn][i].type){
							case (AM_MSG):
									player_turn= player_turn==0 ? 1 : 0;
							break;

							case (INIT_MSG):

							init_msg_sent[player_turn]=1;

							queue_tail[player_turn]++;
							queue_tail[player_turn]%= MSG_NUM;
							msgs_to_send[player_turn][queue_tail[player_turn]].type=SERVER_MSG;
							msgs_to_send[player_turn][queue_tail[player_turn]].data.s_msg=s_msg[player_turn];

							if (DEBUG){
								printf("sent init\n");
							}
							break;

							case(SERVER_MSG):
							if (DEBUG){
								printf("Omer 021\n");
							}
							if (!first_smsg_recv[player_turn]){
								first_smsg_recv[player_turn]=1;
								//first server msg was sent.the tail is now the next index.
								queue_tail[player_turn]++;
								queue_tail[player_turn]%= MSG_NUM;
							}
							if (msgs_to_send[player_turn][i].data.s_msg.winner != NO_WIN){
								close(client_sockets[player_turn]);
								clients_connected--;
								client_sockets[player_turn]=-1;
								player_turn= (player_turn==0)? 1: 0;
							}
							break;

							default:
								break;
							}
						}

					}
				}
			}


			if (cmsg_fully_recieved[opp_player]){

				printf("Omer 022\n");

				i= queue_head[opp_player];
				if (msgs_to_send[opp_player][i].type==CHAT_MSG || msgs_to_send[opp_player][i].type==INIT_MSG ||
						(msgs_to_send[opp_player][i].type==SERVER_MSG && !first_smsg_recv[opp_player])){

					printf("Omer 023\n");
					sock=client_sockets[opp_player];
					if (DEBUG){
						printf("before select of opponent.Maybe no need for another select.\n");
					}
					ret_val=select(max_fd+1,NULL,&write_fds,NULL,NULL);
					if (DEBUG){
						printf("after select of opp.\n");
					}
					if (ret_val< 0) {
						printf("Server:failed using select function: %s\n",strerror(errno));
						break;
					}
					//trying to send the server msg or any previous server msg to client.
					if (FD_ISSET(sock,&write_fds)){

						printf("Omer 024\n");

						size=sizeof(msgs_to_send[opp_player][i])-offset[opp_player]; //decrease the num of bytes already sent.
						//first byte sent is the type and it will be sent for sure(at least one byte is sent).
						ret_val = send(sock, &(msgs_to_send[opp_player][i])+offset[opp_player], size,0);


						printf("Omer 025\n");
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

							printf("Omer 026\n");
							if (msgs_to_send[opp_player][i].type==INIT_MSG){
								init_msg_sent[opp_player]=1;
								queue_tail[opp_player] = (queue_tail[opp_player]+1) % MSG_NUM;
								msgs_to_send[opp_player][queue_tail[opp_player]].type=SERVER_MSG;
								msgs_to_send[opp_player][queue_tail[opp_player]].data.s_msg=s_msg[opp_player];
								if (DEBUG){
									printf("init msg sent!\n");
								}
							}
							else if (msgs_to_send[opp_player][i].type==SERVER_MSG){
								first_smsg_recv[opp_player]=1;
								queue_tail[opp_player] = (queue_tail[opp_player]+1) % MSG_NUM;
							}
							queue_head[opp_player]++;
							queue_head[opp_player]%=MSG_NUM;
							offset[opp_player]=0;
							msg_fully_sent[opp_player]=1;


						}

					}
				}
			}

			printf("Omer 027\n");
			start_game = (init_msg_sent[player_turn] && init_msg_sent[opp_player]) ;
			if (!start_game){
				continue;
			}


			//a full message was sent to the client, now we want to recieve a reply.
			if (msg_fully_sent[player_turn] && first_smsg_recv[player_turn]){

				printf("Omer 028\n");
				time.tv_sec=20;
				ret_val=select(max_fd+1,&read_fds,NULL,NULL,&time);

				printf("Omer 029\n");
				if (ret_val< 0) {
					printf("Server:failed using select function: %s\n",strerror(errno));
					break;
				}
				if (FD_ISSET(sock,&read_fds)){

					printf("Omer 030\n");
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


				printf("Omer 031\n");
			}

			//checks if the second client is read-ready. If it is, try to read from it.

			if (msg_fully_sent[opp_player] && first_smsg_recv[opp_player] && FD_ISSET(client_sockets[opp_player],&read_fds)){

				printf("Omer 032\n");
				sock=client_sockets[opp_player];
				ret_val=recieveClientMessage(opp_player,sock,&c_offset[opp_player],c_msg_recieved);
				cmsg_recv[opp_player]=1;
				printf("Omer 033\n");
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
				if (DEBUG){
					printf("Tomer 033.5\n");
				}

			}

			printf("Omer 034\n");

			//opponent message wad fully recieved.
			if (cmsg_fully_recieved[opp_player] && cmsg_recv[opp_player]){
				i=queue_tail[player_turn] % MSG_NUM;
				queue_tail[player_turn]=i+1;

				printf("Omer 035\n");

				if (c_msg_recieved[opp_player].type==CHAT_MSG){
					msgs_to_send[player_turn][i].data.chat=c_msg_recieved[opp_player].data.chat;
					msgs_to_send[player_turn][i].type=CHAT_MSG;
				}
				else {

					printf("Omer 036\n");
					if (c_msg_recieved[opp_player].data.client_move.heap_name=='Q'){
						//disconnect from client
						close(client_sockets[opp_player]);
						client_sockets[player_turn]=-1;
						clients_connected--;
						s_msg[player_turn].winner=CLIENT_WIN; // no need to update rest of the message. only matters is that he won.
						i=queue_tail[player_turn] % MSG_NUM;
						queue_tail[player_turn]=i+1;
						msgs_to_send[player_turn][i].type=SERVER_MSG;
						msgs_to_send[player_turn][i].data.s_msg=s_msg[player_turn];
						player_turn = (player_turn==0) ? 1 : 0;
						continue; //no need to add a message to the opponent's queue. He quit the game.
					}

					printf("Omer 037\n");
				}


			}

			//if we haven't yet sent the full message to the client, we don't have any server/after move message to send him.
			if (!msg_fully_sent[player_turn] || !cmsg_recv[player_turn]){
				continue;
			}

			printf("Omer 038\n");

			//finally, the current player's message was recieved.
			if (cmsg_fully_recieved[player_turn]){

				printf("Omer 039\n");

				c_msg=c_msg_recieved[player_turn];
				if (types[player_turn]==CHAT_MSG){
					i=queue_tail[opp_player] % MSG_NUM;
					queue_tail[opp_player]=i+1;
					msgs_to_send[opp_player][i].data.chat=c_msg.data.chat;
					msgs_to_send[opp_player][i].type=CHAT_MSG;
					continue;
				}

				printf("Omer 040\n");
				//if we got here, we have a client move waiting.
				short num_cubes=c_msg.data.client_move.num_cubes_to_remove;
				char heap_name = c_msg.data.client_move.heap_name;
				if (DEBUG){
					printf("num_cubes:%d , heap:%c\n",num_cubes,heap_name);
				}
				if (heap_name==QUIT_CHAR){
					//disconnect from client
					close(client_sockets[player_turn]);
					client_sockets[player_turn]=-1;
					clients_connected--;
					s_msg[opp_player].winner=CLIENT_WIN;
					i=queue_tail[opp_player] % MSG_NUM;
					queue_tail[opp_player]=i+1;
					msgs_to_send[opp_player][i].data.s_msg=s_msg[opp_player];
					msgs_to_send[opp_player][i].type=SERVER_MSG;

					printf("Omer 041\n");
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
						if (DEBUG){
							printf("illegal\n");
						}
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

					printf("Omer 042\n");

					//adding am_msg to the current player's msgs queue.
					i=queue_tail[player_turn] % MSG_NUM;
					queue_tail[player_turn]=i+1;
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
						i=queue_tail[player_turn] % MSG_NUM;
						queue_tail[player_turn]=i+1;
						msgs_to_send[player_turn][i].data.s_msg=s_msg[player_turn];
						msgs_to_send[player_turn][i].type=SERVER_MSG;
					}
					else {
						s_msg[opp_player].winner=NO_WIN;
					}

					//adding the server msg to the next player's msgs queue.
					i=queue_tail[opp_player] % MSG_NUM;
					queue_tail[opp_player]=i+1;
					msgs_to_send[opp_player][i].data.s_msg=s_msg[opp_player];
					msgs_to_send[opp_player][i].type=SERVER_MSG;

					printf("Omer 043\n");
				}
			}
		}
		else {
			if (start_game){
				break;
			}
		}
	}

	printf("Omer 044\n");
	for (i=0; i<NUM_CLIENTS;i++){
		if (client_sockets[i]>0){
			close(client_sockets[i]);
		}
	}

	printf("Omer 045\n");
	close(listening_sock);
	return (ret_val == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


