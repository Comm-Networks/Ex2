#include <stdlib.h>
#include "nim_protocol.h"


int recvall(int sockfd,void* buff,unsigned int *len){
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

int sendall(int sockfd,void* buff,unsigned int *len){
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

//check if all piles are empty
int empty_piles(int n_a,int n_b,int n_c) {
	return (n_a == 0) && (n_b == 0) && (n_c == 0);
}

int main(int argc , char** argv) {
	const char hostname[] = DEFAULT_HOST;
	char* port = DEFAULT_PORT;

	int listening_sock;
	unsigned int size;
	int new_sock;
	int max_fd;
	int client_sockets[NUM_CLIENTS];

	fd_set read_fds;
	fd_set write_fds;

	client_msg c_msg;
	server_msg s_msg;
	after_move_msg am_msg;
	init_server_msg init_s_msg;
	struct addrinfo  hints;
	struct addrinfo * my_addr , *rp;
	struct sockaddr_in client_adrr;

	int i;

	if (argc < 4) {
		// Error.
		return EXIT_FAILURE;
	}

	memset(client_sockets,0,sizeof(client_sockets));

	// Initializing piles.
	s_msg.n_a = (int)strtol(argv[1], NULL, 10);
	s_msg.n_b = (int)strtol(argv[2], NULL, 10);
	s_msg.n_c = (int)strtol(argv[3], NULL, 10);
	s_msg.winner = NO_WIN;

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

	if (listen(listening_sock, 2) < 0) {
		printf("problem while listening for an incoming call : %s\n",strerror(errno));
		close(listening_sock);
		return EXIT_FAILURE;
	}

	size = sizeof(struct sockaddr_in);


	struct timeval time;
	int ret_val=0;
	int player_turn =0; // player 0 or 1
	int clients_connected=0;
	void msgs_to_send[NUM_CLIENTS][MSG_NUM]; //void because it can be any kind of msg.
	int msgs_index[NUM_CLIENTS];
	int first_not_sent_1=0;
	int first_not_sent_2=0;
	memset(msgs_index,0,NUM_CLIENTS);

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

			//if valid socket descriptor, add to read set.
			if (fd>0){
				FD_SET(fd,&read_fds);
				FD_SET(fd,&write_fds);
			}
			if (fd>max_fd){
				max_fd=fd;			}
		}

		ret_val= select(max_fd+1,&read_fds,NULL,NULL,NULL);
		if (ret_val<0){
			printf("Server:failed using select function: %s\n",strerror(errno));
			break;
		}
		//listening socket is read-ready - can accept
		if (FD_ISSSET(listening_sock,&read_fds)){
			new_sock = accept(listening_sock, (struct sockaddr*)&client_adrr, &size);
			if (new_sock<0){
				printf("problem while trying to accept incoming call : %s\n",strerror(errno));
				continue;
			}
			init_s_msg.client_num= ++clients_connected;
			FD_SET(new_sock,&write_fds);
			max_fd = (new_sock>max_fd)? new_sock : max_fd;

			//finding the files that are send-ready.
			ret_val=select(max_fd+1,NULL,&write_fds,NULL,NULL);
			if (ret_val< 0) {
				printf("Server:failed using select function: %s\n",strerror(errno));
				break;
			}
			//try to send the initial message to the client.
			if (FD_ISSET(new_sock,&write_fds)){
				if ((ret_val=send(new_sock,&init_s_msg,sizeof(init_s_msg)))<sizeof(init_s_msg)){
					msgs_to_send[player_turn][msgs_index[player_turn]++]=init_s_msg+ret_val;
				}
				else {
					if (player_turn==0){
						first_not_sent_1++;
					}
					else { first_not_sent_2++;}
				}
			}
			else {
				msgs_to_send[player_turn][msgs_index[player_turn]++]=init_s_msg;
			}
		}

		//its an IO operation. all sockets are connected.
		else {


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
	}
	close(listening_sock);
	close(new_sock);
	return (ret_val == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

