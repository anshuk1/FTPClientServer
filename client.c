/*
FTP Client

This code implements an FTP client program. It is a very simple program that sends 
messages (user commands) from the client to the server and displays the reply message.


*/

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
/* stdlib.h is needed for definition of NULL in gcc 3.4.x and above */
#include <stdlib.h>

/* defines used in the program */
#define SERVER_FTP_PORT 7311
#define DATACONN_PORT 7312
#define REMOTE_IP_ADDRESS "127.0.0.1"
#define DEBUG 0



int ClientConnect(char *server_name, int *s);
void ClientExtractReplyCode(char *buffer, int *replycode);
void GetCmd(char * ftpcmd);
void GetPassword(char * ftpcmd);
void GetUsername(char * ftpcmd);
void ParseMsg(char * msg, char * cmd, char * arg);
int RecvMsg(int s, char *buffer, short buffersize, short *msgsize);
int SendMsg(int s, char *msg, short msgsize);
int SvcInitServer(int *s);

				/* MAIN */

				
int main(int argc, char *argv[])
{
	/* variables */
	char ftpcmd[256];		//var to store ftp comand to send to server
	char replymsg[2048];	//var to store message received from server
	int status;				//var to store returns from various functions
	short replymsgsize;		//var to store the size of message received
	int s;					//var to store control connection socket number
	int dataconn_sock;		//var to store data connection socket number
	char cmd[128];			//var to store command from user input
	char arg[128];			//var to store argument from user input
	char username[16];		//var to store username
	char password[16];		//var to store password
	int size;				//var to store size of buffer read from file
	int i;					//var used as index for looping etc
	char buffer[32];		//var to store data chunks read from file
	FILE *fp;				//file pointer
	int listensocket;		//listening server ftp socket
	
	/* open a connection to the server */
	status = ClientConnect(REMOTE_IP_ADDRESS, &s);
	
	/* exit main if ClientConnect failed */
	if(status < 0) return status;
	
	/* start listening on data connection port */
	status = SvcInitServer(&listensocket);
	if(status != 0) return status;
	
	/* get username from user and create appropriate user <username> command */
	GetUsername(ftpcmd);
	
	/* send user <username> to server */
	status = SendMsg(s, ftpcmd, strlen(ftpcmd)+1);
	if(status < 0)
	{
		close(s);
		return status;
	}
	
	/* receive reply from server */
	status = RecvMsg(s, replymsg, 2048, &replymsgsize);
	if(status < 0)
	{
		close(s);
		return status;
	}
	
	/* if reply code is not 331 username ok, quit */
	ClientExtractReplyCode(replymsg, &i);
	if(i != 331)
	{
		/* show reply message and quit */
		for(i=0; i<replymsgsize; i++)
		{
			printf("%c",replymsg[i]);
		}
		printf("\n");
		
		return -1;
	}
	
	/* if username was accepted, get password and create pass <password> command */
	GetPassword(ftpcmd);
	
	/* send pass <password> to server */
	status = SendMsg(s, ftpcmd, strlen(ftpcmd)+1);
	if(status < 0)
	{
		close(s);
		return status;
	}
	
	/* receive reply from server */
	status = RecvMsg(s, replymsg, 2048, &replymsgsize);
	if(status < 0)
	{
		close(s);
		return status;
	}
	
	/* show reply message */
	for(i=0; i<replymsgsize; i++)
	{
		printf("%c",replymsg[i]);
	}
	printf("\n");
	
	/* if reply code is not 230 user logged in, quit */
	ClientExtractReplyCode(replymsg, &i);
	if(i != 230)
	{
		return -1;
	}
	
	/* is the username and password was accepted, proceed with the client. keep asking */
	/* for input, send it to server and display received reply, until quit command     */
	while(strcmp(cmd, "quit") != 0)
	{
		//show prompt, get the input and store it in ftpcmd */
		GetCmd(ftpcmd); 
		
		/* break input into command and argument */
		ParseMsg(ftpcmd, cmd, arg);
		


		/* ******************************************** */
		/*       HW # 3 - SEND and RECV Commands        */
		/* ******************************************** */
		
		/* listening on data connection socket at the beginning of program */
		/* using the following code (COMMENTED OUT HERE)                   */
		//status = SvcInitServer(&listensocket);
		//if(status != 0) return status;

		
		/* check if cmd is SEND or STOR */
		if(strcmp(cmd, "send") == 0 || strcmp(cmd, "stor") == 0)
		{	
			/* send the command first on control so server is waiting for data */
			status = SendMsg(s, ftpcmd, strlen(ftpcmd) + 1);
			if(status < 0)
			{
				close(s);
				return status;
			}
				
			/* Wait until connection from server is requested */
			dataconn_sock = accept(listensocket, NULL, NULL);
			if(dataconn_sock < 0)
			{
				perror("Cannot accept connection");
				exit (-1);
			}
			
			/* debug statement */
			if(DEBUG) printf("data connection established . . . reading file to send\n");
			
			/* open the file to send */
			fp = fopen(arg, "r");
		
			/*  if file i/o errors - show error on server & copy error message in replymsg */
			if(fp == NULL)
			{
				/* show error message */
				printf("ERROR : Cannot open file %s\n", arg);
				
				/* close data connection socket */
				close(dataconn_sock);
				
				/* start at beginning of while(cmd != quit) */
				continue;
			}
			else /* if file opened successfully, send data */
			{
				/* display message to user */
				if (DEBUG) printf("Sending file %s\n", arg);
				
				/* send */
				while (!feof(fp))
				{	
					size = fread(buffer, 1, 32, fp);
					
					if(DEBUG) printf("\nSize (from fread) = %d\n", size);
					
					if(size > 0)
					{
						status = SendMsg(dataconn_sock, buffer, size);
						if(status < 0)
						{
							close(dataconn_sock);
							close(fp);
							return status;
						}
					}
					else if (size == 0)
					{
						//close(dataconn_sock);
						//fclose(fp);
						break;
					}
					else
					{
						printf("ERROR : Could not read from file %s\n", arg);
						//close(dataconn_sock);
						//close(fp);
					}	
				} // while (feof(fp) != 1)
			} //if (fp == NULL)
			
			/* close data connection socket */
			close(dataconn_sock);
			
			/* close file */
			close(fp);
			
			/* wait for reply */
			status = RecvMsg(s, replymsg, 2048, &replymsgsize);
			if(status < 0)
			{
				close(s);
				return status;
			}
			
			/* show reply message */
			for(i=0; i<replymsgsize; i++)
			{
				printf("%c",replymsg[i]);
			}
			printf("\n");
			
			continue;
		} //if command = SEND
				
		
		/* check if cmd is RECV or RETR */
		if(strcmp(cmd, "recv") == 0 || strcmp(cmd, "retr") == 0)
		{
			
			/* send the command first on control so server sends data */
			status = SendMsg(s, ftpcmd, strlen(ftpcmd)+1);
			if(status < 0)
			{
				close(s);
				return status;
			}
				
			/* Wait until connection is requested */
			dataconn_sock = accept(listensocket, NULL, NULL);
			if(dataconn_sock < 0)
			{
				perror("Cannot accept connection");
				exit (-1);
			}
			
			/* debug statement */
			if(DEBUG) printf("data connection established . . . opening file to be received for write\n");
			
			/* open the file to write */
			fp = fopen(arg, "w");
		
			/*  if file i/o errors - show error and start from while(cmd != quit) */
			if(fp == NULL)
			{
				/* show error message */
				printf("ERROR : Cannot open file %s\n", arg);
				
				/* close data connection socket */
				close(dataconn_sock);
				
				/* start at beginning of while(cmd != quit) */
				continue;
			}
			else /* if file opened successfully, receive data */
			{
				/* display message to user */
				if (DEBUG) printf("Receiving file %s\n", arg);
				
				/* receive */
				do
				{	
					/* get data on data connection socket */
					status = RecvMsg(dataconn_sock, buffer, 32, &size);
					if(status < 0)
					{
						close(dataconn_sock);
						close(fp);
						return status;
					}
					
					/* write if data came thru */
					if(size > 0)
					{
						fwrite(buffer, 1, size, fp);
					}
					
					/* debug statement */
					if(DEBUG) printf("\nSize (from fwrite) = %d\n", size);
					
					/* break out if no more data left */
					if (size == 0)
					{
						break;
					}	
				} while (size > 0);	
			} //if (fp == NULL)
			
			/* close data connection socket */
			close(dataconn_sock);
			
			/* close file */
			close(fp);
			
			/* wait for reply on control connection */
			status = RecvMsg(s, replymsg, 2048, &replymsgsize);
			if(status < 0)
			{
				close(s);
				return status;
			}
			
			/* show reply message */
			for(i=0; i<replymsgsize; i++)
			{
				printf("%c",replymsg[i]);
			}
			printf("\n");
			
			/* start at beginning of while (cmd != quit) */
			continue;
			
		} //if command = RECV
		
		
		
		/* ******************************************** */
		/*     END OF HW # 3 - SEND and RECV Commands   */
		/* ******************************************** */
		
		
		
		/* if command is neither SEND nor RECV, do nothing special, */
		/* send command as it is and wait for reply 	    	    */	
		
		/* send the message */
		status = SendMsg(s, ftpcmd, strlen(ftpcmd)+1);
		if(status < 0)
		{
			close(s);
			return status;
		}
		
		/* wait for reply */
		status = RecvMsg(s, replymsg, 2048, &replymsgsize);
		if(status < 0)
		{
			close(s);
			return status;
		}
		
		/* display the reply message to user if not in DEBUG mode */
		if(!DEBUG)
		{
			for(i=0; i<replymsgsize; i++)
			{
				printf("%c",replymsg[i]);
			}
			printf("\n");
		}
			
	} // while (cmd ! = quit)
	
	/* close the control socket */
	close(s);
	
	/* quit main() */
	return 0;
}

				/* END OF MAIN */

			
									

/* 
function GetCmd displays a prompt to the user and reads user input

Variables :
	char * ftpcmd : used to pass back user typed string

*/		

void GetCmd(char * ftpcmd)
{	
	/* Show the prompt */
	printf("[-] FTP > ");
	
	/* read in the typed string */
	gets(ftpcmd);
	
	/* bye */
	return;
}

/* function to prompt user for a username, read the user name and create the FTP command
user <username> using the data

variables : char *ftpcmd -- return pointer pointing to the command
*/
void GetUsername(char * ftpcmd)
{
	char temp[16];
	
	/* copy first part of command into variable */
	strcpy(ftpcmd, "USER ");
	
	/* show prompt to user */
	printf("Username : ");
	
	/* read user input */
	gets(temp);
	
	/* create command */
	strcat(ftpcmd, temp);
	
	/* leave */
	return;
}

/* function to prompt user for a password, read the password and create the FTP command
pass <password> using the data

variables : char *ftpcmd -- return pointer pointing to the command
*/
void GetPassword(char * ftpcmd)
{
	char temp[16];
	
	/* copy first part of command into variable */
	strcpy(ftpcmd, "PASS ");
	
	/* show prompt to user */
	printf("Password : ");
	
	/* read user input */
	gets(temp);
	
	/* create command */
	strcat(ftpcmd, temp);
	
	/* leave */
	return;
}
			
/*function to extract command and argument from the received message

params:
	msg : full message
	cmd : command (OUTPUT)
	arg : argument (OUTPUT)	

Description : 
function takes the message and breaks it into a command and an argument. if only a
command is present, arg points to NULL. Whitespaces before and after the message are
also discarded. It is assumed that the command and srgument are separated by one or
more whitespaces.
*/

void ParseMsg(char * msg, char * cmd, char * arg)
{
	/* local variables to store strings */
	char a[512];
	char b[512];
	/* variables used as indexes */
	int i=0;
	int j=0;

	while(msg[i]==' ') i++; //get rid of whitespaces before cmd

	/* first part of the string, until a <space> or null terminator,
	is the command and is copied into a*/
	while(msg[i] != ' ' && msg[i] != '\0') a[j++]=msg[i++]; //get cmd
	
	a[j++]='\0'; //add null terminator at the end of a
	
	/* if a space was encountered instead of null terminator, argument exists.
	copy argument into b */
	if(msg[i] != '\0')
	{
		while(msg[i]==' ') i++; //get rid of white spaces between cmd and arg
		
		j=0; //reinitialize j
		
		while(msg[i] != '\0') b[j++]=msg[i++]; //get arg
		
		b[j++]='\0'; //add null terminator
		
		/* copy a into cmd and b into arg */
		strcpy(cmd, a);
		strcpy(arg, b);
	}
	else	//else no argument was found
	{
		/* copy a into cmd and point arg to NULL */
		strcpy(cmd, a);
		*arg=NULL;
	}
	
	/* convert the command part to all lower case */
	for(i = 0; i < strlen(cmd); i++)
	{
		cmd[i] = tolower(cmd[i]);
	}
	
	/* leave function */
	return;
}
	
					
/* function to send a message to the server

params:
	s		socket to be used
	*msg 	buffer containing the message
	msgsize 	size of the message in bytes
Returns : 
	Integer, -ve if failed, 0 or +ve if sent successfully		
*/	
int SendMsg(int s, char *msg, short msgsize)
{
	int i;
	
	/* only print message if DEBUG is set to 1 */
	if (DEBUG)
	{
		printf("SendMsg (%d) : ",s);
		for(i=0;i<msgsize;i++)
		{
			printf("%c", msg[i]);
		}
		printf("\n");
	}
	
	if ((send(s, msg, msgsize, 0)) < 0)	//transmit
	{
		perror("Unable to send message");
		return -1;
	}

	return 0;	//successful send
}
		


/* function to receive a message from the server

params:
	s			socket
	*buffer		buffer to store received message
	buffersize		size of the buffer
	msgsize		size of the received message
Returns : 
	Integer, -ve if failed, 0 or +ve if sent successfully		
*/				
int RecvMsg(int s, char *buffer, short buffersize, short *msgsize)
{
	int i;
	
	*msgsize = recv(s, buffer, buffersize, 0);	//receive
	
	if(*msgsize < 0)
	{
		perror("Unable to receive");
		return -1;
	}
	
	/* only print message if DEBUG is set to 1 */
	if (DEBUG)
	{
		printf("RecvMsg (%d) : ",s);
		for(i=0;i<*msgsize;i++)
		{
			printf("%c",buffer[i]);
		}
		printf("\n");
		
	}
	
	return 0;
}		



/* function to connect to the server

params:
	*server_name	server hostname
	*s		socket (output parameter)
Returns : 
	Integer - less than zero means failed, successful otherwise
*/	
int ClientConnect(char *server_name, int *s)
{
	int sock;
	struct	sockaddr_in us;		//local client address
	struct	sockaddr_in them;	//remote server address
	struct hostent *he;			//host entry
	
	if((he = gethostbyname(server_name)) == NULL)
	{
		printf("Unknown Server : %s ", server_name);
		return -1;
	}		

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Cannot create socket");
		return -1;
	}
		
	memset((char *)&us, 0, sizeof(us));	//set local client ftp address	
	us.sin_family = AF_INET;		//internet protocol family
	us.sin_addr.s_addr = htonl(INADDR_ANY);	//INADDR_ANY is 0	
	us.sin_port = 0;			//system will allocate port
	
	if(bind(sock, (struct sockaddr *)&us, sizeof(us)) < 0)
	{
		perror("Cannot bind");
		close(sock);
		return -1;
	}
	
	memset((char *) &them, 0, sizeof(them));	//set remote server ftp address
	them.sin_family = AF_INET;
	memcpy((char *)&them.sin_addr, he->h_addr, he->h_length);
	them.sin_port = htons(SERVER_FTP_PORT);
	
	if(connect(sock, (struct sockaddr *)&them, sizeof(them)) < 0)
	{
		perror("Cannot connect");
		close(sock);
		return -1;
	}
	
	*s = sock;	//return socket
	
	return 0;	//successful connection
}




/* function to extract the code from server reply message

params:	
	*buffer		message received
	*reply_code	extracted code	

Description :
	Function takes a reply message in the form "888 Some String"		
	and extracts the code (888) from it.
*/	
void ClientExtractReplyCode(char *buffer, int *replycode)
{
	/* extract the code from the server reply message */
	sscanf(buffer, "%d", replycode);
	
	return;
}

	

/* function to initialize the server and wait for incoming connections

params:
	*s	socket

Description:
	Function waits for incoming requests, creates a socket and binds it to the 
	specified port number (in the #define), and returns the socket value.			
Returns : 
	Integer, 0 if  successfull. Non zero value otherwise	
*/
int SvcInitServer(int *s)
{
	int sock;
	struct sockaddr_in svcAddr;
	int qlen;
	
	/* Create a socket endpoint */
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Cannot create socket");
		return -1;
	}
	
	/* Initialize svcAddr */
	memset((char *)&svcAddr, 0, sizeof(svcAddr));
	
	svcAddr.sin_family = AF_INET;
	svcAddr.sin_addr.s_addr = htons(INADDR_ANY);
	svcAddr.sin_port = htons(DATACONN_PORT);
	
	if(bind(sock, (struct sockaddr *)&svcAddr, sizeof(svcAddr)) < 0)
	{
		perror("Cannot bind");
		close(sock);
		return -1;
	}
	
	/* Listen for connection request */
	qlen = 1;		//allow 1 outstanding connect request to wait
	
	listen(sock, qlen);
	
	*s = sock;		//set socket value to be returned in *s
	
	return 0;		//successful return
	
}
