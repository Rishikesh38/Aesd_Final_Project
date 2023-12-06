/**
 * @file server_sock.c
 * @brief Server-side socket program for serving image data to clients.
 *
 * This program creates a TCP server, initializes a camera to capture images,
 * and sends the image data to connected clients. It includes signal handling
 * for graceful exit on signals like SIGINT and SIGTERM.
 * Reference : https://beej.us/guide/bgnet/html/#what-is-a-socket and Prof Lectures/notes on sockets
 *
 * @author Rishikesh Goud Sundaragiri
 * @date 5th Dec 2023
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <getopt.h>
#include <linux/fs.h>
#include <pthread.h>
#include "camera_drivers.h"

#define SUCCESS_FLAG 0
#define SIGINT_FAIL 1
#define SIGTERM_FAIL 2
#define SOCKET_API_FAIL 3
#define ADDR_API_FAIL 4
#define SET_SOCK_API_FAIL 5
#define BIND_API_FAIL 6
#define LISTEN_API_FAIL 7
#define ACCEPT_API_FAIL 8


int server_sock_fd;
int client_connection_fd;
struct addrinfo hints;
struct addrinfo *server_info;
struct sockaddr_in client_addr;

void camera_init()
{

    printf("Camera init done\n");
    open_device();
    init_device();
    start_capturing();
}

void camera_off()
{
        printf("Camera switched off\n");
        stop_capturing();
        uninit_device();
        close_device();
}


void signal_handler(int sig)
{
	if(sig==SIGINT)
	{
		syslog(LOG_INFO,"Caught SIGINT, leaving");
	}
	else if(sig==SIGTERM)
	{
		syslog(LOG_INFO,"Caught SIGTERM, leaving");
	}
	
	/* Close socket and client connection */
	close(server_sock_fd);
	close(client_connection_fd);
	syslog(LOG_ERR,"Closed connection with %s",inet_ntoa(client_addr.sin_addr));
	printf("Closed connection with %s\n",inet_ntoa(client_addr.sin_addr));
	/* Exit success */
	exit(SUCCESS_FLAG); 
}

int main()
{
    int num = 1;
    int get_addr, sockopt_status, bind_status, listen_status;
    socklen_t size = sizeof(struct sockaddr);

    /* setup the logging */
    openlog(NULL,LOG_PID, LOG_USER);
    /* initialise the camera */
    camera_init();

    /* initialise the signal handler */
	if(SIG_ERR == signal(SIGINT,signal_handler))
	{
		syslog(LOG_ERR,"SIGINT failed");
		exit(SIGINT_FAIL);
	}
	if(SIG_ERR == signal(SIGTERM,signal_handler))
	{
		syslog(LOG_ERR,"SIGTERM failed");
		exit(SIGTERM_FAIL);
	}

    /* start server socket code */
    server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == server_sock_fd)
    {
		syslog(LOG_ERR, "Failed to create server socket");
		exit(SOCKET_API_FAIL);        
    }

    hints.ai_flags=AI_PASSIVE;

    get_addr = getaddrinfo(NULL,"9000",&hints,&server_info);
    if(get_addr != 0)
    {
		syslog(LOG_ERR, "Failed to get the address from getaddrinfo");
		exit(ADDR_API_FAIL);        
    }
    sockopt_status = setsockopt(server_sock_fd,SOL_SOCKET,SO_REUSEADDR,&num,sizeof(num));
    if(-1 == sockopt_status)
    {
		syslog(LOG_ERR, "Failed the setsockopt function call");
		exit(SET_SOCK_API_FAIL);        
    }

    bind_status = bind(server_sock_fd,server_info->ai_addr,sizeof(struct sockaddr));
    if(-1 == bind_status)
	{
		freeaddrinfo(server_info); 
		syslog(LOG_ERR, "Failed the bind function call");
		exit(BIND_API_FAIL);
	}

    freeaddrinfo(server_info); 

    listen_status=listen(server_sock_fd,1); 
	if(-1 == listen_status)
	{
		syslog(LOG_ERR, "Failed the listen function call");
		exit(LISTEN_API_FAIL);
	}
accept:
	printf("About to accept\n");
    client_connection_fd = accept(server_sock_fd,(struct sockaddr *)&client_addr,&size);
	printf("accept ran\n");
	if(-1 == client_connection_fd)
	{
		syslog(LOG_ERR, "Failed to accept the connection");
		exit(ACCEPT_API_FAIL);
	}
    else
	{
		syslog(LOG_INFO,"Accepts connection from %s",inet_ntoa(client_addr.sin_addr));
		printf("Accepts connection from %s\n",inet_ntoa(client_addr.sin_addr));
	}

    while(1)
    {
		int bytes_sent;
        unsigned char *temp_frame;
		temp_frame = return_pic_buffer();
		bytes_sent = send(client_connection_fd,temp_frame,((614400*6)/4),0);
		if(-1 == bytes_sent)
		{
			printf("Goto accepting a new connection \n");
			goto accept;
		}
    }

}