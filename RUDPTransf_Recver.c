#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>

#include "libs/libRUDP-generic.h"
#include "libs/libRUDP-constants.h"
#include "libs/libRUDP-transfer.h"

int RUDP_Start_Recv(char * FileName, int FileSize, int socketToRecv, struct sockaddr_in * addrOfSender, socklen_t * len, int WindowSize, int IntSendTime) {

/* Creating Args */
	rs_threads_args argsThreads;
	 argsThreads.fileName = FileName;
	 argsThreads.fileSize = FileSize;
	 argsThreads.sockfd = socketToRecv;
	 argsThreads.addrSender = addrOfSender;
	 argsThreads.addrSenderSize = len;
	 argsThreads.windowSize = WindowSize;
	 argsThreads.intSendTime = IntSendTime;
/* Creating Fifos */ 
	char * textId = calloc(6,sizeof(char));
	snprintf(textId, 6, "%d", getpid());

	int i=0;
	char * fifoNames[5]; for(i=0;i<5;i++) fifoNames[i]=calloc(MAXFIFONAME_SIZE,sizeof(char));

	fifoNames[RS_FINDEX_TOLOG] = strcat(fifoNames[RS_FINDEX_TOLOG], RS_FIFO_TO_LOG);
	fifoNames[RS_FINDEX_TOLOG] = strcat(fifoNames[RS_FINDEX_TOLOG], textId);
	fifoNames[RS_FINDEX_WTOS] = strcat(fifoNames[RS_FINDEX_WTOS], RS_FIFO_WINMAN_TO_SENDER);
	fifoNames[RS_FINDEX_WTOS] = strcat(fifoNames[RS_FINDEX_WTOS], textId);
	fifoNames[RS_FINDEX_RTOW] = strcat(fifoNames[RS_FINDEX_RTOW], RS_FIFO_RECVER_TO_WINMAN);
	fifoNames[RS_FINDEX_RTOW] = strcat(fifoNames[RS_FINDEX_RTOW], textId);
	fifoNames[RS_FINDEX_WTOW] = strcat(fifoNames[RS_FINDEX_WTOW], RS_FIFO_WINMAN_TO_WRITER);
	fifoNames[RS_FINDEX_WTOW] = strcat(fifoNames[RS_FINDEX_WTOW], textId);
	fifoNames[RS_FINDEX_WTOR] = strcat(fifoNames[RS_FINDEX_WTOR], RS_FIFO_WINMAN_TO_RECVER);
	fifoNames[RS_FINDEX_WTOR] = strcat(fifoNames[RS_FINDEX_WTOR], textId);

	mkdir("tmp",0777);
	if(mkfifo( fifoNames[RS_FINDEX_TOLOG], 0777)) {
		if(errno!=EEXIST) { perror("RS-Recver: Cannot create RS_FIFO_TO_LOG\n"); return 2; } }
	if(mkfifo( fifoNames[RS_FINDEX_WTOS], 0777)) {
		if(errno!=EEXIST) { perror("RS-Recver: Cannot create RS_FIFO_WINMAN_TO_SENDER\n"); return 2; } }
	if(mkfifo( fifoNames[RS_FINDEX_RTOW], 0777)) {
		if(errno!=EEXIST) { perror("RS-Recver: Cannot create RS_FIFO_RECVER_TO_WINMAN\n"); return 2; } }
	if(mkfifo( fifoNames[RS_FINDEX_WTOW], 0777)) {
		if(errno!=EEXIST) { perror("RS-Recver: Cannot create RS_FIFO_WINMAN_TO_WRITER\n"); return 2; } }
	if(mkfifo( fifoNames[RS_FINDEX_WTOR], 0777)) {
		if(errno!=EEXIST) { perror("RS-Recver: Cannot create RS_FIFO_WINMAN_TO_RECVER\n"); return 2; } }

	 argsThreads.fnames = &(fifoNames[0]);

/* Launching Threads */
	pthread_t t_r_windowManager, t_r_sender, t_r_recver, t_r_timerManager, t_r_logManager, t_r_writer;
	pthread_create (&t_r_windowManager,NULL, (void *) f_r_windowManager, (void *)(&argsThreads) );
	pthread_create (&t_r_sender, 			 NULL, (void *) f_r_sender, 				(void *)(&argsThreads) );
	pthread_create (&t_r_recver, 			 NULL, (void *) f_r_recver, 				(void *)(&argsThreads) );
	pthread_create (&t_r_logManager, 	 NULL, (void *) f_r_logManager, 		(void *)(&argsThreads) );
	pthread_create (&t_r_writer,		 	 NULL, (void *) f_r_writer, 				(void *)(&argsThreads) );

/* waiting... */
	int * retVal[5];
	for(i=0;i<5;i++) retVal[i]=malloc(sizeof(int));
	pthread_join( t_r_sender, 			(void **) &retVal[0]);
	pthread_join( t_r_recver, 			(void **) &retVal[1]);
	pthread_join( t_r_logManager, 	(void **) &retVal[2]);
	pthread_join( t_r_writer, 			(void **) &retVal[3]);
	pthread_join( t_r_windowManager,(void **) &retVal[4]);
	int retValue = 0;
	for(i=0;i<5;i++) if(*((int*)retVal[i])==2) retValue = 2;

	return retValue;
}

int RUDP_Recv(int sockServe, struct sockaddr_in * addrServeServer, char * filePath, int fifo_TOlog, int whoIam) {

	signal(SIGINT, closingRecver);

	int fileSize = 0;
	char * cmdPkt = calloc(CMDPKT_SIZE, sizeof(char));
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));

	/* * * * * * * * * * * * * * * * *
	 * Start the Transfer Session    *
	 * * * * * * * * * * * * * * * * */
	// log...
	if(fifo_TOlog) {
		snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> Start Transfer Receive Session\n",getpid());
		logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
	}

	/* Initialize Address for Transfer */
	struct sockaddr_in * addrTrasfSender = malloc(sizeof(struct sockaddr_in));
	socklen_t lenTrasfSender = sizeof(*addrTrasfSender);
	/* Wait for [RDY ][] Packet */
	if ( recvfrom(sockServe, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrTrasfSender, &lenTrasfSender) < 0) { perror("RUDP: Error in recvfrom for 1st transfer control message"); return 2;}

	if ( strncmp(cmdPkt,"RDY ", CMDCMD_SIZE) == 0) {
		/* OK, is [RDY ][fileSize] Packet *
		 *      1st Transfer step         *
		 *      Read and Save fileSize    */
		memcpy(&fileSize, (cmdPkt+CMDCMD_SIZE), sizeof(int));

		/* Get Parameters from Files *
		 *   WindowSize              *
		 *   IntervalSendTime        */
		int WindowSize;
		char *AuxString = malloc(10*sizeof(char));
		FILE * param;
		int IntSendTime;
		char * windowSizePath = malloc(100*sizeof(char));
		char * intervalTimePath = malloc(100*sizeof(char));
		if(whoIam == IAMCLIENT) { 
			strncpy(windowSizePath, "ClientParams/WindowSize.param", 100);
			strncpy(intervalTimePath, "ClientParams/IntervalSendTime.param", 100);
		}
		if(whoIam == IAMSERVER) {
			strncpy(windowSizePath, "ServerParams/WindowSize.param", 100);
			strncpy(intervalTimePath, "ServerParams/IntervalSendTime.param", 100);
		}
		if(whoIam != 0) {
			if( !(param=fopen(windowSizePath,"r"))){
				WindowSize = TRANSFERWINDOW_SIZE;
				printf("Error To Retrieve WindowSize, setted as Default: %d", WindowSize);
			} else {
				memset(AuxString, 0, 10);
				fgets(AuxString, 10, param);
				WindowSize = atoi(AuxString);
			}
			if( !(param=fopen(intervalTimePath,"r"))){
				IntSendTime = INTERVALSENDTIME;
				printf("Error to Retrieve IntervalSendTime, setted as Default: %d", IntSendTime);
			} else {
				memset(AuxString, 0, 10);
				fgets(AuxString, 10, param);
				IntSendTime = atoi(AuxString);
			}
		} else {
			WindowSize = TRANSFERWINDOW_SIZE;
			IntSendTime = INTERVALSENDTIME;
		}

		/* Inizialize and Bind new Transf Socket with new Port */
		struct sockaddr_in * addrTrasfRecver = malloc(sizeof(struct sockaddr_in));
		memset((void *)addrTrasfRecver, 0, sizeof(*addrTrasfRecver));
		addrTrasfRecver->sin_family = AF_INET;
		addrTrasfRecver->sin_addr.s_addr = htonl(INADDR_ANY);
		socklen_t lenTrasfRecver = sizeof(*addrTrasfRecver);
		int sockTrasf;
		if ( (sockTrasf = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { perror("RUDP: Errore in socket() SERVICE SOCKET"); return 2; }
		if ( bind(sockTrasf, (struct sockaddr *)addrTrasfRecver, sizeof(*addrTrasfRecver)) < 0) { perror("RUDP: Error in bind() SERVICE SOCKET\n"); return 2; }

		// log...
		if(fifo_TOlog) {
			snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> RUDP: new Transfer Socket\n",getpid(), sockTrasf);
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
		}

		/* Build and Send [OK  ]+[] *
		 *    2nd Transfer step     */
		memset(cmdPkt,0,CMDPKT_SIZE);
		strncpy(cmdPkt,"OK  ", CMDCMD_SIZE);
		usleep(TIMEPOLL);
		if ( sendto (sockTrasf, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrTrasfSender, sizeof(*addrTrasfSender)) < 0) { perror("Error in sendto() for 2nd Transfer control message"); return 2; }

		// log...
		if(fifo_TOlog) {
			snprintf(buf_LOG+8, LOG_SIZE-8, "%d:%d:RUDP   -> Starting Transfer of\n- %s\n- with size: %d\n",getpid(), sockTrasf, filePath, fileSize);
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
		}

		int ret = RUDP_Start_Recv(filePath, fileSize, sockTrasf, addrTrasfSender, &lenTrasfSender, WindowSize, IntSendTime);
		return ret;
	} else if ( strncmp(cmdPkt,"NOTF", CMDCMD_SIZE) == 0) {
		/* OK, is [NOTF][] Packet      *
		 *   Transfering fail          */
		return 1;
	} else {
		if(DEBUG_ERROR) perror("RUDP: Initialize Connection Sender-Recver Fail!: dont' Receive RDY pkt");
	}
}

int RUDP_GET(int sockConn, struct sockaddr_in * addrServeServer, char * path, char * fileRequested, int fifo_TOlog, int whoIam) {
	char * cmdPkt = malloc(CMDPKT_SIZE*sizeof(char));
	char * filePath = malloc(CMDTXT_SIZE*sizeof(char));
	/* Build [GET ]+[fileRequested] *
	 *    start Transfer steps      */
	memset(cmdPkt,0,CMDPKT_SIZE);
	strncpy(cmdPkt,"GET ", CMDCMD_SIZE);
	strncat(cmdPkt, fileRequested, CMDTXT_SIZE);
	if ( sendto(sockConn, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrServeServer, sizeof(*addrServeServer)) < 0) { perror("RUDP: Error in SENDTO for CMD PKT"); return 2;}

	memset(filePath, 0, CMDTXT_SIZE);
	strcpy(filePath, path);
	strcat(filePath, fileRequested); // FolderPath+fileName

	int RUDPstatus = 0;
	RUDPstatus = RUDP_Recv(sockConn, addrServeServer, filePath, fifo_TOlog, whoIam);
	return RUDPstatus;
}


