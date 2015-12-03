
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
	size = sizeof(current_s_msg);
	while ((ret_val = recv(sockfd, &current_s_msg + current_s_msg_offset, size, 0)) < size){
		if (ret_val==-1){
			fprintf(stderr, "Client:failed to read from server\n");
			exit(1);
		} else {
			// Message received partially.
			size -= ret_val;
			current_s_msg_offset += ret_val;
		}
	}

	current_s_msg_offset = 0; // Resetting for next use.

	if (current_s_msg.type == REJECTED_MSG) {
		// Server notified the client that it was rejected.
		printf("Client rejected\n");
		// TODO: Do we need to close anything here? //why not?
		exit(1);

	} else if (current_s_msg.type == INIT_MSG) {
		// Fetch client num and display messages.
		client_num = current_s_msg.data.init_msg.client_num;
		my_turn = (client_num == 1);
		printf("You are client %hd\n", client_num);
		if (client_num == 1) {
			printf("Waiting to client 2 to connect\n");
		}

	} else {
		// Shouldn't get here.
		// TODO: Handle.
	}

	while (!end_game) {

		// Checking if server is ready to send.
		ret_val = select(sockfd + 1, &sock_set, NULL, NULL, (0, 0));
		if (ret_val == -1) {
			// Error.
			// TODO: Handle.

		} else if (ret_val == 0) {
			// No data from server.
			if (client_num == 2) {
				// Nothing to do because client 1 is starting.
				// TODO: Maybe sleep? //No need. Continue is ok.
				continue;
			}

		} else if (ret_val == 1) {
			// Recv data from server.
			size = sizeof(msg) - current_s_msg_offset;
			ret_val = recv(sockfd, &current_s_msg + current_s_msg_offset, size, 0);
			if (ret_val == -1) {
				// Error.
				// TODO: Handle.
			} else if (ret_val < size) {
				// Message partially received. Updating offset.
				current_s_msg_offset += ret_val;

			} else {
				// Message fully received. Resetting offset and processing.
				current_s_msg_offset = 0;
				data_union* data = &current_s_msg.data;

				switch (current_s_msg.type) {
				case SERVER_MSG:
					if (data->s_msg.winner != NO_WIN) {
						char * result = data->s_msg.winner == CLIENT_WIN ? "win" : "lose" ;
						printf("You %s!\n", result);
						end_game = 1;

					} else {
						// Game continues.
						if (data->s_msg.player_turn != client_num) {
							// Other's turn.
							if (data->s_msg.legal == LEGAL_MOVE) {
								printf("Client %hd removed %hd cubes from heap %c\n", data->s_msg.player_turn, data->s_msg.cubes_removed, data->s_msg.heap_name);
								printf("Heap A: %d\nHeap B: %d\nHeap C: %d\n", data->s_msg.n_a, data->s_msg.n_b, data->s_msg.n_c);
							} else {
								printf("Client %hd made an illegal move\n", data->s_msg.player_turn);
							}
						} else {
							// Your turn.
							printf("Heap A: %d\nHeap B: %d\nHeap C: %d\n", data->s_msg.n_a, data->s_msg.n_b, data->s_msg.n_c);
						}
					}

					break;

				case AM_MSG:
					if (1) {
						char * am_print = data->am_msg.legal == ILLEGAL_MOVE ? "Illegal move" : "Move accepted";
						printf("%s\n",am_print);

						if (data->am_msg.legal == LEGAL_MOVE) {
							// Print updated piles list after legal move.
							printf("Heap A: %d\nHeap B: %d\nHeap C: %d\n", data->am_msg.n_a, data->am_msg.n_b, data->am_msg.n_c);
						}
					}

					break;

				case CHAT_MSG:
					printf("Client %hd: %s\n", data->chat.sender_num, data->chat.msg);
					break;

				default:
					break;
				}
			}

		}

//		//getting heap info and winner info from server
//		size = sizeof(s_msg);
//		if ((ret_val=recvall(sockfd,(void *)&s_msg, &size))<0){
//			fprintf(stderr, "Client:failed to read from server\n");
//			break;
//		}
//		printf("Heap A: %d\nHeap B: %d\nHeap C: %d\n", s_msg.n_a, s_msg.n_b, s_msg.n_c);
//
//		if (s_msg.winner != NO_WIN) {
//			char * winner = s_msg.winner == CLIENT_WIN ? "You" : "Server" ;
//			printf("%s win!\n",winner);
//			break;
//		}
		if (1) { // TODO: Set if condition.
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
				msg_queue[queue_head].type = CLIENT_MOVE_MSG;
				msg_queue[queue_head].data.client_move.heap_name = pile;
				msg_queue[queue_head].data.client_move.num_cubes_to_remove = 0;
				queue_head++;
//				//letting the server know we close the socket
//				if ((ret_val=sendall(sockfd,(void *)&c_msg,&size))<0){
//					fprintf(stderr, "Client:failed to write to server\n");
//					break;
//				}
//				// Shutting down connection.
//				shutdown(sockfd, SHUT_WR);
//				char buf;
//				int shutdown_res = recv(sockfd, &buf, 1, 0);
//		        if (shutdown_res < 0) {
//					fprintf(stderr, "Client:failed to write to server\n");
//		            return EXIT_FAILURE;
//		        }
//		        else if (!shutdown_res) {
//		        	close(sockfd);
//		        	return EXIT_SUCCESS;
//		        }
//		        else {
//		        	// Will not reach as the server will not send anything at this point.
//		        	break;
//		        }
			} else if (pile == MSG_CHAR) {
				// Client wants to send a message. Getting the message.
				// M was removed here intentionally. We already read it...
				scanf("SG");
				fgets(msg_queue[queue_head].data.chat.msg, MSG_MAX_SIZE, stdin);
				msg_queue[queue_head].type = CHAT_MSG;
				msg_queue[queue_head].data.chat.sender_num = client_num;
				queue_head++;

				// Sending message to server.
				// TODO

			} else {
				// Client wants to make his move. Getting the pile number.
				scanf(" %hd", &number);

				//sending the move to the server
				msg_queue[queue_head].type = CLIENT_MOVE_MSG;
				msg_queue[queue_head].data.client_move.heap_name = pile;
				msg_queue[queue_head].data.client_move.num_cubes_to_remove = number;
				queue_head++;
//				size = sizeof(c_msg);
//				if ((ret_val=sendall(sockfd,(void *)&c_msg,&size))<0){
//					fprintf(stderr, "Client:failed to write to server\n");
//					break;
//				}
//				//receiving from server a message if that was a legal move or not
//				size = sizeof(am_msg);
//				if ((ret_val=recvall(sockfd,(void *)&am_msg,&size)<0)){
//					fprintf(stderr, "Client:failed to read from server\n");
//					break;
//				}
//				char * msg = am_msg.legal == ILLEGAL_MOVE ? "Illegal move\n" : "Move accepted\n";
//				printf("%s",msg);
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
				ret_val = select(sockfd + 1, NULL, &sock_set, NULL, (0, 0));
				if (ret_val == -1) {
					// Error.
					// TODO: Handle.

				} else if (ret_val == 0) {
					// Can't send data to server.
					// TODO: Handle.

				} else if (ret_val == 1) {
					// Sending data to server.
					ret_val = send(sockfd, &current_msg + current_msg_offset, size, 0);
					if (ret_val == -1) {
						// Error.
						// TODO: Handle.
					} else if (ret_val < size) {
						// Message partially sent. Updating offset of current message.
						current_msg_offset += ret_val;
					} else {
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
