// change cache size to 10
// assumes all the file names in the cache are unique

#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h> 
#include <sys/stat.h>
#include <time.h>

#define CACHESIZE 10

int convertmonth(char *month)            
{
	if (strcmp("Jan",month) == 0)
		return 0;
	else if (strcmp("Feb",month) == 0)
		return 1;
	else if (strcmp("Mar",month) == 0)
		return 2;
	else if (strcmp("Apr",month) == 0)
		return 3;
	else if (strcmp("May",month) == 0)
		return 4;
	else if (strcmp("Jun",month) == 0)
		return 5;
	else if (strcmp("July",month) == 0)
		return 6;
	else if (strcmp("Aug",month) == 0)
		return 7;
	else if (strcmp("Sep",month) == 0)
		return 8;
	else if (strcmp("Oct",month) == 0)
		return 9;
	else if (strcmp("Nov",month) == 0)
		return 10;
	else if (strcmp("Dec",month) == 0)
		return 11;
};

struct clientsocket
{
	int socketfd;
	char *filename;  // filename from which it is currently reading
	int filedescriptor; // descriptor of that file
	time_t start;  // time at which it starts reading data from the file
	struct clientsocket *next;
};

struct webserversocket
{
	int websocketfd;
	char *filename;  // name of the file into which it is writing currently
	int fileposition; // position of this file in cache
	int filedescriptor;
};

struct fileinfo
{
	char *filename;
	char *url; 
	time_t start; // time when this file is put in the cache for LRU algorithm
	time_t expirytime; // the time at which file will expire in the cache
	char *expirytimewordformat; // text format of the expiry date
};

// adding node and head cannot be NULL
void insert(struct clientsocket *head, struct clientsocket *node)
{
	if (head->next == NULL)
	{
		head->next = node;
		return;
	}
	else
		insert(head->next,node);
}

// return position where there is empty
int insertweb(struct webserversocket *head)
{
	int i = 0;
	for(i = 0; i<CACHESIZE ;i++)
	{
		if(head->websocketfd == -1)
			return i;
		head+=1;
	}
	return -999; // returned when the cache is full
}

// insert file in cache : see null or anything expired
int insertfile(struct fileinfo *head)
{
	time_t end;
	time(&end);
	double seconds = 0;
	int i = 0;
	int mini = 0;
	for (i = 0;i<CACHESIZE;i++)
	{
		if (head->url == NULL)
			return i;
		else 
		{
			if (seconds < difftime(end,head->start))
			{
				seconds = difftime(end,head->start);
				mini = i;
			}
		}
		head+=1;
	}
	return mini; // if there is no empty space in the cache sending the least recently used one
}

// finding client based on socketfd. It returns the node before it, except in case of head
struct clientsocket* find(int socketfd, struct clientsocket *head)  
{
	if (socketfd == head->socketfd)
		return head;
	else if (head->next == NULL)
		return NULL;
	else if (socketfd == head->next->socketfd)
		return head;
	else 
		return(find(socketfd,head->next));
}

// finding client struct by the file descriptor
struct clientsocket* findbyfd(int filefd, struct clientsocket *head)  
{
	if (filefd == head->filedescriptor)
		return head;
	else if (head->next == NULL)
		return NULL;
	else if (filefd == head->next->filedescriptor)
		return head;
	else 
		return(find(filefd,head->next));
}

// return the position in the list of web server sockets
int findweb(struct webserversocket *head,int websocketfd)
{
	int i = 0;
	for(i = 0; i<CACHESIZE ;i++)
	{
		if(head->websocketfd == websocketfd)
			return i;
		head+=1;
	}
	return -999; // if not found
}

//finding file by url in the cache
int findfile(struct fileinfo *head,char *url)
{
	int i = 0;
	for(i = 0; i<CACHESIZE ;i++)
	{
		if(head->url!=NULL && strcmp(head->url,url)==0)
			return i;
		head+=1;
	}
	return -999; // if not found
}

// deleting node : giving the previous node and current node as input
struct clientsocket* delete(struct clientsocket *head, struct clientsocket *deletenodeprevious, struct clientsocket *deletenodecurrent)
{
	if (deletenodeprevious == head && deletenodecurrent == head)
		return head->next;
	else
	{
		deletenodeprevious->next = deletenodecurrent->next;
		return head;
	}
}

// after receiving the data from the web server, info is deleted
int deleteweb(struct webserversocket *head,int websocketfd)
{
	int i = 0;
	for(i = 0; i<CACHESIZE ;i++)
	{
		if(head->websocketfd == websocketfd)
		{
			head->websocketfd = -1;
			head->filename = NULL;
			head->filedescriptor = -1;
			head->fileposition = -1;
			return 0; // successfully found and deleted
		}
		head+=1;
	}
	return -999; // if not found
}

int main( int argc, char *argv[] )
{
	// finding difference between current machine time and gmt 
	time_t currtime;
	struct tm *gmt_time;
	time(&currtime);
	gmt_time = gmtime(&currtime);
	gmt_time->tm_isdst = 1;
	time_t cur = mktime(gmt_time);
	time_t diff = cur - currtime; // as texas time is behind gmt
	printf("time difference in seconds = %d current time = %s\n",diff,ctime(&cur));fflush(stdout);

	int sockfd, newsockfd, portno, clilen;
	struct sockaddr_in proxyserv_addr, cli_addr;

	// sending or receiving data is done in chunks of 4096
	char *message_received = (char *)malloc(sizeof(char)*4096);
	if (message_received == NULL)
	{
		fprintf(stderr,"failed to allocate memory\n");
		exit(0);
	}
	char *message_sent = (char *)malloc(sizeof(char)*4096);
	if (message_sent == NULL)
	{
		fprintf(stderr,"failed to allocate memory\n");
		exit(0);
	}
	
	// checking number of arguments
	if (argc < 3) 
	{
		fprintf(stderr,"usage %s server_ip server_port\n", argv[0]);
		exit(0);
	}

	// First call to socket() function 
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
	{
		perror("ERROR opening socket");
		exit(1);
	}

	// Initialize socket structure 
	bzero((char *) &proxyserv_addr, sizeof(proxyserv_addr));
	portno = atoi(argv[2]);
	proxyserv_addr.sin_family = AF_INET;
	proxyserv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	proxyserv_addr.sin_port = htons(portno);

	// Now bind the host address using bind() call.  
	if (bind(sockfd, (struct sockaddr *) &proxyserv_addr,
		          sizeof(proxyserv_addr)) < 0)
	{
		 perror("ERROR on binding");
		 exit(1);
	}

	// Now start listening for the clients, here process will
	// go in sleep mode and will wait for the incoming connection
	
	listen(sockfd,atoi(argv[2]));
	clilen = sizeof(cli_addr);

	fd_set master;
	fd_set read_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	int fdmax = 0, nbytes; //keeping track of maximum file descriptor
	FD_SET(sockfd,&master);
	fdmax = sockfd;
	int i,j,k;

	struct clientsocket *headclients = NULL;

	// initializing CACHESIZE web server requests by the the proxy servers
	struct webserversocket *headwebservers = (struct webserversocket *)malloc(CACHESIZE*sizeof(struct webserversocket));
	if (headwebservers == NULL)
	{
		fprintf(stderr,"failed to allocate memory\n");
		exit(0);
	}
	struct webserversocket *tempweb = headwebservers;
	for(j = 0; j < CACHESIZE; j++)
	{
		tempweb->websocketfd = -1;
		tempweb->filename = NULL;
		tempweb->filedescriptor = -1;
		tempweb->fileposition = -1;
		tempweb+=1;
	}

	// initializing structure to store information of cache
	struct fileinfo *headoffiles = (struct fileinfo *)malloc(CACHESIZE*sizeof(struct fileinfo));
	if (headoffiles == NULL)
	{
		fprintf(stderr,"failed to allocate memory\n");
		exit(0);
	}
	struct fileinfo *temp1 = headoffiles;
	for(j = 0; j < CACHESIZE; j++)
	{
		temp1->filename = NULL;
		temp1->url = NULL;
		temp1->start = 0;
		temp1->expirytime = 0;
		temp1->expirytimewordformat = NULL;
		temp1+=1;
	}

	for(;;)
	{
		//updating readfds
		read_fds = master;
		
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) 
		{
			perror("select");
			exit(1);
		}
		// assuming sockets are assigned with successive numbers
		for(i = sockfd; i <= fdmax; i++) 
		{
			if (FD_ISSET(i, &read_fds)) 
			{       // we got one!!
				// if its server socket
				if (i == sockfd) 
				{
					// handle new connections
					// Accept actual connection from the client 
					// sockfd will be in read state when there are any incoming connections
					newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,&clilen); 
					if (newsockfd < 0) 
					{
						perror("ERROR on accept");
						exit(1);
					}
					else 
					{
						printf("The client with socket descriptor %d has been connected\n",newsockfd);
						FD_SET(newsockfd, &master); // add to master set
						if (newsockfd > fdmax) 
						{	// keep track of the max
						        fdmax = newsockfd;
						}
						// adding the client to list of clients
						if (headclients == NULL)
						{
							headclients = (struct clientsocket*)malloc(sizeof(struct clientsocket));
							if (headclients == NULL)
							{
								fprintf(stderr,"failed to allocate memory\n");
								exit(0);
							}
							headclients->socketfd = newsockfd;
							headclients->filename = NULL;
							headclients->filedescriptor = -1;
							headclients->start = 0;
							headclients->next = NULL;
						}
						else
						{
							struct clientsocket* tempcl =  (struct clientsocket*)malloc(sizeof(struct clientsocket));
							if (tempcl == NULL)
							{
								fprintf(stderr,"failed to allocate memory\n");
								exit(0);
							}
							tempcl->socketfd = newsockfd;
							tempcl->filename = NULL;
							tempcl->filedescriptor = -1;
							tempcl->start = 0;
							tempcl->next = NULL;
							insert(headclients,tempcl);
						}
					}
				} 
				else 
				{
					// client from which we received the message
					struct clientsocket* temp = find(i,headclients);
					
					// ensuring that the message is from client and not from the web server				
					if (temp != NULL)
					{
						bzero(message_received,4096);
						// handle data from a client
						if ((nbytes = recv(i, message_received, 4096, 0)) <= 0) 
						{
							// got error or connection closed by client
							if (nbytes == 0) 
							{
								// connection closed
								printf("selectserver: socket %d hung up\n", i);
								close(i); // bye!
								FD_CLR(i, &master); // remove from master set
								FD_CLR(temp->filedescriptor, &master); // clear the fd from which it is reading
								close(temp->filedescriptor);
								// when the client has received entire file it will close the connection on its side. Deleting the client information
								struct clientsocket* temp1 = find(i,headclients);
								if (temp1!=headclients || i!=temp1->socketfd)
									headclients=delete(headclients,temp1,temp1->next);
								else
									headclients=delete(headclients,temp1,temp1);
							} 
							else 
							{
								perror("recv");
							}	
						} 
						else 
						{
							// getting the client with socket fd = i
							if (temp!=headclients || i!=temp->socketfd)
								temp = temp->next;
						
		                			printf("we got the message from the client with socket %d : %s\n",i,message_received);
							fflush(stdout);
							// from clients only GET information are received
							// getting the host name from the request by finding position of ':'
							char *position = strchr(message_received,':');
							char *host = (char *)malloc(strlen(position)-6+1);
							if (host == NULL)
							{
								fprintf(stderr,"failed to allocate memory\n");
								exit(0);
							}	
							memcpy(host,position+2,strlen(position)-6);
							*(host+strlen(position)-6)='\0';
							printf("host %s \n",host);fflush(stdout);

							// getting path path after /
							position = strchr(message_received,'/');
							char *position1 = strchr(message_received,':');
							char *path = (char *)malloc(strlen(position)-strlen(position1)-15+1);
							if (path == NULL)
							{
								fprintf(stderr,"failed to allocate memory\n");
								exit(0);
							}
							memcpy(path,position,strlen(position)-strlen(position1)-15);
							*(path+strlen(position)-strlen(position1)-15)='\0';
							printf("path %s \n",path);fflush(stdout);

							// getting url
							char *url = (char *)malloc(strlen(host)+strlen(path)+1);
							if (url == NULL)
							{
								fprintf(stderr,"failed to allocate memory\n");
								exit(0);
							}
							memcpy(url,host,strlen(host));
							memcpy(url + strlen(host),path,strlen(path));
							*(url+strlen(host)+strlen(path))='\0';
							printf("url %s ---\n",url);fflush(stdout);

							// name of file
							char *nameoffile = NULL;
							char *position2 = strrchr(path,'/');
							if (strlen(position2) <= 1)
							{
								nameoffile = (char *)malloc(15);
								if (nameoffile == NULL)
								{
									fprintf(stderr,"failed to allocate memory\n");
									exit(0);
								}
								memcpy(nameoffile,"cacheindex.html",15);
							}
							else
							{
								nameoffile = (char *)malloc(strlen(position2)-1+1+5);
								if (nameoffile == NULL)
								{
									fprintf(stderr,"failed to allocate memory\n");
									exit(0);
								}
								memcpy(nameoffile,"cache",5);
								memcpy(nameoffile+5,position2+1,strlen(position2)-1);
								*(nameoffile + 5+strlen(position2)-1) = '\0';
							}
							printf("name of file %s ---\n",nameoffile);fflush(stdout);

							// the value of current time as per GMT
							time_t current;
							time(&current);
							current = current + diff;
							printf("current time in seconds %d current time %s \n",current,ctime(&current));fflush(stdout);


							// if file is already in the cache and if the expiry date is not crossed then the same file is returned
							if ((k = findfile(headoffiles,url))!=-999 && (headoffiles+k)->expirytime > current)
							{
								// updating the contents of even the client struct
								temp->filename = (char *)malloc(strlen((headoffiles+k)->filename)+1);
								if (temp->filename == NULL)
								{
									fprintf(stderr,"failed to allocate memory\n");
									exit(0);
								}
								memcpy(temp->filename,(headoffiles+k)->filename,strlen((headoffiles+k)->filename));
								*(temp->filename+strlen((headoffiles+k)->filename))='\0';
								int fd_file1 = open(temp->filename,O_RDONLY);
								temp->filedescriptor = fd_file1;
								
								// adding filedescriptor to master
								FD_SET(fd_file1, &master);
								if (fd_file1 > fdmax) 
								{	// keep track of the max
									fdmax = fd_file1;
								}
								time(&((headoffiles+k)->start)); // resetting the time according to LRU
								printf("file is in the cache and is not stale.");fflush(stdout);
								printf("opening the file %s already in cache with file descriptor %d\n",temp->filename,temp->filedescriptor);fflush(stdout);
							}
							else
							{
								// trying to connect to server of web host
								struct sockaddr_in webserv_addr;
								struct hostent *webserver;
								int websockfd = socket(AF_INET, SOCK_STREAM, 0);

								if (websockfd < 0) 
								{
									perror("ERROR opening socket");
									exit(1);
								}

								webserver = gethostbyname(host);
								if (webserver == NULL) 
								{
									fprintf(stderr,"ERROR, no such host\n");
									exit(0);
								}

								// initializing the values of webserver
								bzero((char *) &webserv_addr, sizeof(webserv_addr));
								webserv_addr.sin_family = AF_INET;
								bcopy((char *)webserver->h_addr,(char *)&webserv_addr.sin_addr.s_addr,webserver->h_length);
								webserv_addr.sin_port = htons(80);
							
								// Now connect to the server 
								if (connect(websockfd,(struct sockaddr *)&webserv_addr,sizeof(webserv_addr)) < 0) 
								{
									perror("ERROR connecting");
									exit(1);
								}
							
								bzero(message_sent,4096);
								memcpy(message_sent,message_received,nbytes);
								
								// ------ if file in cache (modified or not modified)	change message sent : it should include expired date of file in cache 	
								int message_sent_length = 0;
								if (k!=-999)
								{
									memcpy(message_sent+nbytes-2,"If-Modified-Since: ",19);
									memcpy(message_sent+nbytes-2+19,(headoffiles+k)->expirytimewordformat,strlen((headoffiles+k)->expirytimewordformat));
									memcpy(message_sent+nbytes-2+19+strlen((headoffiles+k)->expirytimewordformat),"\r\n\r\n",4);
									message_sent_length = -2+19+strlen((headoffiles+k)->expirytimewordformat)+4;
								}

								printf("message that is sent to the web server %s \n",message_sent);fflush(stdout);

								if (send(websockfd, message_sent, nbytes+message_sent_length, 0) == -1) 
								{
									perror("send");
								}

								// make the web server socket non blocking 
								fcntl(websockfd, F_SETFL, O_NONBLOCK);

								// waiting to get first set of information from server
								fd_set temporary;
								FD_ZERO(&temporary);
								while(!FD_ISSET(websockfd, &temporary))
								{
									FD_SET(websockfd, &temporary);
									if (select(websockfd+1, &temporary, NULL, NULL, NULL) == -1) 
									{
										perror("select");
										exit(1);
									}
								}

								// receiving first block of information from webserver ; analyze it
								bzero(message_received,4096);
								if ((nbytes = recv(websockfd, message_received, 4096, 0)) <= 0) 
								{
									// got error or connection closed by client
									if (nbytes == 0) 
									{
										// connection closed
										printf("selectserver: socket %d hung up\n", i);
										close(websockfd);
									} 
									else 
									{
										perror("recv");
									}
						
								} 
								else
								{
									// analyzing the response type
									char *pos = strchr(message_received,'/');
									char responsetype[3];
									responsetype[0] = *(pos + 5);
									responsetype[1] = *(pos + 6);
									responsetype[2] = *(pos + 7);
									int response = atoi(responsetype);
									printf("response code from the server %d \n",response);

									// if the webpage does not exist
									if(response == 404)
									{
										if (send(temp->socketfd, "404",3, 0) == -1) 
										{
											perror("send");
										}
										// closing all the clients connections
										printf("Closing the connection of the client %d",temp->socketfd);fflush(stdout);
										close(temp->socketfd);
										FD_CLR(temp->socketfd, &master);
										struct clientsocket* temp2 = find(i,headclients);
										if (temp2!=headclients || i!=temp2->socketfd)
											headclients = delete(headclients,temp2,temp2->next);
										else
											headclients = delete(headclients,temp2,temp2);
									}
									else
									{
										// processing to get the expiry date
										char *positionexpiry = strstr(message_received,"Expires");
										char *endofline = strchr(positionexpiry,'\n');
										char *required = (char *)malloc(strlen(positionexpiry)-strlen(endofline));
										if (required == NULL)
										{
											fprintf(stderr,"failed to allocate memory\n");
											exit(0);
										}
										sprintf(required,"%s",positionexpiry);
										char *required1 = (char *)malloc(strlen(positionexpiry)-strlen(endofline)+1);
										if (required1 == NULL)
										{
											fprintf(stderr,"failed to allocate memory\n");
											exit(0);
										}
										char ch = *positionexpiry; int p = 0;
										while(ch!='\n' && ch!='\r')
										{
											ch = *(positionexpiry+p);
											*(required1 + p) = ch;
											p++;
										}
										*(required1+p-1) = '\0';
										char *splittings;
										splittings = strtok(required," :");
										// neglect Expires
										splittings = strtok (NULL, " :");
										// neglect day
										splittings = strtok (NULL, " :");
										// day of month
										int day = atoi(splittings);
										splittings = strtok (NULL, " :");
										// month
										int month = convertmonth(splittings);
										splittings = strtok (NULL, " :");
										// year
										int year = atoi(splittings);
										splittings = strtok (NULL, " :");
										// hours
										int hours = atoi(splittings);
										splittings = strtok (NULL, " :");	
										// minutes
										int minutes = atoi(splittings);
										splittings = strtok (NULL, " :");
										// seconds
										int seconds = atoi(splittings);
										splittings = strtok (NULL, " :");							
										printf("expiry : day %d month %d year %d hours %d minutes %d seconds %d\n",day,month,year,hours,minutes,seconds);fflush(stdout);
										// filling mk details 
										struct tm expired_time;
										expired_time.tm_year = year - 1900;
										expired_time.tm_mon = month;
										expired_time.tm_mday = day;
										expired_time.tm_hour = hours;
										expired_time.tm_min = minutes;
										expired_time.tm_sec = seconds;
										expired_time.tm_isdst = 1;

										time_t expired_format = mktime(&expired_time);
										printf("Expiry time in seconds %d\n expiry time in text %s \n",expired_format,required1);fflush(stdout);			

										// if it is in cache and not modified at the server then update the expire date send the same contents of file
										if (k!=-999 && response == 304)	
										{	
											// updating the contents of even the client struct
											temp->filename = (char *)malloc(strlen((headoffiles+k)->filename)+1);
											if (temp->filename == NULL)
											{
												fprintf(stderr,"failed to allocate memory\n");
												exit(0);
											}
											memcpy(temp->filename,(headoffiles+k)->filename,strlen((headoffiles+k)->filename));
											*(temp->filename+strlen((headoffiles+k)->filename))='\0';
											int fd_file1 = open(temp->filename,O_RDONLY);
											temp->filedescriptor = fd_file1;
								
											// adding filedescriptor to master
											FD_SET(fd_file1, &master);
											if (fd_file1 > fdmax) 
											{	// keep track of the max
												fdmax = fd_file1;
											}
											time(&((headoffiles+k)->start)); // resetting the time according to LRU
											printf("opening the file %s already in cache with file descriptor %d but not modified at the server\n",temp->filename,temp->filedescriptor );fflush(stdout);  
											(headoffiles+k)->expirytime = expired_format;
											(headoffiles+k)->expirytimewordformat = (char *)malloc(strlen(required1));
											if ((headoffiles+k)->expirytimewordformat == NULL)
											{
												fprintf(stderr,"failed to allocate memory\n");
												exit(0);
											}
											memcpy((headoffiles+k)->expirytimewordformat,required1+9,strlen(required1)-9);
											*((headoffiles+k)->expirytimewordformat+strlen(required1)-9) = '\0';
										}

										// if it is not in cache or in cache but modified
										// deletefile if in cache but modified  
										else if (response == 200)
										{
											int positionfile = 0;
											// checking whether the file is in cache or not
											if (k!=-999)
											{
												printf("Deleting the file %s since it has been modified and updating the cache \n",(headoffiles+k)->filename);fflush(stdout);
												remove((headoffiles+k)->filename);
												positionfile = k;
												time(&((headoffiles+positionfile)->start));
											}
											else
											{
												// updating the cache if the file is not in cache
												positionfile = insertfile(headoffiles);
												if ((headoffiles+positionfile)->url != NULL)
												{
													bzero((headoffiles+positionfile)->url,strlen((headoffiles+positionfile)->url));
													remove((headoffiles+positionfile)->filename); 
													bzero((headoffiles+positionfile)->filename,strlen((headoffiles+positionfile)->filename));
												}
												(headoffiles+positionfile)->url = (char *)malloc(strlen(host)+strlen(path));
												if ((headoffiles+positionfile)->url == NULL)
												{
													fprintf(stderr,"failed to allocate memory\n");
													exit(0);
												}
												memcpy((headoffiles+positionfile)->url,host,strlen(host));
												memcpy((headoffiles+positionfile)->url+strlen(host),path,strlen(path));	
												*((headoffiles+positionfile)->url + strlen(host)+strlen(path)) = '\0';
												(headoffiles+positionfile)->filename = (char *)malloc(strlen(nameoffile));
												if ((headoffiles+positionfile)->filename == NULL)
												{
													fprintf(stderr,"failed to allocate memory\n");
													exit(0);
												}
												memcpy((headoffiles+positionfile)->filename,nameoffile,strlen(nameoffile));
												*((headoffiles+positionfile)->filename + strlen(nameoffile)) = '\0';
												// resetting time for LRU
												time(&((headoffiles+positionfile)->start));
												printf("file %s is not in the cache. the file at position is being replaced %d\n",(headoffiles+positionfile)->filename,positionfile);
												fflush(stdout);	
											}
											// updating the expiry time in both the formats
											(headoffiles+positionfile)->expirytime = expired_format;
											(headoffiles+positionfile)->expirytimewordformat = (char *)malloc(strlen(required1));
											if ((headoffiles+positionfile)->expirytimewordformat == NULL)
											{
												fprintf(stderr,"failed to allocate memory\n");
												exit(0);
											}
											memcpy((headoffiles+positionfile)->expirytimewordformat,required1+9,strlen(required1)-9);
											*((headoffiles+positionfile)->expirytimewordformat+strlen(required1)-9) = '\0';
	
											printf("expiry time %d %s \n",(headoffiles+positionfile)->expirytime,(headoffiles+positionfile)->expirytimewordformat);fflush(stdout);
										

											// storing the information of web server along with the file information
											int positionweb = insertweb(headwebservers);	
											(headwebservers+positionweb)->websocketfd = websockfd;
											if ((headwebservers+positionweb)->filename!=NULL)
												bzero((headwebservers+positionweb)->filename,strlen((headwebservers+positionweb)->filename));
											(headwebservers+positionweb)->filename = (char *)malloc(strlen((headoffiles+positionfile)->filename)+1);
											if ((headwebservers+positionweb)->filename == NULL)
											{
												fprintf(stderr,"failed to allocate memory\n");
												exit(0);
											}
											memcpy((headwebservers+positionweb)->filename,(headoffiles+positionfile)->filename,strlen((headoffiles+positionfile)->filename));
											*((headwebservers+positionweb)->filename+strlen((headoffiles+positionfile)->filename))='\0';
											mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
								
											int fd_file = open ((headwebservers+positionweb)->filename, O_WRONLY| O_CREAT |O_APPEND, mode);
											(headwebservers+positionweb)->filedescriptor = fd_file;
											(headwebservers+positionweb)->fileposition = positionfile;
									
											printf("opening the file with fd for writing %d\n",fd_file);fflush(stdout);
							
											// updating the contents of even the client struct
											temp->filename = (char *)malloc(strlen((headoffiles+positionfile)->filename)+1);
											if (temp->filename == NULL)
											{
												fprintf(stderr,"failed to allocate memory\n");
												exit(0);
											}
											memcpy(temp->filename,(headoffiles+positionfile)->filename,strlen((headoffiles+positionfile)->filename));
											*(temp->filename+strlen((headoffiles+positionfile)->filename))='\0';
											int fd_file1 = open((headoffiles+positionfile)->filename,O_RDONLY);
											temp->filedescriptor = fd_file1;
											time(&(temp->start));
											printf("opening the file with fd for the client to read from %d\n",fd_file1);fflush(stdout);
	
											// adding filedescriptor and web server socket fd to master
											FD_SET(fd_file1, &master);
											FD_SET(websockfd, &master);
											if (fd_file1 > fdmax) 
											{	// keep track of the max
												fdmax = fd_file1;
											}
											if (websockfd > fdmax) 
											{	// keep track of the max
												fdmax = websockfd;
											}	
											// writing the first block of message into file
											write((headwebservers+positionweb)->filedescriptor,message_received,nbytes);fflush(stdout);
										}
									}
							
								}

							}
						}
					}
					else if((k = findweb(headwebservers,i)) != -999)
					{ // message received from the web server						
						bzero(message_received,4096);
						//printf("message received from web server %d and writing into file %d\n",i,(headwebservers+k)->filedescriptor);fflush(stdout);
						if ((nbytes = recv(i, message_received, 4096, 0)) <= 0) 
						{
							// got error or connection closed by client
							if (nbytes == 0) 
							{
								// connection closed
								printf("selectserver: socket %d hung up\n", i);
								// removing the webserver information from the list of clients
								close(i);
								FD_CLR(i, &master);
								FD_CLR((headwebservers+k)->filedescriptor, &master);
								close((headwebservers+k)->filedescriptor);
								deleteweb(headwebservers,i);
							} 
							else 
							{
								perror("recv");
							}
						
						} 
						else
						{
							write((headwebservers+k)->filedescriptor,message_received,nbytes);fflush(stdout);
						}
					}
					else
					{
						//sending the contents of file to client
						struct clientsocket* temp1 = findbyfd(i,headclients);
						if (temp1!=headclients || i!=temp1->filedescriptor)
							temp1 = temp1->next;
						
						bzero(message_sent,4096);
						bzero(message_received,4096);
						nbytes = read(i,message_received,4096);
						memcpy(message_sent,message_received,nbytes);
						// sending the contents of file to client
						if(nbytes > 0)
						{
							//printf("sending the contents from file %d to client %d\n",i,temp1->socketfd);fflush(stdout);
							time(&(temp1->start));
							if (send(temp1->socketfd, message_sent, nbytes, 0) == -1) 
							{
								perror("send");
							}
						}
						else
						{
							time_t end;
							time(&end);
							// checking if the client is idle for sometime so that we can close the clients connection and delete its information
							if(difftime(end,temp1->start)>0.25)
							{
								printf("Closing the connection of the client %d and the file for reading %d",temp1->socketfd,i);fflush(stdout);
								close(temp1->filedescriptor);
								FD_CLR(temp1->filedescriptor, &master);
								close(temp1->socketfd);
								FD_CLR(temp1->socketfd, &master);
								struct clientsocket* temp2 = findbyfd(i,headclients);
								if (temp2!=headclients || i!=temp2->filedescriptor)
									headclients = delete(headclients,temp2,temp2->next);
								else
									headclients = delete(headclients,temp2,temp2);
							}
						}
					}
				}// ending else when i is not sockfd
			} // ending if where is something to read is present in readfds
		} // END looping through file descriptors
	}
	return 0;
}

