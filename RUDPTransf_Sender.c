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
#include <dirent.h>

#include "libs/libRUDP-generic.h"
#include "libs/libRUDP-constants.h"
#include "libs/libRUDP-transfer.h"

int RUDP_Start_Send(char * FileName, int socketToSend, struct sockaddr_in * addrOfRecver, int WindowSize, int IntSendTime, double LossProb) {

/* Creating Args */
	ss_threads_args argsThreads;
	 argsThreads.fileName = FileName;
	 argsThreads.fileSize = fileSize(FileName);
	 argsThreads.sockfd = socketToSend;
	 argsThreads.addrRecver = addrOfRecver;
	 argsThreads.addrRecverSize = sizeof(struct sockaddr_in); // ????
	 argsThreads.windowSize = WindowSize;
	 argsThreads.intSendTime = IntSendTime;
	 argsThreads.lossProb = LossProb;

/* Creating Fifos */ 
	char * textId = calloc(5,sizeof(char));
	sprintf(textId, "%d", getpid());
	int i = 0;
	char * fifoNames[6]; for(i=0;i<6;i++) fifoNames[i]=calloc(MAXFIFONAME_SIZE,sizeof(char));

	fifoNames[SS_FINDEX_TOLOG] = strcat(fifoNames[SS_FINDEX_TOLOG], SS_FIFO_TO_LOG);
	fifoNames[SS_FINDEX_TOLOG] = strcat(fifoNames[SS_FINDEX_TOLOG], textId);
	fifoNames[SS_FINDEX_WTOS] = strcat(fifoNames[SS_FINDEX_WTOS], SS_FIFO_WINMAN_TO_SENDER);
	fifoNames[SS_FINDEX_WTOS] = strcat(fifoNames[SS_FINDEX_WTOS], textId);
	fifoNames[SS_FINDEX_RTOW] = strcat(fifoNames[SS_FINDEX_RTOW], SS_FIFO_RECVER_TO_WINMAN);
	fifoNames[SS_FINDEX_RTOW] = strcat(fifoNames[SS_FINDEX_RTOW], textId);
	fifoNames[SS_FINDEX_WTOT] = strcat(fifoNames[SS_FINDEX_WTOT], SS_FIFO_WINMAN_TO_TIMMAN);
	fifoNames[SS_FINDEX_WTOT] = strcat(fifoNames[SS_FINDEX_WTOT], textId);
	fifoNames[SS_FINDEX_TTOW] = strcat(fifoNames[SS_FINDEX_TTOW], SS_FIFO_TIMMAN_TO_WINMAN);
	fifoNames[SS_FINDEX_TTOW] = strcat(fifoNames[SS_FINDEX_TTOW], textId);
	fifoNames[SS_FINDEX_WTOR] = strcat(fifoNames[SS_FINDEX_WTOR], SS_FIFO_WINMAN_TO_RECVER);
	fifoNames[SS_FINDEX_WTOR] = strcat(fifoNames[SS_FINDEX_WTOR], textId);

	mkdir("tmp",0777);
	if(mkfifo( fifoNames[SS_FINDEX_TOLOG], 0777)) {
		if(errno!=EEXIST) { perror("SS-Sender: Cannot create FIFO_TO_LOG:"); return 2; } }
	if(mkfifo( fifoNames[SS_FINDEX_WTOS], 0777)) {
		if(errno!=EEXIST) { perror("SS-Sender: Cannot create FIFO_WINMAN_TO_SENDER:"); return 2; } }
	if(mkfifo( fifoNames[SS_FINDEX_WTOT], 0777)) {
		if(errno!=EEXIST) { perror("SS-Sender: Cannot create FIFO_WINMAN_TO_TIMMAN:"); return 2; } }
	if(mkfifo( fifoNames[SS_FINDEX_TTOW], 0777)) {
		if(errno!=EEXIST) { perror("SS-Sender: Cannot create FIFO_TIMMAN_TO_WINMAN:"); return 2; } }
	if(mkfifo( fifoNames[SS_FINDEX_RTOW], 0777)) {
		if(errno!=EEXIST) { perror("SS-Sender: Cannot create FIFO_RECVER_TO_WINMAN:"); return 2; } }
	if(mkfifo( fifoNames[SS_FINDEX_WTOR], 0777)) {
		if(errno!=EEXIST) { perror("SS-Sender: Cannot create FIFO_WINMAN_TO_RECVER:"); return 2; } }

	argsThreads.fnames = &(fifoNames[0]);

	//printf("RUDPsend of \"%s\" Start Now\n",FileName);

/* Launching Threads */
	pthread_t t_s_windowManager, t_s_sender, t_s_recver, t_s_timerManager, t_s_logManager;
	pthread_create (&t_s_windowManager,NULL, (void *) f_s_windowManager,  (void *)(&argsThreads) );
	pthread_create (&t_s_sender, 			 NULL, (void *) f_s_sender, 				(void *)(&argsThreads) );
	pthread_create (&t_s_recver, 			 NULL, (void *) f_s_recver, 				(void *)(&argsThreads) );
	pthread_create (&t_s_timerManager, NULL, (void *) f_s_timerManager, 	(void *)(&argsThreads) );
	pthread_create (&t_s_logManager, 	 NULL, (void *) f_s_logManager, 		(void *)(&argsThreads) );

/* waiting... */
	int * retVal[5];
	for(i=0;i<5;i++) retVal[i]=malloc(sizeof(int));
	pthread_join( t_s_sender, 			(void **) &retVal[0]);
	pthread_join( t_s_recver, 			(void **) &retVal[1]);
	pthread_join( t_s_timerManager, (void **) &retVal[2]);
	pthread_join( t_s_logManager, 	(void **) &retVal[3]);
	pthread_join( t_s_windowManager,(void **) &retVal[4]);
	int retValue = 0;
	for(i=0;i<5;i++) if(*((int*)retVal[i])!=0) retValue = *(retVal[i]);

	return retValue;
}

int RUDP_Send(int sockServe, struct sockaddr_in * addrConnClient, char * filePath, int fifo_TOlog, int whoIam) {

	char * buf_LOG = calloc(LOG_SIZE,sizeof(char));
	char * cmdPkt  = calloc(CMDPKT_SIZE,sizeof(char));
	int cantAccess = 0;

	cantAccess = access(filePath, F_OK);
	if(!cantAccess) {

		/* * * * * * * * * * * * * * * * *
		 * Start the Transfer Session    *
		 * * * * * * * * * * * * * * * * */
		// log...
		if(fifo_TOlog) {
			snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> Start Transfer Send Session\n",getpid());
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
		}

		/* Get Parameters from Files *
		 *   WindowSize              *
		 *   IntervalSendTime        */
		int WindowSize;
		char *AuxString = malloc(10*sizeof(char));
		double lossProb;
		FILE * param;
		int IntSendTime;
		char * windowSizePath = calloc(100,sizeof(char));
		char * intervalTimePath = calloc(100,sizeof(char));
		char * lossProbPath = calloc(100,sizeof(char));
		if(whoIam == IAMCLIENT) { 
			strncpy(windowSizePath, "ClientParams/WindowSize.param", 100);
			strncpy(intervalTimePath, "ClientParams/IntervalSendTime.param", 100);
			strncpy(lossProbPath, "ClientParams/LossProb.param", 100);
		}
		if(whoIam == IAMSERVER) {
			strncpy(windowSizePath, "ServerParams/WindowSize.param", 100);
			strncpy(intervalTimePath, "ServerParams/IntervalSendTime.param", 100);
			strncpy(lossProbPath, "ServerParams/LossProb.param", 100);
		}
		if( !(param=fopen(windowSizePath,"r"))){
			WindowSize = TRANSFERWINDOW_SIZE;
			printf("Error in Open WindowSize.param, setted as Default: %d\n", WindowSize); fflush(stdout);
		} else {
			memset(AuxString, 0, 10);
			fgets(AuxString, 10, param);
			WindowSize = atoi(AuxString);
		}
		if( !(param=fopen(intervalTimePath,"r"))){
			IntSendTime = INTERVALSENDTIME;
			printf("Error in Open IntervalSendTime.param, setted as Default: %d", IntSendTime); fflush(stdout);
		} else {
			memset(AuxString, 0, 10);
			fgets(AuxString, 10, param);
			IntSendTime = atoi(AuxString);
		}
		if( !(param=fopen(lossProbPath,"r"))){
			printf("Error in Open LossProb.param, setted as Default"); fflush(stdout);
			lossProb = LOSS_PROB;
		} else {
			memset(AuxString, 0, 10);
			fgets(AuxString, 10, param);
			lossProb = atof(AuxString);
		}

		/* Inizialize and Bind new Transf Socket with new Port */
		struct sockaddr_in * addrTrasfSender = malloc(sizeof(struct sockaddr_in));
		memset((void *)addrTrasfSender, 0, sizeof(*addrTrasfSender));
		addrTrasfSender->sin_family = AF_INET;
		addrTrasfSender->sin_addr.s_addr = htonl(INADDR_ANY);
		socklen_t lenTrasf = sizeof(*addrTrasfSender);		
		int sockTrasf;
		if ( (sockTrasf = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { perror("RUDP: Error in socket TRANSFER SOCKET:"); return 2; }
		if ( bind(sockTrasf, (struct sockaddr *)addrTrasfSender, sizeof(*addrTrasfSender)) < 0) { perror("RUDP: Error in bind TRANSFER SOCKET\n"); return 2; }

		// log...
		if(fifo_TOlog) {
			snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> RUDP: new Transfer Socket\n",getpid(), sockTrasf);
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
		}

		/* Build and Send [RDY ][fileSize] *
		 *    1st Transf Step              */
		memset(cmdPkt,0,CMDPKT_SIZE);
		strncpy(cmdPkt,"RDY ", CMDCMD_SIZE);
		int filesize = fileSize(filePath);
		memcpy(cmdPkt+4, &filesize, sizeof(int));
		//usleep(TIMEPOLL);
		if ( sendto (sockTrasf, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrConnClient, lenTrasf) < 0) { perror("RUDP: Error in sendto"); return 2; }

		/* Initialize Address for Transfer */
		struct sockaddr_in * addrTrasfRcver = malloc(sizeof(struct sockaddr_in));
		socklen_t lenTrasfrcv = sizeof(*addrTrasfRcver);
		/* Wait for [OK  ][] Packet */
		if ( recvfrom (sockTrasf, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrTrasfRcver, &lenTrasfrcv) < 0) {perror("RUDP: Error in recvfrom in 2nd Transf Step"); return 2; }

		if( strncmp(cmdPkt,"OK  ", CMDCMD_SIZE)==0) {
		/* OK, Received [OK  ][] Packet *
		 *     2nd Transf Step          */

			// log...
			if(fifo_TOlog) {
				snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> RUDP: Starting RUDP Transfer\n",getpid(), sockTrasf);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
			}
			usleep(TIMEPOLL);
			int ret = RUDP_Start_Send(filePath, sockTrasf, addrTrasfRcver, WindowSize, IntSendTime, lossProb);
			return ret;
		} else {
			if(DEBUG_ERROR>=L1_DEBUG) { printf("%d:%d: -> RUDP: Connection with Recver Failed \"%s\"\n", getpid(), sockTrasf, cmdPkt); fflush(stdout); return 2;}
		}
	}	else {
		usleep(TIMEPOLL);
		/* Build and Send [NOTF][] *
		 *    File not Found       *
		 *    Transfering Fail     */
		memset(cmdPkt,0,CMDPKT_SIZE);
		strncpy(cmdPkt,"NOTF", CMDCMD_SIZE);
		// log...
		if(fifo_TOlog) {
			snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> RUDP: File Not Found\n",getpid());
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
		}
		if ( sendto (sockServe, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrConnClient, sizeof(*addrConnClient)) < 0) { perror("Errore in sendto"); return 2; }
		return 1;
	}
}


int RUDP_PUT(int sockConn, struct  sockaddr_in * addrServeServer, char * path, char * fileRequested, int fifo_TOlog, int whoIam) {
	char * cmdPkt = malloc(CMDPKT_SIZE*sizeof(char));
	char * filePath = malloc(CMDTXT_SIZE*sizeof(char));
		/* Build and Send [PUT ][fileName] *
		 *    start Transfer steps         */
	memset(cmdPkt,0,CMDPKT_SIZE);
	strncpy(cmdPkt,"PUT ", CMDCMD_SIZE);
	strncat(cmdPkt, fileRequested, CMDTXT_SIZE);
	if ( sendto(sockConn, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrServeServer, sizeof(*addrServeServer)) < 0) {perror("RUDP: Error in SENDTO for CMD PKT"); return 2; }
	
	memset(filePath, 0, CMDTXT_SIZE);
	strcpy(filePath, path);
	strcat(filePath, fileRequested); // FolderPath+fileRequested

	usleep(TIMEPOLL);						
	int ret =0;
	ret = RUDP_Send(sockConn, addrServeServer, filePath, fifo_TOlog, whoIam);
	return ret;
}
