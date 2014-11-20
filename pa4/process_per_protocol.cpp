#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <thread>
#include "threadpool.h"
#include "message.h"
#include <semaphore.h>

#define BUFSIZE 4096
#define DEFAULT_NUM_THREADS 30
#define IN_SOCKET_TYPE 2
#define OUT_SOCKET_TYPE 1
#define HEADER_LEN 40
#define NUM_PROTOCOL_PIPES 16
using namespace std;


/* Pipearray designations: 
	0,1 (ethernet recieve pipe read,write)
	2,3 (ethernet send pipe read, write)
	4,5 (ip receive pipe read, write)
	6,7 (ip send pipe read, write)
	*/

struct header {
	int hlp;
	vector<char> other_info;
	int message_len;
};

void receive_pipe_read(int receive_pipe_read_end, int *pipearray) {
	char *buffer = new char[BUFSIZE];
	char *write_buffer = new char[BUFSIZE];
	bzero(buffer, BUFSIZE);
	if(read(receive_pipe_read_end, buffer, BUFSIZE) == -1) {
		fprintf(stderr, "error reading from ethernet receive pipe: %s\n", strerror(errno));
	}
	Message *m = new Message(buffer, BUFSIZE);
	char *stripped_header = m->msgStripHdr(HEADER_LEN);
	header *temp_header = new header;
	memcpy(temp_header, stripped_header, sizeof(HEADER_LEN));
	cout << "Temp_header->hlp: " << temp_header->hlp << endl;
	m->msgFlat(write_buffer);
	cout << "Message after strip: " << write_buffer << endl;
	receive_pipe_write(pipearray, hlp);
}

void receive_pipe_write(int *pipearray, int hlp) {
	switch (hlp) {
		case 2: //write to IPs receive pipe
			if(write(receive_pipe_write_end, write_buffer, BUFSIZE) == -1) {
				fprintf(stderr, "error writing to ip receive pipe: %s\n", strerror(errno));
			}
		case 3: //write to TCP's receive pipe
		case 4: //write to UDP's receive pipe
		case 5: //write to FTP's receive pipe
		case 6: //write to telent's receive pipe
		case 7: //write to RDP's receive pipe
		case 8: //write to DNS's receive pip
			
			
}

char *append_header(int higher_protocol_id, int other_info, char *buffer, int bytes_read) {
	char *send_buffer = new char[BUFSIZE];
	header *ethernet_header = new header;
	bzero(ethernet_header, sizeof(header));
	ethernet_header->hlp = higher_protocol_id;
	ethernet_header->other_info.resize(other_info);
	ethernet_header->message_len = bytes_read;
	Message *m = new Message(buffer, bytes_read);
	ethernet_header->message_len = bytes_read;
	char struct_buffer[sizeof(*ethernet_header)];
	memcpy(struct_buffer, ethernet_header, sizeof(*ethernet_header));
	m->msgAddHdr(struct_buffer, sizeof(struct_buffer));
	m->msgFlat(send_buffer);
	return send_buffer;
}

void send_pipe(int send_pipe_write_end, int send_pipe_read_end, int other_info) { 
	int bytes_read, higher_protocol_id;
	char *read_buffer = new char[BUFSIZE];
	char *read_buffer_minus_id = new char[BUFSIZE];
	char *send_buffer;
	bzero(read_buffer, BUFSIZE);
	bytes_read = read(send_pipe_read_end, read_buffer, BUFSIZE);
	if(bytes_read == -1) {
		fprintf(stderr, "error reading from ip send pipe: %s\n", strerror(errno));
		exit(1);
	}
	higher_protocol_id =  (int)atol(&read_buffer[0]);
	memcpy(read_buffer_minus_id, read_buffer + 1, BUFSIZE-1);
	send_buffer = append_header(higher_protocol_id, other_info, read_buffer_minus_id, bytes_read-1);
	if(write(send_pipe_write_end, send_buffer, BUFSIZE) == -1) {
		fprintf(stderr, "error writing to ethernet send pipe: %s\n", strerror(errno));
	}
}

int *create_protocol_pipes() {
	int i;
	int j = 0;
	int *pipearray = new int[32];
	for(i = 0; i < 16; i++) {
		int *pipefd = new int[2];
		if(pipe(pipefd) == -1) {
			fprintf(stderr, "error creating pipe: %s\n", strerror(errno));
		}
		pipearray[j] = pipefd[0];
		j++;
		pipearray[j] = pipefd[1];
		j++;
	}
	return pipearray;
}
	
int create_udp_socket(int socket_type) {
	struct sockaddr_in clientaddr;
	int s, errno;
	memset(&clientaddr, 0, sizeof(clientaddr));
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	clientaddr.sin_port = htons(0);
	if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		fprintf(stderr, "cannot creat UDP socket: %s\n", strerror(errno));
	}
	if(bind(s, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
		fprintf(stderr, "can't bind to port: %s\n", strerror(errno));
		exit(1);
	}
	else {
		socklen_t socklen = sizeof(clientaddr);
		if((getsockname(s, (struct sockaddr *)&clientaddr, &socklen)) < 0) {
			fprintf(stderr, "getsockname error: %s\n", strerror(errno));
		}
		if(socket_type == 1) {
			printf("New OUT udp socket port number is %d\n", ntohs(clientaddr.sin_port));
			return s;
		}
		else {
			printf("New IN udp socket port number is %d\n", ntohs(clientaddr.sin_port));
			return s;
		}
	}
}

void socket_write(int port_number, int s, int ethernet_send_pipe_read_end) {
	char buffer[BUFSIZE];
	//while(1) {
		bzero(buffer, BUFSIZE);
		if(read(ethernet_send_pipe_read_end, buffer, BUFSIZE) == -1) {
			fprintf(stderr, "error reading from send_pipe pipe: %s\n", strerror(errno));
		}
		int sendto_error;
		struct sockaddr_in servaddr;
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons(port_number);
		sendto_error = sendto(s, buffer, BUFSIZE, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
		if(sendto_error == -1) {
			fprintf(stderr, "error sending udp message: %s\n", strerror(errno));
		}
	//}
}

void socket_read(int s, int ethernet_receive_pipe_write_end) {
	char *buf = new char[BUFSIZE]; 
	int recvfrom_error;
	//while(1) {
		bzero(buf, BUFSIZE);
		recvfrom_error = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
		if(recvfrom_error == -1) {
			fprintf(stderr, "error receiving udp message: %s\n", strerror(errno));
		}
		if(write(ethernet_receive_pipe_write_end, buf, BUFSIZE) == -1) {
			fprintf(stderr, "error writing to ethernet receive pipe: %s\n", strerror(errno));
		}
	//}
}

void pipe_sendreceive_test() {
	/*
	int *pipearray = create_protocol_pipes();
	write(pipearray[1], "3", 1);
	write(pipearray[1], "hi!", 3);
	send_pipe(pipearray[5], pipearray[0], 12);
	receive_pipe(pipearray[4], pipearray[3]);
	char buffer1[43];
	char test_buffer[43];
	bzero(buffer1, 43);
	read(pipearray[2], buffer1, 3);
	Message *m = new Message(buffer1, 3);
	cout << m->msgLen() << endl;
	m->msgFlat(test_buffer);
	test_buffer[m->msgLen()] = '\0'; 
	cout << test_buffer << endl;
	*/
}

void socket_readwrite_test() {
	/*
	int *ethernet_send_pipe = create_pipe();
	int *ethernet_receive_pipe = create_pipe();
	char *buf = new char[BUFSIZE];
	strncpy(buf, "This is a test!", 15);
	write(ethernet_send_pipe[1], buf, 15); 
	int in_udp_socket, out_udp_socket, serv_in_udp_port;
	out_udp_socket = create_udp_socket(OUT_SOCKET_TYPE);
	in_udp_socket = create_udp_socket(IN_SOCKET_TYPE);
	cout << "Enter the port number of the server in udp socket: ";
	cin >> serv_in_udp_port;
	thread write_socket (socket_write, serv_in_udp_port, out_udp_socket, ethernet_send_pipe[0]); 
	thread read_socket (socket_read, in_udp_socket, ethernet_receive_pipe[1]);
	bzero(buf, 15);
	read(ethernet_receive_pipe[0], buf, 15);
	cout << buf << endl;
	cout << "Expected: This is a test!" << endl;
	write_socket.join();
	read_socket.join();
	*/
}

int main() {
	//socket_readwrite_test();
	pipe_sendreceive_test();
	//int *pipearray = create_protocol_pipes();
	return 0;
}

