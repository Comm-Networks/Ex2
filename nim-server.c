#include <stdlib.h>
#include "nim_protocol.h"



//check if all piles are empty
int empty_piles(int n_a,int n_b,int n_c) {
	return (n_a == 0) && (n_b == 0) && (n_c == 0);
}




int main(int argc , char** argv) {
	const char hostname[] = DEFAULT_HOST;
	char* port = DEFAULT_PORT;

	int listening_sock;
	unsigned int size;
	int new_sock,sock;
	int max_fd;
	int client_sockets[NUM_CLIENTS+1]; //needs another socket for rejected client


	fd_set read_fds;
	fd_set write_fds;

	client_msg c_msg[NUM_CLIENTS];
	void c_msg_recieved[NUM_CLIENTS];
	server_msg s_msg[NUM_CLIENTS];
	after_move_msg am_msg;
	init_server_msg init_s_msg;
	chat_msg chat[NUM_CLIENTS];
	struct addrinfo  hints;
	struct addrinfo * my_addr , *rp;
	struct sockaddr_in client_adrr;

	int i,j,k=0;
	int offset;

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




	struct timeval time;
	int ret_val=0;
	int player_turn =0; // player 0 or 1
	int clients_connected=0;
	void msgs_to_send[NUM_CLIENTS][MSG_NUM]; //void because it can be any kind of server msg.
	msg_type types_of_msgs[NUM_CLIENTS][MSG_NUM]; // types of messages to send;
	int msgs_index[NUM_CLIENTS];
	int first_not_sent_1=0;	int first_not_sent_2=0;
	int msg_fully_sent[NUM_CLIENTS];
	int cmsg_fully_recieved[NUM_CLIENTS];
	msg_type type;


	memset(msgs_index,0,NUM_CLIENTS);
	memset(msg_fully_sent,0,NUM_CLIENTS);
	memset(cmsg_fully_recieved,0,NUM_CLIENTS);




	// Main loop.
	for (;;) {

		player_turn = (++player_turn) % 1;
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

		//return read-ready sockets
		ret_val= select(max_fd+1,&read_fds,NULL,NULL,NULL);
		if (ret_val<0){
			printf("Server:failed using select function: %s\n",strerror(errno));
			break;
		}
		//listening socket is read-ready - can accept
		if (FD_ISSSET(listening_sock,&read_fds)){
			size = sizeof(struct sockaddr_in);
			new_sock = accept(listening_sock, (struct sockaddr*)&client_adrr, &size);
			if (new_sock<0){
				printf("problem while trying to accept incoming call : %s\n",strerror(errno));
				continue;
			}
			init_s_msg.client_num= ++clients_connected;
			client_sockets[init_s_msg.client_num-1]=new_sock;
			FD_SET(new_sock,&write_fds);
			max_fd = (new_sock>max_fd)? new_sock : max_fd;

			//finding the clients that are send-ready.
			ret_val=select(max_fd+1,NULL,&write_fds,NULL,(0,0));
			if (ret_val< 0) {
				printf("Server:failed using select function: %s\n",strerror(errno));
				break;
			}

			//try to send the initial message to the client/ letting him know he is rejected.
			if (FD_ISSET(new_sock,&write_fds)){

				if (clients_connected>2){
					type=REJECTED_MSG;
					size = sizeof(type);
					if (send(new_sock,&type,size)==-1){
						//need to implement
					}
					close(new_sock);
					client_sockets[clients_connected--]=-1;
				}

				//not a reject client
				else {
					type=INIT_MSG;
					types_of_msgs[player_turn][msgs_index[player_turn]]=type;
					size = sizeof(type);
					//informs the user which message he is going to recieve now
					if ((ret_val=send(new_sock,&type,size))==-1){
						//need to implement what to do when send fails.
					}
					ret_val=select(max_fd+1,NULL,&write_fds,NULL,NULL);
					if (ret_val< 0) {
						printf("Server:failed using select function: %s\n",strerror(errno));
						break;
					}
					//sending the init message if the socket is ready to recieve.
					if (FD_ISSET(new_sock,&write_fds)){
						size=sizeof(init_s_msg);
						if ((ret_val=send(new_sock,&init_s_msg,size))<size){
							if (ret_val==-1){
								//need to implement.
							}
							msgs_to_send[player_turn][msgs_index[player_turn]++]=&init_s_msg+ret_val;
						}
						else {
							msg_fully_sent[player_turn]=1;
							if (player_turn==0){
								first_not_sent_1++;
							}
							else {
								first_not_sent_2++;
							}
						}
					}
					else {
						msgs_to_send[player_turn][msgs_index[player_turn]++]=init_s_msg;
					}
				}
			}
			else {
				types_of_msgs[player_turn][msgs_index[player_turn]]=INIT_MSG;
				msgs_to_send[player_turn][msgs_index[player_turn]++]=init_s_msg;
			}

		}

		//its an IO operation. all sockets are connected.
		else if (clients_connected==2) {

			//if player's message was not fully recieved, we won't send him a new server message for now
			if (cmsg_fully_recieved[player_turn]){
				s_msg[player_turn].player_turn=(short) player_turn;
				//sending the first message we have't sent yet.
				i=msgs_index[player_turn] % MSG_NUM;
				msgs_index[player_turn]=i++;
				msgs_to_send[player_turn][i]=s_msg[player_turn];
				types_of_msgs[player_turn][i]=SERVER_MSG;
				sock=client_sockets[player_turn];

				ret_val=select(max_fd+1,NULL,&write_fds,NULL,NULL);
				if (ret_val< 0) {
					printf("Server:failed using select function: %s\n",strerror(errno));
					break;
				}
				//trying to send the server msg or any previous server msg to client.
				if (FD_ISSET(sock,&write_fds)){
					i= player_turn==0 ? first_not_sent_1 : first_not_sent_2;
					size=sizeof(msgs_to_send[player_turn][i]);
					type = types_of_msgs[player_turn][i];
					ret_val = send(sock,&type,sizeof(type));
					if (ret_val<0){
						//implement...
					}

					ret_val = send(sock, &msgs_to_send[player_turn][i], size);
					if (ret_val ==-1){
						//implement
					}
					//part of msg was sent. update the msg in the array to the part that wasn't sent.
					else if (ret_val<size ) {
						offset = sizeof(msgs_to_send[player_turn][i]) -size ;
						&(msgs_to_send[player_turn][i])+=offset;
						msg_fully_sent[player_turn]=0;
					}
					else {
						if (player_turn==0){
							first_not_sent_1++;
						}
						else {
							first_not_sent_2++;
						}
						msg_fully_sent[player_turn]=1;
					}
				}
			}


			ret_val=select(max_fd+1,&read_fds,NULL,NULL,NULL);
			if (ret_val< 0) {
				printf("Server:failed using select function: %s\n",strerror(errno));
				break;
			}

			//a full message was sent to the client, now we want to recieve a reply.
			if (msg_fully_sent[player_turn]){

				if (FD_ISSET(sock,&read_fds)){

					ret_val = recv(sock,&type,sizeof(type));
					if (type==CHAT_MSG){
						size =sizeof(chat_msg)-j;
						ret_val = recv(sock,&c_msg_recieved[player_turn],size);
						if (ret_val ==-1){
							//implement
						}
						else if (ret_val<size){
							cmsg_fully_recieved[player_turn]=0;
							//updating the client message to be the message we recieved so far.
							memcpy(chat+player_turn+j,c_msg_recieved+player_turn,sizeof(c_msg_recieved[player_turn]));
							j+=size;
						}
						else {
							cmsg_fully_recieved[player_turn]=1;
							memcpy(chat+player_turn+j,c_msg_recieved+player_turn,sizeof(c_msg_recieved[player_turn]));
							j=0;
						}
						memset(&c_msg_recieved[player_turn],0,size); // initlializing the message recieved to recieve another message next time.

					}
					else {

						size = sizeof(client_msg)-j;
						ret_val = recv(sock,&c_msg_recieved[player_turn],size);
						if (ret_val ==-1){
							//implement...
						}
						else if (ret_val<size){
							cmsg_fully_recieved[player_turn]=0;
							//updating the client message to be the message we recieved so far.
							memcpy(c_msg+player_turn+j,c_msg_recieved+player_turn,sizeof(c_msg_recieved[player_turn]));
							j+=size;
						}
						else {
							cmsg_fully_recieved[player_turn]=1;
							memcpy(c_msg+player_turn+j,c_msg_recieved+player_turn,sizeof(c_msg_recieved[player_turn]));
							j=0;
						}
						memset(&c_msg_recieved[player_turn],0,size); // initlializing the message recieved to recieve another message next time.
					}
				}
			}
			//checks if the second client is read-ready. If it is, try to read from it.
			int opp_player = (player_turn+1) % 1 ;
			if (FD_ISSET(client_sockets[opp_player],&read_fds)){
				sock=client_sockets[opp_player];
				size = sizeof(client_msg)-k;
				ret_val = recv(sock,&c_msg_recieved[opp_player],size);
				if (ret_val<size){
					cmsg_fully_recieved[opp_player]=0;
					memcpy(c_msg+opp_player+k,c_msg_recieved+opp_player,sizeof(c_msg_recieved[opp_player]));
					k+=size;
				}
				else {
					//client message fully recieved.
					cmsg_fully_recieved[opp_player]=1;
					memcpy(c_msg+opp_player+k,c_msg_recieved+opp_player,sizeof(c_msg_recieved[opp_player]));
					k=0;
				}
			}


			//opponent message wad fully recieved. update only the message.not his turn.
			if (cmsg_fully_recieved[opp_player]){
				chat=c_msg[opp_player].msg;
				if (chat!=NULL){
					i=msgs_index[player_turn] % MSG_NUM;
					msgs_index[player_turn]=i++;
					msgs_to_send[player_turn][i]=chat;
				}
			}

			if (!msg_fully_sent[player_turn]){
				continue;
			}

			//finally, the current player's message was recieved.
			if (cmsg_fully_recieved[player_turn]){
				short num_cubes=c_msg[player_turn].num_cubes_to_remove;
				char heap_name = c_msg[player_turn].heap_name;
				chat=c_msg[player_turn].msg;
				if (chat!=NULL){
					i=msgs_index[opp_player] % MSG_NUM;
					msgs_index[opp_player]=i++;
					msgs_to_send[opp_player][i]=chat;
				}
				if (heap_name==QUIT_CHAR){
					//disconnect from client
					close(client_sockets[player_turn]);
					client_sockets[player_turn]=-1;
					clients_connected--;
				}
				else {
					// Do move.
					am_msg.legal = LEGAL_MOVE;
					if (num_cubes > 0) {
						if ((heap_name == PILE_A_CHAR) && (num_cubes <= n_a)) {
							n_a -= num_cubes;

						}
						else if ((heap_name == PILE_B_CHAR) && (num_cubes <= n_b)) {
							n_b -= num_cubes;
						}
						else if ((heap_name == PILE_C_CHAR) && (num_cubes <= n_c)) {
							n_c -= num_cubes;
						}
						else {
							// Illegal move (trying to get more cubes than available).
							am_msg.legal = ILLEGAL_MOVE;
						}
						s_msg[opp_player].n_a=n_a;
						s_msg[opp_player].n_b=n_b;
						s_msg[opp_player].n_c=n_c;
						s_msg[opp_player].cubes_removed=num_cubes;
						s_msg[opp_player].heap_name=heap_name;
						// Checking if client won.
						if (empty_piles(n_a,n_b,n_c)) {
							// "player_turn" client won.
							s_msg[opp_player].winner = CLIENT_LOST; // Letting the other client know he lost.
						}
					}
					else {
						// Ithllegal move (number of cubes to remove not positive).
						am_msg.legal = ILLEGAL_MOVE;
					}
					size = sizeof(am_msg);
					ret_val=select(max_fd+1,NULL,&write_fds,NULL,(0,0));
					if (ret_val< 0) {
						printf("Server:failed using select function: %s\n",strerror(errno));
						close(client_sockets[player_turn]);
						close(client_sockets[opp_player]);
						break;
					}
					if (ISSET(client_sockets[player_turn],&write_fds)){


					}



				}

			}











		}





	}



	// -------- ex1 nim app ---------  //

	/*
		size = sizeof(s_msg);
		ret_val = sendall(new_sock, &s_msg, &size);
		if (ret_val < 0) {
			fprintf(stderr, "Server:failed to send to client\n");
			break;
		}

		// Ending loop if we had a winner in previous iteration.
		if (s_msg.winner != NO_WIN) {
			break;
		}

		size = sizeof(c_msg);
		if ((ret_val = recvall(new_sock, &c_msg, &size)) < 0) {
			fprintf(stderr, "Server:failed to send to client\n");
			break;
		}

		if (c_msg.heap_name == QUIT_CHAR){
			break;
		}


		// Do move.
		am_msg.legal = LEGAL_MOVE;
		if (c_msg.num_cubes_to_remove > 0) {
			if ((c_msg.heap_name == PILE_A_CHAR) && (c_msg.num_cubes_to_remove <= s_msg.n_a)) {
				s_msg.n_a -= c_msg.num_cubes_to_remove;
			}
			else if ((c_msg.heap_name == PILE_B_CHAR) && (c_msg.num_cubes_to_remove <= s_msg.n_b)) {
				s_msg.n_b -= c_msg.num_cubes_to_remove;
			}
			else if ((c_msg.heap_name == PILE_C_CHAR) && (c_msg.num_cubes_to_remove <= s_msg.n_c)) {
				s_msg.n_c -= c_msg.num_cubes_to_remove;
			}
			else {
				// Illegal move (trying to get more cubes than available).
				am_msg.legal = ILLEGAL_MOVE;
			}
		}
		else {
			// Illegal move (number of cubes to remove not positive).
			am_msg.legal = ILLEGAL_MOVE;
		}

		size = sizeof(am_msg);
		if ((ret_val=sendall(new_sock, &am_msg, &size)) < 0) {
			fprintf(stderr, "Server:failed to send to client\n");
			break;
		}

		// Checking if client won.
		if (empty_piles(s_msg.n_a,s_msg.n_b,s_msg.n_c)) {
			// Client won.
			s_msg.winner = CLIENT_WIN; // Letting the client know.

		}
	 */
	//}
	close(listening_sock);
	return (ret_val == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

