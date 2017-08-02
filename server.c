/* 

The program acts as an FTP server. It waits to listen for incoming connections.
When a conenction is requested, it creates a socket, binds it to the specified port 
and allows for data transfer on that port between client and server. 

The program will wait for a user <username> command followed by a pass <password>
command. When the username and password is received, the program will compare them 
to the usernames / passwords in the specified file USERLIST_FILENAME. If a match is 
found, program will accept commands from the client and keep sending replies until a 
quit command is encountered.

When a quit is received, the program will end current session and get back to listening.

*/

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>

/* defines for constants used in the program */

/* default port to listen on */
#define SERVER_FTP_PORT 7311
/* file where username password is stored */
#define USERLIST_FILENAME "userlist"
/* FTP root folder - starting folder for new connections */
#define HOME_FOLDER "/home/kunal/ftphw/ftpserver"
/* if 1, includes all debug related code in program */
#define DEBUG 0
/* IP Address of client */
#define CLIENT_IP_ADDR "127.0.0.1"
/* port for data connection */
#define DATACONN_PORT 7312


void CreateReplyMsg(char * command, char * argument, char * replymsg);
int ClientConnect(char *server_name, int *s);
int Authenticate(char * user, char * pass);
void ParseMsg(char * msg, char * cmd, char * arg);
int RecvMsg(int s, char *buffer, short buffersize, short *msgsize);
int SendMsg(int s, char *msg, short msgsize);
int SvcInitServer(int *s);


				/* MAIN */

				
int main(int argc, char *argv[])
{
	/* variables */
	int listensocket;		//listening server ftp socket
	int s;					//accepting socket
	int status;				//return values from function calls
	char recvbuffer[1024];	//information received from client
	int msgsize;			//size of message
	char replymsg[1024];	//reply messages
	char cmd[128];			//command extracted from recvbuffer
	char arg[128];			//argument extracted from recvbuffer
	char username[16];		//username sent by client
	char password[16];		//password sent by client	
	
	
	/* Initialize server */
	status = SvcInitServer(&listensocket);
	if(status != 0)
	{
		printf("Error! Server exiting . . .\n");
		exit(-1);
	}
	
	/* BEGIN INFINITE LOOP */
	while(1)
	{
		/* move to home folder incase previous user moved elsewhere */
		chdir(HOME_FOLDER);
		
		/* Announce New Service */
		printf("\nService ready for new user . . .\n");
		
		/* Wait until connection is requested */
		s = accept(listensocket, NULL, NULL);
		if(s < 0)
		{
			perror("Cannot accept connection");
			exit (-1);
		}
		
		/* wait till message is received */
		status = RecvMsg(s, recvbuffer, 1024, &msgsize);
		if(status < 0)
		{
			perror("Failed to receive message");
			close(s);
			return status;
		}
		
		/* extract command and srgument from received message */
		ParseMsg(recvbuffer, cmd, arg);
		
		/* keep asking for a user <username> until provided */
		while(strcmp(cmd, "user") != 0)
		{
			strcpy(replymsg, "530 Not Logged In. Login First");
			status = SendMsg(s, replymsg, strlen(replymsg)+1);
			if(status < 0)
			{
				perror("Failed to send message\n");
				close(s);
				return status;
			}
			
			/* receive user command */
			status = RecvMsg(s, recvbuffer, 1024, &msgsize);
			if(status < 0)
			{
				perror("Failed to receive message");
				close(s);
				return status;
			}
			
			/* extract command and srgument from received message */
			ParseMsg(recvbuffer, cmd, arg);
		}
		
		/* out of loop means "user username" came thru */
		
		/* copy username into variable */
		strcpy(username, arg);
		
		/* populate reply message in variable and send it */
		strcpy(replymsg, "331 Username OK. Need Password");
		status = SendMsg(s, replymsg, strlen(replymsg)+1);
		if(status < 0)
		{
			perror("Failed to send message\n");
			close(s);
			return status;
		}
		
		/* wait for password to come thru */
		status = RecvMsg(s, recvbuffer, 1024, &msgsize);
		if(status < 0)
		{
			perror("Failed to receive message");
			close(s);
			return status;
		}
		
		/* extract command and argument from received message */
		ParseMsg(recvbuffer, cmd, arg);
		
		/* keep asking for password until "pass password" comes thru */
		while(strcmp(cmd, "pass") != 0)
		{
			strcpy(replymsg, "530 Not Logged In. Need Password");
			status = SendMsg(s, replymsg, strlen(replymsg)+1);
			if(status < 0)
			{
				perror("Failed to send message\n");
				close(s);
				return status;
			}
			
			/* receive pass command */
			status = RecvMsg(s, recvbuffer, 1024, &msgsize);
			if(status < 0)
			{
				perror("Failed to receive message");
				close(s);
				return status;
			}
			
			/* extract command and srgument from received message */
			ParseMsg(recvbuffer, cmd, arg);
		}
		
		/* copy password into variable */
		strcpy(password, arg);
		
		/* check if username / password is correct */
		status = Authenticate(username, password);
		
		/* if not correct, send a 530 back and go back to while(1) */
		if (status != 1)
		{
			
			/* send 530 if not authenticated */
			strcpy(replymsg, "530 Invalid Username / Password"); 
			status = SendMsg(s, replymsg, strlen(replymsg)+1);
			if(status < 0)
			{
				perror("Failed to send message\n");
				close(s);
				return status;
			}
		}
		/* user authenticated, send proper message and process commands until quit is received */
		else 
		{
			/* send 230 if authenticated */
			strcpy(replymsg, "230 User Logged In"); 
			status = SendMsg(s, replymsg, strlen(replymsg)+1);
			if(status < 0)
			{
				perror("Failed to send message\n");
				close(s);
				return status;
			}
			
			/* start loop until quit command is received */
			while( strcmp(cmd, "quit") != 0 )
			{
				/* receive a message */
				status = RecvMsg(s, recvbuffer, 1024, &msgsize);
				if(status < 0)
				{
					perror("Failed to receive message");
					close(s);
					return status;
				}
		
				/* break received message into command and argument */
				ParseMsg(recvbuffer, cmd, arg);
				
				/* if command is quit, send 221 and start at beginning of while(1) */
				if ( strcmp(cmd, "quit") == 0 ) 
				{
					strcpy(replymsg, "221 Goodbye"); 
					status = SendMsg(s, replymsg, strlen(replymsg)+1);
					if(status < 0)
					{
						perror("Failed to send message\n");
						close(s);
						return status;
					}
					continue;
				}

				/* for all other commands, create a reply message */
				CreateReplyMsg(cmd, arg, replymsg);
				
				/* send the reply message */
				status = SendMsg(s, replymsg, strlen(replymsg)+1);
				if(status < 0)
				{
					perror("Failed to send message\n");
					close(s);
					return status;
				}
			
			} // while (strcmp(cmd, "quit") != 0)

			
		} // if (status != 1) - else - user authenticated
		
		
	} // while(1)
	
	
	return 0;
}

				/* END OF MAIN */
			
			
/* 
function CreateReplyMsg creates a reply message to be sent to the client depending on the 
incoming command and argument. 

This function has the code for processing all commands.

variables:
	char * command : command passed in to function	
	char * argument : argument passed in to function
	char * replymsg : reply message passed back by function

Description:
	The function reads in the command and argumrent passed to it; performs the task requested 
	and creates a reply message. The output of system commands are piped to files named 
	.<command>.tmp and are deleted after the data is read from them. 	
*/			
void CreateReplyMsg(char * command, char * argument, char * replymsg)						
{
	/* variables */
	FILE *fp;			//file pointer to open files
	char cmdstr[128];	//string to store commands to run on the file system
	int i;				//used in loops et al
	unsigned char ch;	//used to read a character from a file
	char buffer[32];	//used to read binary data from file for send / recv
	int size;			//size of read / write buffer
	int dataconn_sock;	//socket for data connection
	
	
	/* processing commands */
	
	/* ******************************************** */
	/* HW # 3 - SEND and RECV Commands on server    */
	/* ******************************************** */
	
	/* send or stor */
	if (strcmp(command, "send") == 0 || strcmp(command, "stor") == 0)
	{
		/* debug statement */
		if(DEBUG) printf("Writing file %s\n", argument);
	
		/* open file to write data to - comes as argument e.g. send <filename> */	
		fp = fopen(argument, "w");
		
		/*  if file i/o errors - show error on server & copy error message in replymsg */
		if(fp == NULL)
		{
			printf("ERROR : Cannot open file %s\n", argument);
			strcpy(replymsg, "451 Action aborted. Local error on server");
			return;
		}
		
		/* debug message */
		if(DEBUG) printf("Establishing connection with client\n");
		
		/* establish connection to client */
		i = ClientConnect(CLIENT_IP_ADDR, &dataconn_sock);
		if(i < 0) {strcpy(replymsg, "425 Can't open data connection"); return;}
		
		/* debug statement */
		if(DEBUG) printf("Connection established . . . receiving file\n");
		
		/* loop to read incoming data and write it to local file */
		do
		{
			/* receive data from client */
			i = RecvMsg(dataconn_sock, buffer, 32, &size);
			if(i < 0)
			{
				perror("Failed to receive file");
				strcpy(replymsg, "451 Action aborted. Local error on server");
				close(dataconn_sock);
				return;
			}
			
			/* debug statement */
			if(DEBUG) printf("Received chunk of size : %d\n", size);
			
			/* if something came thru, write it */
			if(size > 0)
			{
				fwrite(buffer, 1, size, fp);
			}
			
		} while (size > 0);
		
		/* close file */
		fclose(fp);
		
		/* close data connection socket */
		close(dataconn_sock);
		
		/* send reply for successful file tranfer */
		strcpy(replymsg, "250 File Received Successfully");
		
		/* leave function */
		return;
	}
	
	/* recv or retr */
	if (strcmp(command, "recv") == 0 || strcmp(command, "retr") == 0)
	{
		/* debug statements */
		if(DEBUG) printf("Reading file %s\n", argument);
	
		/* open file to read data from - comes as argument e.g. recv <filename> */	
		fp = fopen(argument, "r");
		
		/*  if file i/o errors - show error on server & copy error message in replymsg */
		if(fp == NULL)
		{
			printf("ERROR : Cannot open file %s\n", argument);
			strcpy(replymsg, "451 Action aborted. Local error on server");
			return;
		}
		
		/* debug statement */
		if(DEBUG) printf("Establishing connection with client\n");
		
		/* connecting to client - if it fails, create reply message and go back to main */
		i = ClientConnect(CLIENT_IP_ADDR, &dataconn_sock);
		if(i < 0) {strcpy(replymsg, "425 Can't open data connection"); return;}
		
		/* debug statement */
		if(DEBUG) printf("Connection established . . . sending file\n");
		
		/* read contents of file and send it on data connection */
		while (!feof(fp))
		{
			/* read file contents */
			size = fread(buffer, 1, 32, fp);
			
			/* if data exists in file, send it thru */
			if(size > 0)
			{
				/* send data - on error, close file/socket and send error message on control */
				i = SendMsg(dataconn_sock, buffer, size);
				if(i < 0)
				{
					close(dataconn_sock);
					close(fp);
					strcpy(replymsg, "425 Data Connection Error");
					return;
				}
			}
			else if (size == 0) /* get out if no more data is left to read */
			{
				break;
			}
			else
			{
				printf("ERROR : Could not read from file %s\n", argument);
			}
			
			/* debug statement */
			if(DEBUG) printf("Sent chunk of size : %d\n", size);
			

		}// while (not eof)
		
		/* close file */
		fclose(fp);
		
		/* close data connection socket */
		close(dataconn_sock);
		
		/* create reply message to be sent on control */
		strcpy(replymsg, "250 File Sent Successfully");
		
		/* exit function */
		return;
	}
		
	/* ******************************************** */
	/*     END OF HW # 3 - SEND and RECV Commands   */
	/* ******************************************** */
		
		
	else if (strcmp(command, "pwd") == 0)
	{
		i=1;	//initialize to 1. NOT ZERO since first character in replymsg is '\n'
		
		/* issue the command and pipe output to a file */
		system("pwd > .pwd.tmp");
		 
		/* open the file for input to the program */
		fp = fopen(".pwd.tmp", "r");
		
		/*  if file i/o errors - show error on server & copy error message in replymsg */
		if(fp == NULL)
		{
			printf("ERROR : Cannot open file .pwd.tmp\n");
			strcpy(replymsg, "451 Action aborted. Local error on server");
			return;
		}
		
		/* put in a new line character first */
		strcpy(replymsg, "\n");
		
		/* read everything from file and put it in replymsg */
		while (!feof(fp) != 1)
		{	
			ch = fgetc(fp);
			replymsg[i++] = ch;
		}
		/*one extra character is read, put a null terminator before it */
		replymsg[i-1]='\0';
		
		/* add a 200 cmd ok at the end. replymsg is ready */
		strcat(replymsg, "\n200 CMD OK");
		
		/* close the file pointer */
		fclose(fp);
		
		/* kill the temporary file */
		system("rm .pwd.tmp");
	}
	else if (strcmp(command, "list") == 0 || strcmp(command, "stat") == 0)
	{
		i = 1;	//initialize to 1. NOT ZERO since first character in replymsg is '\n'
		
		/* issue the command and pipe output to a file */
		system("ls -l > .filelist.tmp");
		
		/* open the file for input to the program */			
		fp = fopen(".filelist.tmp", "r");
		
		/*  if file i/o errors - show error on server & copy error message in replymsg */	
		if(fp == NULL)
		{
			printf("ERROR : Cannot open file .filelist.tmp\n");
			strcpy(replymsg, "451 Action aborted. Local error on server");
			return;
		}
		
		/* put in a new line character first */
		strcpy(replymsg, "\n");
		
		/* read everything from file and put it in replymsg */
		while (!feof(fp))
		{	
			ch = fgetc(fp);
			replymsg[i++] = ch;
		}
		
		/*one extra character is read, put a null terminator before it */
		replymsg[i-1]='\0';
		
		/* add a 200 cmd ok at the end. replymsg is ready */
		strcat(replymsg, "\n200 CMD OK");
		
		/* close the file pointer */
		fclose(fp);
		
		/* kill the temporary file */
		system("rm .filelist.tmp");
		
	}
	else if (strcmp(command, "cwd") == 0)
	{
		/* if directory can be changed, do it and send 200 cmd ok */
		if(chdir(argument) != -1)
		{
			strcpy(replymsg, "200 CMD OK");
			return;
		}
		/* else send error message */
		else
		{
			/* add message */
			strcpy(replymsg, "553 Action Aborted. Failed to change to directory ");
			/* add requested folder name */
			strcat(replymsg, argument);
			/* add new line */
			strcat(replymsg, "\n");
			/* done, bail out */
			return;
		}
	}
	else if (strcmp(command, "cdup") == 0)
	{
		/* if directory can be changed, do it and send 200 cmd ok */
		if(chdir("..") != -1)
		{
			strcpy(replymsg, "200 CMD OK");
			return;
		}
		/* else send error message */
		else
		{
			/* add message */
			strcpy(replymsg, "553 Action Aborted. Failed to change to parent directory ");
			/* add new line */
			strcat(replymsg, "\n");
			/* done, go back */
			return;
		}
	}	
	else if (strcmp(command, "mkd") == 0)
	{
		i = 1;	//initialize to 1. NOT ZERO since first character in replymsg is '\n'
		
		/* create a command string --> command + argument */
		strcpy(cmdstr, "mkdir ");	//add shell command
		strcat(cmdstr, argument);	//add argument
		strcat(cmdstr, " 2> .mkd.tmp"); //add pipe to ouput file
		
		/* issue the command */
		system(cmdstr);
		
		/* open the file for input to the program */
		fp = fopen(".mkd.tmp", "r");
		
		/*  if file i/o errors - show error on server & copy error message in replymsg */	
		if(fp == NULL)
		{
			printf("ERROR : Cannot open file .mkd.tmp\n");
			strcpy(replymsg, "451 Action aborted. Local error on server");
			return;
		}
		/* put in a new line character first */
		strcpy(replymsg, "\n");
		
		/* read everything from file and put it in replymsg */
		while (!feof(fp))
		{	
			ch = fgetc(fp);
			replymsg[i++] = ch;
		}
		
		/*one extra character is read, put a null terminator before it */
		replymsg[i-1]='\0';
		
		/* add a 200 cmd ok at the end. replymsg is ready */
		strcat(replymsg, "\n200 CMD OK");
		
		/* close the file pointer */
		fclose(fp);
		
		/* kill the temporary file */
		system("rm .mkd.tmp");
	}		
	else if (strcmp(command, "rmd") == 0)
	{
		i = 1;	//initialize to 1. NOT ZERO since first character in replymsg is '\n'
		
		/* create the command string */
		strcpy(cmdstr, "rmdir ");	// add system command
		strcat(cmdstr, argument);	// add argument
		strcat(cmdstr, " 2> .rmd.tmp"); // add pipe to output file
		
		/* issue the command */
		system(cmdstr);
		
		/* open the file for input to the program */
		fp = fopen(".rmd.tmp", "r");
		
		/*  if file i/o errors - show error on server & copy error message in replymsg */	
		if(fp == NULL)
		{
			printf("ERROR : Cannot open file .rmd.tmp\n");
			strcpy(replymsg, "451 Action aborted. Local error on server");
			return;
		}
		
		/* put in a new line character first */
		strcpy(replymsg, "\n");
		
		/* read everything from file and put it in replymsg */
		while (!feof(fp))
		{	
			ch = fgetc(fp);
			replymsg[i++] = ch;
		}
		
		/* put in a new line character first */
		replymsg[i-1]='\0';
		
		/* add a 200 cmd ok at the end. replymsg is ready */
		strcat(replymsg, "\n200 CMD OK");
		
		/* close the file pointer */
		fclose(fp);
		
		/* kill the temporary file */
		system("rm .rmd.tmp");
	}
	else if (strcmp(command, "dele") == 0)
	{
		i = 1;	//initialize to 1. NOT ZERO since first character in replymsg is '\n'
		strcpy(cmdstr, "rm ");
		strcat(cmdstr, argument);
		strcat(cmdstr, " 2> .dele.tmp");
		
		/* issue the command and pipe output to a file */
		system(cmdstr);
		
		/* open the file for input to the program */
		fp = fopen(".dele.tmp", "r");
		
		/*  if file i/o errors - show error on server & copy error message in replymsg */	
		if(fp == NULL)
		{
			printf("ERROR : Cannot open file .dele.tmp\n");
			strcpy(replymsg, "451 Action aborted. Local error on server");
			return;
		}
		
		/* put in a new line character first */
		strcpy(replymsg, "\n");
		
		/* read everything from file and put it in replymsg */
		while (!feof(fp))
		{	
			ch = fgetc(fp);
			replymsg[i++] = ch;
		}
		
		/*one extra character is read, put a null terminator before it */
		replymsg[i-1]='\0';
		
		/* add a 200 cmd ok at the end. replymsg is ready */
		strcat(replymsg, "\n200 CMD OK");
		
		/* close the file pointer */
		fclose(fp);
		
		/* kill the temporary file */
		system("rm .dele.tmp");
	}
	else if (strcmp(command, "help") == 0)
	{
		/* add newline to the reply message */
		strcpy(replymsg, "\n");
		
		/* for help, the argument can be upper or lower case. convert it to lower */
		for(i = 0; i < strlen(argument); i++)
		{
			argument[i] = tolower(argument[i]);
		}
		
		/* depending on the argument to help, fill in appropriate reply message */
		if(strcmp(argument, "help") == 0 || *argument == NULL)
		{
			strcat(replymsg, "Type help <command> to see help for that command");
		}
		else if (strcmp(argument, "send") == 0 || strcmp(argument, "stor") == 0)
		{
			strcat(replymsg, "send (or stor) <filename> : sends the specified file to the server");
		}
		else if (strcmp(argument, "recv") == 0 || strcmp(argument, "retr") == 0)
		{
			strcat(replymsg, "recv (or retr) <filename> : receives the specified file from the server");
		}
		else if (strcmp(argument, "user") == 0)
		{
			strcat(replymsg, "user <username> : sends the username to the server");
		}
		else if (strcmp(argument, "pass") == 0)
		{
			strcat(replymsg, "pass <password> : sends the password to the server");
		}
		else if (strcmp(argument, "quit") == 0)
		{
			strcat(replymsg, "quit commands ends the current ftp session");
		}
		else if (strcmp(argument, "mkd") == 0)
		{
			strcat(replymsg, "mkd <name> creates a folder on the server root directory with the specified name");
		}
		else if (strcmp(argument, "rmd") == 0)
		{
			strcat(replymsg, "rmd <name> deletes the folder on the server root directory with the specified name");
		}
		else if (strcmp(argument, "dele") == 0)
		{
			strcat(replymsg, "dele <filename> deletes the file in the server root directory");
		}
	
		else if (strcmp(argument, "pwd") == 0)
		{
			strcat(replymsg, "pwd prints the working directory on the server");

		}	
		else if (strcmp(argument, "list") == 0)
		{
			strcat(replymsg, "list prints the list of files in the current working directory of the server\nlist <filename> prints the information about the specified file");
		}
		else if (strcmp(argument, "cwd") == 0)
		{
			strcat(replymsg, "cwd <directoryname> changes the working directory on the server to the specified directory");
		}
		else if (strcmp(argument, "cdup") == 0)
		{
			strcat(replymsg, "cdup changes the working directory on the server to the parent directory");
		}
		else /* is nothing matched, put in following message */
		{
			strcat(replymsg, "There is no help associated with this argument");
		}
		
		/* add a 200 cmd ok to reply message */
		strcat(replymsg, "\n200 CMD OK");
		
		/* done */
		return;
	}
	else /* if nothing matched, send command not implemented */
	{
		strcpy(replymsg, "502 Command not implemented");
	}
	
	return;
}


/* function to extract command and argument from the received message

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


/*function to check if the passed in username and password exists

params:
	user : username
	pass : password
Returns : 
	1 if user exists
	0 if user does not exist		

Description :
function compares the passed in username and password 
with values from the file USERLIST_FILENAME. (#defined above)
If the username / password exists function returns 1, else it returns 0
*/

int Authenticate(char * user, char * pass)
{
	char fuser[32];		//username read from file
	char fpass[32];		//password read from file
	int fread_retval;	//return value from file read (fscanf)
	int userexists = 0;	//flag to check if user exists. default - user doesnt exist
	
	FILE *fp;		//pointer to input file
	
	fp = fopen(USERLIST_FILENAME, "r");	//open input file
	
	/* if file couldnt be opened, display error and quit server */
	if(fp == NULL)
	{
		printf("ERROR : Cannot open file %s\n", USERLIST_FILENAME);
		exit(-1);
	}
	
	fread_retval = fscanf(fp, "%s %s", fuser, fpass);	//read first line
	
	/* read all username/passwords from file and compare them to values passed in */
	while (fread_retval != -1)
	{
		/* if a match occurs, set userexists to 1 */
		if(strcmp(fuser, user) == 0 && strcmp(fpass, pass) ==0)
		{
			userexists = 1;
		}
		fread_retval = fscanf(fp, "%s %s", fuser, fpass);
	}
	
	fclose(fp);	//close file pointer
	
	return userexists;	//return 1 if match found, 0 otherwuse
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
	svcAddr.sin_port = htons(SERVER_FTP_PORT);
	
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

			
/*
function to send a message to the client

params:
	s		socket to be used
	*msg 		buffer containing the message
	msgsize 	size of the message in bytes
Returns : 
	Integer, -ve if failed, 0 or +ve if sent successfully		
*/	
int SendMsg(int s, char *msg, short msgsize)
{
	int i;
	
	/* display send message if DEBUG = 1 */
	if (DEBUG)
	{
		printf("SendMsg (%d) : ",s);
		for(i=0;i<msgsize;i++)
		{
			printf("%c", msg[i]);
		}
		printf("\n");
		
		//printf("Size of message : %d (%d)\n", msgsize, strlen(msg));
	}
	
	if ((send(s, msg, msgsize, 0)) < 0)	//transmit
	{
		perror("Unable to send message");
		return -1;
	}

	return 0;	//successful send
}
		
/*function to receive a message from the client

params:
	s		socket
	*buffer		buffer to store received message
	buffersize	size of the buffer
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
	
	/* display receive message if DEBUG = 1 */
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

/* WRITE ME

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
		
	memset((char *)&us, 0, sizeof(us));		//set local client ftp address	
	us.sin_family = AF_INET;				//internet protocol family
	us.sin_addr.s_addr = htonl(INADDR_ANY);	//INADDR_ANY is 0	
	us.sin_port = 0;						//system will allocate port
	
	if(bind(sock, (struct sockaddr *)&us, sizeof(us)) < 0)
	{
		perror("Cannot bind");
		close(sock);
		return -1;
	}
	
	memset((char *) &them, 0, sizeof(them));	//set remote server ftp address
	them.sin_family = AF_INET;
	memcpy((char *)&them.sin_addr, he->h_addr, he->h_length);
	them.sin_port = htons(DATACONN_PORT);
	
	if(connect(sock, (struct sockaddr *)&them, sizeof(them)) < 0)
	{
		perror("Cannot connect");
		close(sock);
		return -1;
	}
	
	*s = sock;	//return socket
	
	return 0;	//successful connection
}
