/**
 * @file client_sock.c
 * @brief Client-side socket program for receiving and dumping images.
 *
 * This program establishes a TCP connection with a server, receives image data
 * on the socket, and dumps the images to PPM files. It includes a signal handler
 * to gracefully exit on signals like SIGINT and SIGTERM.
 * Reference : https://beej.us/guide/bgnet/html/#what-is-a-socket and Prof Lectures/notes on sockets
 *
 * @author Rishikesh Goud Sundaragiri
 * @date 5th Dec 2023
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <syslog.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

#define SUCCESS_FLAG 0
#define SIGINT_FAIL 1
#define SIGTERM_FAIL 2
#define SOCKET_API_FAIL 3
#define INET_API_FAIL 4
#define CONNECT_API_FAIL 5
#define RECEIVE_ERROR 6
#define PORT 9000
#define HRES_STR "640"
#define VRES_STR "480"
#define STARTUP_FRAMES 20
int client_fd;
static int current_frame = 0;

void signal_handler(int sig)
{
	if(sig==SIGINT)
		syslog(LOG_INFO,"Caught SIGINT, leaving now");

	else if(sig==SIGTERM)
		syslog(LOG_INFO,"Caught SIGTERM, leaving now");

	else if (sig==SIGTSTP)
		syslog(LOG_INFO,"Caught SIGTSTP, leaving now");

	/* Close socket connection */
	close(client_fd);

	syslog(LOG_ERR,"Closed connection");
	printf("Closed connection\n");
	/* Exit success */
	exit(SUCCESS_FLAG);  
}

void dump_ppm(const unsigned char *p, int size, int frame_number)
{
    int written, total, dumpfd;
    char ppm_header[100]; 
    char ppm_dumpname[30]; 

    snprintf(ppm_dumpname, sizeof(ppm_dumpname), "frames/frame%d.ppm", frame_number);
    dumpfd = open(ppm_dumpname, O_WRONLY | O_NONBLOCK | O_CREAT, 00666);

    /* PPM header construction */ 
    snprintf(ppm_header, sizeof(ppm_header), "P6\n#Frame %d\n%s %s\n255\n", frame_number, HRES_STR, VRES_STR);

    /* Write header to file */
    written = write(dumpfd, ppm_header, strlen(ppm_header));

    total = 0;
    /* Write frame data to file */
    do
    {
        written = write(dumpfd, p, size);
        total += written;
    } while (total < size);
    close(dumpfd);
}

int main(int argc, char const* argv[])
{
    printf("Entered main\n");
    struct sockaddr_in my_addr;
    int status;
    int num_frame = 1;
    int requested_frames = 0;
    requested_frames = atoi(argv[2]);
    openlog(NULL,LOG_PID, LOG_USER);
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

    if((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		syslog(LOG_ERR,"Socket creation error");
		exit(SOCKET_API_FAIL);
	}

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);

    /* Convert IPv4 and IPv6 addresses from text to binary */
	if (inet_pton(AF_INET, argv[1], &my_addr.sin_addr)<= 0) 
	{
		syslog(LOG_ERR,"Invalid address: Address not supported");
        printf("Invalid address: Address not supported");
		exit(INET_API_FAIL);
	}
    printf("inet_pton done\n");
	if ((status=connect(client_fd, (struct sockaddr*)&my_addr,sizeof(my_addr)))< 0) 
	{
		syslog(LOG_ERR,"Connection Failed");
        printf("Connection Failed\n");
		exit(CONNECT_API_FAIL);
	}
    printf("connected\n");
    printf("%d is the requested frames\n",requested_frames);
    while (num_frame  <= requested_frames)
    {
        unsigned char buffer[((614400 * 6) / 4)];
        int bytes_received;
        int total_bytes_received = 0;

        while (total_bytes_received < sizeof(buffer))
        {
            bytes_received = recv(client_fd, buffer + total_bytes_received, sizeof(buffer) - total_bytes_received, 0);

            if (bytes_received < 0)
            {
                syslog(LOG_ERR, "Receive error");
                exit(RECEIVE_ERROR);
            }

            total_bytes_received += bytes_received;
            current_frame++;
        }

        // Now 'buffer' contains the entire image data
        if(current_frame > STARTUP_FRAMES)
        {
            dump_ppm(buffer, ((614400 * 6) / 4), num_frame);
            num_frame++;
        }
    }

}