
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "libRUDP-constants.h"
#include "libRUDP-generic.h"
#include "libRUDP-transfer.h"

char * showConsoleUserInterface( char * cmd, int errors, LogCommandsList * logcmdlist) {
	showCUIMenu(errors, logcmdlist); // print the Console User Interface Menu
	scanf("%s", cmd); // fill it with the command
	while(getchar()!='\n'); // remove '\n' from stdin
	fflush(stdout);
	return cmd;
}

char * getCUIFileName(char * fileRequested) {
	printf("Please, type the file name, or \"..\" to go back to menu\n"); fflush(stdout);
	scanf ("%s", fileRequested);
	while(getchar()!='\n'); //remove '\n' char from stdin
	return fileRequested;
}

void showCUIMenu(int err, LogCommandsList * logcmdlist) {
	  puts("");
	  puts("                                  __ ");
	  puts("    /\\    /\\  _____  _ ___   __  / /  ");
	  puts("   /  \\  /  \\ \\  _ \\ \\ | _ \\ \\ \\ \\ \\ 	  ");
	  puts("  / /\\ \\/ /\\ \\ \\  __| \\ \\ \\ \\ \\ \\_\\ \\	  	          ");
	  puts(" / /  \\__/  \\_\\ \\____\\ \\_\\ \\_\\ \\_____\\");
	printf(" \\ \\ >~~~~~[L]ist~of~files~~~~~~ ~~ ~ ~ ~  ~| %s\n", logcmdlist->logCommands[((logcmdlist->pos)-1+8)%8] );
  printf(" / / >~~~~~[G]et~a~file~~~~~~~~ ~~~ ~~ ~  ~ | %s\n",   logcmdlist->logCommands[((logcmdlist->pos)-2+8)%8] );
  printf(" \\ \\ >~~~~~[P]ut~a~file~~~~~~~~~~~~ ~~ ~ ~  | %s\n", logcmdlist->logCommands[((logcmdlist->pos)-3+8)%8] );
  printf(" / / >~~~~~[Q]uit~~~~~~~~~~~~~~~ ~~~ ~ ~  ~ | %s\n",   logcmdlist->logCommands[((logcmdlist->pos)-4+8)%8] );
	printf(" \\ \\_________________________               | %s\n", logcmdlist->logCommands[((logcmdlist->pos)-5+8)%8] );
	if (err==1)
	  puts("  \\_________________________/ Command Not Found, Retry");
	else
	  puts("  \\_________________________/ type your command");
	fflush(stdout);
}

void srvr_logManager() {

	/* Inizialize Strctures and Files */
	signal(SIGINT, closing_clnt_srvr_LogManager);
	FILE * logFile = 0;
	FILE * usrsFile = 0;
	char * usrFilePath = calloc(LOG_SIZE,sizeof(char));
	char * logFilePath = calloc(LOG_SIZE,sizeof(char));
	char * counttime = calloc(LOG_SIZE,sizeof(char));
	char * AuxString = calloc(10,sizeof(char));
	time_t timer;
	timer=time(NULL);
	struct tm * t;
	t = localtime(&timer);
	mkdir("log", 0777);
	strftime(counttime, LOG_SIZE, "%y-%m-%d|%H-%M-%S--", t);
	strncat(logFilePath, "log/", LOG_SIZE);
	strncat(logFilePath, counttime, LOG_SIZE);
	snprintf(AuxString, 10, "%d", getpid());
	strncat(logFilePath, AuxString, LOG_SIZE);
	strncpy(usrFilePath, logFilePath, LOG_SIZE);
	strncat(logFilePath,"-RUDP_Server.log", LOG_SIZE);
	strncat(usrFilePath,"-RUDP_Server_Users.log", LOG_SIZE);
	logFile  = fopen(logFilePath, "a"); // File where Log
	usrsFile = fopen(usrFilePath, "a"); // File where Log users
	if(logFile<0) { perror("LOG: Error in Open logfile"); exit(1);}
	if(usrsFile<0) { perror("LOG: Error in Open usrsLogFile"); exit(1);}

	int i =0;
	int endPrint = 1;

	/* Sync Fifo with Log Thread */
	int fifo_TOlog;
	int nread=0;
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( SERVER_FIFO_TO_LOG, O_RDONLY );
	if(fifo_TOlog<0) { perror("LOG: Cannot open fifo_TOlog\n"); exit(1); }

	/*...*/
	fputs("\n----------------------------------------"
				"\n--------NEW---EXECUTION--> ", logFile); fputs(asctime(localtime(&timer)), logFile);
	fflush(logFile);
	fputs("\n----------------------------------------"
				"\n--------NEW---EXECUTION--> ", usrsFile); fputs(asctime(localtime(&timer)), usrsFile);
	fflush(usrsFile);
	int WORKING = 1;
	int DEBV = 0;
	int LOGV = 0;

	/* Start Routine Log Manager */
	while(WORKING) {
		nread = read(fifo_TOlog, buf_LOG, LOG_SIZE);
		timer=time(NULL);
		t = localtime(&timer);
		memset(counttime, 0, LOG_SIZE);
		strftime(counttime, LOG_SIZE, "%x-%X ", t);
		if(nread<0) { 
			perror("LOG: Read Error\n"); exit(1);
		}
		if( strncmp(buf_LOG,"USR",3) == 0) {
			fputs(counttime, usrsFile);
			fputs("  new User:   ", usrsFile);
			fputs(buf_LOG+3, usrsFile);
			fflush(usrsFile);
		} else if( strncmp(buf_LOG,"EXT",3) == 0) {
			fputs(counttime, usrsFile);
			fputs(" User leaves: ", usrsFile);
			fputs(buf_LOG+3, usrsFile);
			fflush(usrsFile);
		} else {
			memcpy(&DEBV, buf_LOG, sizeof(int));
			memcpy(&LOGV, buf_LOG+4, sizeof(int));
			if(DEBUG_SERVER>=DEBV) {
				fputs(buf_LOG+8, stdout);
				fflush(stdout);
			}
			if(LOG_TO_FILE>=LOGV) {
				fputs(counttime, logFile);
				fputs(buf_LOG+8, logFile);
				fflush(logFile);
			}
		}
	}
}

void clnt_logManager(char * logfifoname) {

	/* Inizialize Strctures */
	signal(SIGINT, closing_clnt_srvr_LogManager);
	int i =0;
	int endPrint = 1;
	FILE * logFile = 0;
	char * logFilePath = calloc(LOG_SIZE,sizeof(char));
	char * counttime = calloc(LOG_SIZE,sizeof(char));
	char * AuxString = calloc(10,sizeof(char));
	time_t timer;
	timer=time(NULL);
	struct tm * t;
	t = localtime(&timer);
	mkdir("log", 0777);
	strftime(counttime, LOG_SIZE, "%y-%m-%d|%H-%M-%S--", t);
	strncat(logFilePath, "log/", LOG_SIZE);
	strncat(logFilePath, counttime, LOG_SIZE);
	snprintf(AuxString, 10, "%d", getpid());
	strncat(logFilePath, AuxString, LOG_SIZE);
	strncat(logFilePath,"-RUDP_Client.log", LOG_SIZE);
	logFile  = fopen(logFilePath, "a"); // File where Log
	if(logFile<0) { perror("LOG: Error in Open logfile"); exit(1);}

	/* Sync Fifo with Log Thread */
	int nread=0;
	int fifo_TOlog;
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( logfifoname, O_RDONLY );
	if(fifo_TOlog<0) { perror("LOG: Cannot open fifo_TOlog\n"); exit(1); }

	/*...*/
	fputs("\n----------------------------------------"
				"\n--------NEW---EXECUTION--> ", logFile); fputs(asctime(localtime(&timer)), logFile);
	fflush(logFile);

	int WORKING = 1;
	int DEBV = 0;
	int LOGV = 0;

	/* Start Routine Log Manager */
	while(WORKING) {
		memset(buf_LOG, 0, LOG_SIZE);
		nread = read(fifo_TOlog, buf_LOG, LOG_SIZE);
		timer=time(NULL);
		t = localtime(&timer);
		memset(counttime, 0, LOG_SIZE);
		strftime(counttime, LOG_SIZE, "%x-%X ", t);
		if(nread<0) { 
			perror("LOG: Read Error\n"); exit(1);
		} else {
			memcpy(&DEBV, buf_LOG, sizeof(int));
			memcpy(&LOGV, buf_LOG+4, sizeof(int));
			if(DEBUG_CLIENT>=DEBV) {
				fputs(buf_LOG+8, stdout);
				fflush(stdout);
			}
			if(LOG_TO_FILE>=LOGV) {
				fputs(counttime, logFile);
				fputs(buf_LOG+8, logFile);
				fflush(logFile);
			}
		}
	}
}

void logit(char * buffer, int fifo_TOlog, int size, int debugv, int flogv) {
	int debug = debugv;
	int flog = flogv;
	memcpy(buffer, &debug, sizeof(int));
	memcpy(buffer+4, &flog, sizeof(int));
	write (fifo_TOlog, buffer, size );
}


void closing_clnt_srvr_LogManager() {
	printf(" K:'%d' ", getpid());
	fcloseall();
	exit(2);
}

void closing_client() {
	printf(" K:'%d' ", getpid());
	fcloseall();
	exit(2);
}

void closing_server() {
	printf(" K:'%d' ", getpid());
	fcloseall();
	unlink(SERVER_FIFO_TO_LOG);
	exit(2);
}

long fileSize(char * FileName) {
  long lSize;
	FILE * pFile;
  if ( !( pFile = fopen ( FileName , "rb" ) ) ) {
    printf("FileSize Calcolate: Error to open File!! %s\n", FileName);fflush(stdout); exit (1);
  }
  fseek (pFile , 0 , SEEK_END);
  lSize = ftell (pFile);
  rewind (pFile);
	fclose(pFile);
	return lSize;
}




