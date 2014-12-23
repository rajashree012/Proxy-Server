// 1. All the urls can start with http:// or not. If the url doesnt contain / or no path after / then name of the file is index.html. Anything other error is thrown. If no / then path = /
// 2. names stored on the client side will have prefix client

#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h> 
#include <sys/stat.h>

int main(int argc, char *argv[])
{
	int sockfd;
	int portno;
	struct sockaddr_in proxyserv_addr;
	struct hostent *proxyserver;
	struct in_addr ipv4addr;
	int nbytes = 0;

	// message_received from the server
	char *message_received = (char *)malloc(4096*sizeof(char));
	if (message_received == NULL)
	{
		fprintf(stderr,"failed to allocate memory\n");
		exit(0);
	}
	
	// checking the arguments from the user
	if (argc < 4) 
	{
		fprintf(stderr,"usage %s proxyserver_ip proxyserver_port url\n", argv[0]);
		exit(0);
	}
	
	portno = atoi(argv[2]);

	// Create a socket point 
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) 
	{
		perror("ERROR opening socket");
		exit(1);
	}
	
	inet_pton(AF_INET,argv[1],&ipv4addr);
	proxyserver = gethostbyaddr(&ipv4addr,sizeof ipv4addr,AF_INET);
		
	if (proxyserver == NULL) 
	{
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}

	// initializing the values of server
	bzero((char *) &proxyserv_addr, sizeof(proxyserv_addr));
	proxyserv_addr.sin_family = AF_INET;
	bcopy((char *)proxyserver->h_addr,(char *)&proxyserv_addr.sin_addr.s_addr,proxyserver->h_length);
	proxyserv_addr.sin_port = htons(portno);

	// Now connect to the server 
	if (connect(sockfd,(struct sockaddr *)&proxyserv_addr,sizeof(proxyserv_addr)) < 0) 
	{
		perror("ERROR connecting");
		exit(1);
	}

	char *nameoffile = NULL;	
	
	// in case the url starts with http://
	char *initial_position = strchr(argv[3],':');
	char *position = NULL;

	// trying to find path or '/' character in the entered url
	char *host; char *path;
	if (initial_position != NULL)
	{
		initial_position += 3;
		position = strchr(initial_position,'/');
	}
	else
	{
		initial_position = argv[3];
		position = strchr(argv[3],'/');	
	}	

	// taking care of 2 cases in case there is a '/' character in the url (if there is filename  after '/' or if there is no filename after '/' : filename clientindex.html)
	if(position != NULL)
	{
		host = (char *)malloc(abs((void *)initial_position-(void *)position));	
		if (host == NULL)
		{
			fprintf(stderr,"failed to allocate memory\n");
			exit(0);
		}
		memcpy(host,initial_position,abs((void*)initial_position-(void*)position));
		*(host + abs((void*)initial_position-(void*)position)) = '\0';
		path = (char *)malloc(strlen(initial_position)-strlen(host)+1);
		if (path == NULL)
		{
			fprintf(stderr,"failed to allocate memory\n");
			exit(0);
		}
		memcpy(path,(initial_position+strlen(host)),strlen(initial_position)-strlen(host));
		*(path+strlen(initial_position)-strlen(host)) = '\0';
		printf(" %s -- %s ",host,path);fflush(stdout);

		// getting the filename
		char *position2 = strrchr(path,'/');
		if (strlen(position2) <= 1)
		{
			nameoffile = (char *)malloc(10);
			if (nameoffile == NULL)
			{
				fprintf(stderr,"failed to allocate memory\n");
				exit(0);
			}
			nameoffile = "index.html";
		}
		else
		{
			nameoffile = (char *)malloc(strlen(position2)-1+1);
			if (nameoffile == NULL)
			{
				fprintf(stderr,"failed to allocate memory\n");
				exit(0);
			}
			memcpy(nameoffile,position2+1,strlen(position2)-1);
			*(nameoffile + strlen(position2)-1) = '\0';
		}
		printf("name of file %s ---\n",nameoffile);fflush(stdout);
	}
	// in case there is no '/' then the filename is clientindex.html
	else 
	{
		host = initial_position;
		path = "/";
		printf(" %s-- %s ",host,path);fflush(stdout);
		nameoffile = (char *)malloc(10);
		if (nameoffile == NULL)
		{
			fprintf(stderr,"failed to allocate memory\n");
			exit(0);
		}
		nameoffile = "index.html";
		printf("name of file %s ---\n",nameoffile);fflush(stdout);
	}

	// send http/1.0 request to the proxy server  GET = 3 + space = 1 + path = strlen(path) + space = 1 + HTTP/1.0 = 8  + \r\n = 2 + HOST: = 6(including space) + host = strlen(host) + \r\n = 2 + \r\n = 2
	char *message_sent = (char *)malloc(3+1+strlen(path)+1+8+2+6+strlen(host)+2+2+1);
	if (message_sent == NULL)
	{
		fprintf(stderr,"failed to allocate memory\n");
		exit(0);
	}
	char *temp = message_sent;
	// adding GET
	memcpy(message_sent,"GET",3);
	message_sent+=3;
	// adding space
	memcpy(message_sent," ",1);
	message_sent+=1;
	// adding path
	memcpy(message_sent,path,strlen(path));
	message_sent+=strlen(path);
	// adding space
	memcpy(message_sent," ",1);
	message_sent+=1;
	// adding HTTP/1.0
	memcpy(message_sent,"HTTP/1.0",8);
	message_sent+=8;
	// adding CR
	memcpy(message_sent,"\r",1);
	message_sent+=1;
	// adding LF
	memcpy(message_sent,"\n",1);
	message_sent+=1;
	// adding HOST: 
	memcpy(message_sent,"HOST: ",6);
	message_sent+=6;
	// adding host
	memcpy(message_sent,host,strlen(host));
	message_sent+=(strlen(host));
	// adding CR
	memcpy(message_sent,"\r",1);
	message_sent+=1;
	// adding LF
	memcpy(message_sent,"\n",1);
	message_sent+=1;
	// adding CR
	memcpy(message_sent,"\r",1);
	message_sent+=1;
	// adding LF
	memcpy(message_sent,"\n",1);
	message_sent+=1;
	*message_sent = '\0';
			
	if (send(sockfd, temp,3+1+strlen(path)+1+8+2+6+strlen(host)+2+2, 0) == -1) 
	{
		perror("send");
		exit(1);
	}
	printf ("message sent to the server : %s\n",temp);

	// receiving the file from server
	fd_set read_fds;
	FD_ZERO(&read_fds);
	FD_SET(sockfd,&read_fds);

	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
	int fd_file = open (nameoffile, O_WRONLY| O_CREAT |O_APPEND| O_TRUNC, mode); 

	int flag = 0;

	// receiving message in chunks of size 4096
	while (1)
	{
		bzero(message_received,4096);printf ("***%d\n",nbytes);fflush(stdout);
		if ((nbytes = recv(sockfd, message_received, 4096, 0)) <= 0) 
		{
			// got error or connection closed by client
			if (nbytes == 0) 
			{
				// connection closed
				printf("selectserver: socket %d hung up\n", sockfd);
			} 
			else 
			{
				perror("recv");
			}
			exit(1);
		} 
		else
		{
			// in case the url is not correct
			if(strcmp(message_received,"404")==0)
			{
				printf("404 error message, host not found");fflush(stdout);
				exit(1);
			}
			else
			{
				// removing the header part from the file
				char* end_of_header = strstr(message_received,"\r\n\r\n");
				if (end_of_header != NULL && flag == 0)
					{
						int x = (end_of_header - message_received);
						write(fd_file,end_of_header+4,nbytes-x-4);fflush(stdout);
						flag = 1;
					}	
				else
					{write(fd_file,message_received,nbytes);fflush(stdout);}
			}
		}
	}
	close(fd_file);	
	return 0;
}

