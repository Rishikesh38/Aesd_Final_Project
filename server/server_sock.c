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

//Modify this 
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
	
	//Close socket and client connection
	close(socket_fd);
	close(accept_return);
	syslog(LOG_ERR,"Closed connection with %s",inet_ntoa(client_addr.sin_addr));
	printf("Closed connection with %s\n",inet_ntoa(client_addr.sin_addr));
	
	exit(0); //Exit success 
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
	if(signal(SIGINT,signal_handler)==SIG_ERR)
	{
		syslog(LOG_ERR,"SIGINT failed");
		exit(2);
	}
	if(signal(SIGTERM,signal_handler)==SIG_ERR)
	{
		syslog(LOG_ERR,"SIGTERM failed");
		exit(3);
	}

    /* start server socket code */
    server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == server_sock_fd)
    {
		syslog(LOG_ERR, "Failed to create server socket");
		exit(4);        
    }

    hints.ai_flags=AI_PASSIVE;

    get_addr = getaddrinfo(NULL,"9000",&hints,&server_info);
    if(get_addr != 0)
    {
		syslog(LOG_ERR, "Failed to get the address from getaddrinfo");
		exit(5);        
    }
    sockopt_status = setsockopt(server_sock_fd,SOL_SOCKET,SO_REUSEADDR,&num,sizeof(num));
    if(-1 == sockopt_status)
    {
		syslog(LOG_ERR, "Failed the setsockopt function call");
		exit(6);        
    }

    bind_status = bind(server_sock_fd,server_info->ai_addr,sizeof(struct sockaddr));
    if(-1 == bind_status)
	{
		freeaddrinfo(server_info); 
		syslog(LOG_ERR, "Failed the bind function call");
		exit(7);
	}

    freeaddrinfo(server_info); 

    listen_status=listen(server_sock_fd,1); 
	if(-1 = listen_status)
	{
		syslog(LOG_ERR, "Failed the listen function call");
		exit(8);
	}

    client_connection_fd = accept(server_sock_fd,(struct sockaddr *)&client_addr,&size);
	if(-1 == client_connection_fd)
	{
		syslog(LOG_ERR, "Failed to accept the connection");
		exit(9);
	}
    else
	{
		syslog(LOG_INFO,"Accepts connection from %s",inet_ntoa(client_addr.sin_addr));
		printf("Accepts connection from %s\n",inet_ntoa(client_addr.sin_addr));
	}

    while(1)
    {
        
    }

}