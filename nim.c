
#include "nim_protocol.h"

#define INPUT_SIZE 2

int recvall(int sockfd,void* buff,int *len){
	int total = 0; /* how many bytes we've read */
	int bytesleft = *len; /* how many we have left to read */
	int n;
	while(total < *len) {
	   n = recv(sockfd, buff+total, bytesleft, 0);
	   if (n == -1) { break; }
	   total += n;
	   bytesleft -= n;
	}
	*len = total; /* return number actually read here */
	return n == -1 ? -1:0; /*-1 on failure, 0 on success */
}

int sendall(int sockfd,void* buff,int *len){
	int total = 0; /* how many bytes we've sent */
	int bytesleft = *len; /* how many we have left to send */
	int n;
	while(total < *len) {
	   n = send(sockfd, buff+total, bytesleft, 0);
	   if (n == -1) { break; }
	   total += n;
	   bytesleft -= n;
	}
	*len = total; /* return number actually sent here */
	return n == -1 ? -1:0; /*-1 on failure, 0 on success */
}



int main(int argc , char** argv){
	char * hostname = DEFAULT_HOST;
	char* port = DEFAULT_PORT;
	struct addrinfo  hints;
	struct addrinfo * dest_addr , *rp;
	int sockfd;
	int size;
	char pile;
	short number;
	client_msg c_msg;
	server_msg s_msg;
	after_move_msg am_msg;
	if (argc==3) {
		hostname=argv[1];
		port=argv[2];
	}
	else if (argc==2) {
		hostname=argv[1];
	}

	short client_num;
	short my_turn;

	// For select.
	fd_set stdin_set;
	FD_ZERO(&stdin_set);
	FD_SET(fileno(stdin), &stdin_set);
	fd_set sock_set;
	FD_ZERO(&sock_set);


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
	        fprintf(stderr, "Client:failed to connect\n");
	        close(sockfd);
	        freeaddrinfo(dest_addr);
	        exit(1);
	    }
	freeaddrinfo(dest_addr);

	// Adding socket to select set.
	FD_SET(sockfd, &sock_set);

	int ret_val=0;

	// Getting init msg.
	msg m;
	size = sizeof(m);
	if ((ret_val=recvall(sockfd,(void *)&m, &size))<0){
		fprintf(stderr, "Client:failed to read from server\n");
		break;
	}

	if (m.type == REJECTED_MSG) {
		// Server notified the client that it was rejected.
		printf("Client rejected\n");
		// TODO: Do we need to close anything here?
		exit(1);

	} else if (m.type == INIT_MSG) {
		// Fetch client num and display messages.
		client_num = m.data.init_msg.client_num;
		my_turn = (client_num == 1);
		printf("You are client %hd\n", client_num);
		if (client_num == 1) {
			printf("Waiting to client 2 to connect\n");
		}

	} else {
		// Shouldn't get here.
		// TODO: Handle.
	}

	for (;;) {

		// Checking if server is ready to send.
		ret_val = select(sockfd + 1, &sock_set, NULL, NULL, (0, 0));
		if (ret_val == -1) {
			// Error.
			// TODO: Handle.

		} else if (ret_val == 0) {
			// No data from server.
			if (client_num == 2) {
				// Nothing to do.
				// TODO: Maybe sleep?
				continue;
			}

		} else if (ret_val == 1) {
			// Recv data from server.
			if ((ret_val=recvall(sockfd,(void *)&m, &size))<0){
				fprintf(stderr, "Client:failed to read from server\n");
				break;
			}

			switch (m.type) {
			case SERVER_MSG:
				break;

			case AM_MSG:
				if (my_turn) {
					char * am_msg = m.data.am_msg.legal == ILLEGAL_MOVE ? "Illegal move" : "Move accepted";
					printf("%s\n",am_msg);

				} else {
					if (m.data.am_msg.legal == ILLEGAL_MOVE) {
						// Notify that other client made illegal move.
						printf("Client %hd made an illegal move\n", (NUM_CLIENTS - client_num + 1));

						// Now it's my turn.
						my_turn = 1;
					}
				}

				break;

			case CHAT_MSG:
				printf("Client %hd: %s\n", m.data.chat.sender_num, m.data.chat.msg);
				break;
			}
		}

		//getting heap info and winner info from server
		size = sizeof(s_msg);
		if ((ret_val=recvall(sockfd,(void *)&s_msg, &size))<0){
			fprintf(stderr, "Client:failed to read from server\n");
			break;
		}
		printf("Heap A: %d\nHeap B: %d\nHeap C: %d\n", s_msg.n_a, s_msg.n_b, s_msg.n_c);

		if (s_msg.winner != NO_WIN) {
			char * winner = s_msg.winner == CLIENT_WIN ? "You" : "Server" ;
			printf("%s win!\n",winner);
			break;
		}
		else {
			printf("Your turn:\n");
			ret_val = select(fileno(stdin) + 1, &stdin_set, NULL, NULL, (0, 0));
			if (ret_val == -1) {
				// Error.
				// TODO: Handle.
			} else if (ret_val == 0) {
				// No input.
			}

			// If here, user is trying to add input.

			scanf(" %c", &pile); // Space before %c is to consume the newline char from the previous scanf.
			if (pile == QUIT_CHAR) {
				c_msg.heap_name=pile;
				c_msg.num_cubes_to_remove=0;
				//letting the server know we close the socket
				if ((ret_val=sendall(sockfd,(void *)&c_msg,&size))<0){
					fprintf(stderr, "Client:failed to write to server\n");
					break;
				}
				// Shutting down connection.
				shutdown(sockfd, SHUT_WR);
				char buf;
				int shutdown_res = recv(sockfd, &buf, 1, 0);
		        if (shutdown_res < 0) {
					fprintf(stderr, "Client:failed to write to server\n");
		            return EXIT_FAILURE;
		        }
		        else if (!shutdown_res) {
		        	close(sockfd);
		        	return EXIT_SUCCESS;
		        }
		        else {
		        	// Will not reach as the server will not send anything at this point.
		        	break;
		        }
			} else if (pile == MSG_CHAR) {
				// Client wants to send a message. Getting the message.
				char msg[256];
				scanf("SG %s\n", &msg);

				// Sending message to server.
				// TODO

			} else {
				// Client wants to make his move. Getting the pile number.
				scanf(" %hd", &number);

				//sending the move to the server
				c_msg.heap_name=pile;
				c_msg.num_cubes_to_remove=number;
				size = sizeof(c_msg);
				if ((ret_val=sendall(sockfd,(void *)&c_msg,&size))<0){
					fprintf(stderr, "Client:failed to write to server\n");
					break;
				}
				//receiving from server a message if that was a legal move or not
				size = sizeof(am_msg);
				if ((ret_val=recvall(sockfd,(void *)&am_msg,&size)<0)){
					fprintf(stderr, "Client:failed to read from server\n");
					break;
				}
				char * msg = am_msg.legal == ILLEGAL_MOVE ? "Illegal move\n" : "Move accepted\n";
				printf("%s",msg);
			}
		}
	}

	close(sockfd);
	return (ret_val == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
