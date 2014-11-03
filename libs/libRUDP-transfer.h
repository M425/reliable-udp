#ifndef LIBRUDPTRANSFER_H
#define LIBRUDPTRANSFER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <dirent.h>

#include "listP.h"

#define WINSTATE_TOSEND 2
#define WINSTATE_SENT 1
#define WINSTATE_ACKED 0
#define WINSTATE_RECVED 0
#define WINSTATE_TORECV 2

// Argument of Sender Side Threads
typedef struct _SS_ThreadsARGS {
	char * fileName;
	int fileSize;
	int sockfd;
	struct sockaddr_in * addrRecver;
	socklen_t addrRecverSize;
	char ** fnames;
	int windowSize;
	int intSendTime;
	double lossProb;
} ss_threads_args;

// Argument of Receiver Side Threads
typedef struct _RS_ThreadsARGS {
	char * fileName;
	int fileSize;
	int sockfd;
	struct sockaddr_in * addrSender;
	socklen_t * addrSenderSize;
	char ** fnames;
	int windowSize;
	int intSendTime;
} rs_threads_args;

// Transfert Window
typedef struct _RUDP_TRANSFER_WINDOW {
	int * PKTS_STATE;
	int * PKTS_TRY;
	int TOT_FLP;
	int WIN_START;
	int WIN_SIZE;
	int PKTS_STOCK;
} TransferWindow;

// Buffering Window
typedef struct _RUDP_BUFFERING_WINDOW {
	char ** PKT_BUF;
	char ** temp_BUF;
	int dim;
} BufferingWindow;

// Log Commands List
typedef struct _LOG_COMMANDS_LIST {
	char ** logCommands;
	char * currentstring;
	int dim;
	int pos;
	int strlength;
} LogCommandsList;

/* Functions for transfer RUDP File, Sender Side */
 int * f_s_sender();
 int * f_s_windowManager();
 int * f_s_recver();
 int * f_s_timerManager();
 int * f_s_logManager();
 void closingSender();
 int RUDP_Start_Send(char * FileName, int socketToSend, struct sockaddr_in * addrOfRecver, int WindowSize, int IntSendTime, double LossProb);
 int RUDP_Send(int sockServe, struct sockaddr_in * addrConnClient, char * filePath, int fifo_TOlog, int whoIam); 
 int RUDP_PUT(int sockConn, struct  sockaddr_in * addrServeServer, char * path, char * fileRequested, int fifo_TOlog, int whoIam);

/* Functions for transfer RUDP File, Receiver Side */
 int * f_r_sender();
 int * f_r_windowManager();
 int * f_r_recver();
 int * f_r_writer();
 int * f_r_logManager();
 void closingRecver();
 int RUDP_Start_Recv(char * FileName, int FileSize, int socketToRecv, struct sockaddr_in * addrOfSender, socklen_t * len, int WindowSize, int IntSendTime);
 int RUDP_Recv(int sockConn, struct sockaddr_in * addrServeServer, char * fileRequested, int fifo_TOlog, int whoIam);
 int RUDP_GET(int sockConn, struct sockaddr_in * addrServeServer, char * path, char * fileRequested, int fifo_TOlog, int whoIam);

/* Aux Transfer Functions */
 char * addHeader(char * dest, int *header, char * buff, int sizeH, int sizeB);

/* Transfer Window Functions */
 TransferWindow * new_Window(TransferWindow * window, int tot_flp, int win_start, int win_size);
 int FindNextToSendInWindow(TransferWindow * window);
 double MeanSendTry(TransferWindow * window);
 int moveWindow(TransferWindow * window, int seqRcvd, int whoiam);
 int resendIt(TransferWindow * window, int occSeq);

/* Buffering Window Functions */
 BufferingWindow * new_BufWindow(BufferingWindow * window, int dimension);
 BufferingWindow * shift_BufWindow(BufferingWindow * window, int posMov);

/* Other Aux Function */
LogCommandsList * logcmd(LogCommandsList * logcmdlist, int type, char * fileName);
LogCommandsList * newLogCommandsList(LogCommandsList * logcmd, int dime, int stringl);
int checkTimer(my_list_p * list, long actualTime);

#endif /* LIBRUDPTRANSFER_H */ 

