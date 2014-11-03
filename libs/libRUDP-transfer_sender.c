
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
#include <string.h>

#include "libRUDP-constants.h"
#include "libRUDP-transfer.h"
#include "listP.h"


int * f_s_sender(ss_threads_args * threadArgs) {

	/* Inizializing Structures */
	ss_threads_args thr_args = (ss_threads_args) *(threadArgs);
	int * nextSeq = calloc(1, sizeof(int));
	srand(time(NULL)); 
	double randomProb = 0.0;
	int * retVal = malloc(sizeof(int));
	*retVal = 0;

	char * buf_WINMAN = malloc(MSG_SIZE*sizeof(char));
	int nreadWINMAN=0;
	char * FLP_buffer = calloc(FLP_SIZE,sizeof(char));
	char * PKT_buffer = malloc(PKT_SIZE*sizeof(char));

	/* Sync Fifos */
	int fifo_winman_to_sender, fifo_TOlog, flags;
	/// Sync Fifo with Window Manager
	fifo_winman_to_sender = open( thr_args.fnames[SS_FINDEX_WTOS], O_RDONLY );
	if(fifo_winman_to_sender<0) { perror("SS-SENDER: Cannot open fifo_winman_to_sender"); *retVal = 2; return retVal; }
	flags = fcntl(fifo_winman_to_sender, F_GETFL); fcntl(fifo_winman_to_sender, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
	/// Sync Fifo with Log Thread
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( thr_args.fnames[SS_FINDEX_TOLOG], O_WRONLY );
	if(fifo_TOlog<0) { perror("SS-SENDER: Cannot open fifo_TOlog"); *retVal = 2; return retVal; }

	/* Open File to Send */
	FILE * toSendFile = fopen(thr_args.fileName, "r");
	if(toSendFile<0) { perror("SS-SENDER: Cannot Open file to send"); *retVal = 2; return retVal;}
	rewind(toSendFile);

	/*...*/
	if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "Sender: Ready\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log	
	if(DEBUG_SENDER>=L1_DEBUG) { printf("> Sender: Ready\n"); fflush(stdout); }
	int WORKING =1;

	/* Start Routine */
	while(WORKING) {
		// Read nextSeq from Window Manager
		nreadWINMAN = read( fifo_winman_to_sender, buf_WINMAN, MSG_SIZE);
		if(nreadWINMAN<0 && errno!=(EAGAIN)) { 
			perror("SS-SENDER: Read Error in fifo_winman_to_sender"); *retVal = 2; return retVal; 
		} else if(nreadWINMAN>0) {
			if( buf_WINMAN[0]=='N' && 
					buf_WINMAN[1]=='X' &&
					buf_WINMAN[2]=='T' ) {
				// OK, i have [NXT] packet to send!
				memcpy(nextSeq, buf_WINMAN+3, sizeof(int));
				// Read FLP from toSendFile
				fseek(toSendFile,(long) ((*nextSeq)*FLP_SIZE), SEEK_SET);	
				fread(FLP_buffer, 1, FLP_SIZE, toSendFile); 
				// Add Header to toSendFile
				PKT_buffer = addHeader (PKT_buffer, nextSeq, FLP_buffer, HDR_SIZE, FLP_SIZE);

				//Send Packet
				randomProb = (double)(((double)rand())/(double)(RAND_MAX));
				// Here the random decision to loss packet
				if(randomProb<thr_args.lossProb) {
				 	if (sendto(thr_args.sockfd, PKT_buffer, PKT_SIZE, 0, (struct sockaddr *)(thr_args.addrRecver), (thr_args.addrRecverSize) ) < 0) { perror("SENDER: Error in sendto"); *retVal = 2; return retVal; }
					if(LOG_TO_FILE>=L2_LOG) { snprintf(buf_LOG, LOG_SIZE, "Sender: send new Packet >%d<\n", *nextSeq); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
					if(DEBUG_SENDER>=L2_DEBUG) { printf("> Sender: send new Packet >%d<\n", *nextSeq);fflush(stdout); }
				} else {
					if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "Sender: I don't send Packet >%d<\n", *nextSeq); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log			
					if(DEBUG_SENDER>=L1_DEBUG) { printf("> Sender: I don't send Packet  >%d<\n", *nextSeq);fflush(stdout); }
				}
			} else if ( buf_WINMAN[0]=='Q' &&
									buf_WINMAN[1]=='U' &&
									buf_WINMAN[2]=='I' &&
									buf_WINMAN[3]=='T' ) {
				// OK, is the [QUIT] Notification
				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "Sender: Quitting...\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_SENDER>=L1_DEBUG) { printf("> Sender: Quitting...\n");fflush(stdout); }
				WORKING = 0;
			} 
			else {
				// No, this is a not-well formatted packet, rejected
				if(DEBUG_ERROR) { printf("> ERR - Sender: Received something wrong from window manager %d:%s\n",nreadWINMAN, buf_WINMAN);fflush(stdout); }
				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "ERR - Sender: Received something wrong from window manager %d:<%s>\n",nreadWINMAN, buf_WINMAN); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log			
			}
		} else if(errno==EAGAIN) {
			// NO Message Arrived
		} else {
			perror("SS-SENDER: fifo_winman_to_sender Closed"); *retVal = 2; return retVal;
		}

	// end working cycle here
	}
	close(fifo_TOlog);
	close(fifo_winman_to_sender);
	return retVal;
}

int * f_s_recver(ss_threads_args * threadArgs) {

	/* Inizialize Strctures */
	ss_threads_args thr_args = (ss_threads_args) *(threadArgs);
	char * buf_ACK = malloc (ACK_SIZE*sizeof(char));
	char * buf_WINMAN = malloc(MSG_SIZE*sizeof(char));
	int seq=0;
	int nreadSOCKFD=0;
	int nreadWINMAN=0;
	int * retVal = malloc(sizeof(int));
	*retVal = 0;

	/* Sync Fifos */
	int fifo_recver_to_winman, fifo_TOlog, fifo_winman_to_recver, flags;
	/// Sync Fifo with Window Manager
	fifo_recver_to_winman = open( thr_args.fnames[SS_FINDEX_RTOW], O_WRONLY );
	if(fifo_recver_to_winman<0) { perror("SS-RECVER: Cannot open fifo_recver_to_winman"); *retVal = 2; return retVal; }
	/// Sync Fifo with Window Manager, Non Blocking Fifo
	fifo_winman_to_recver = open( thr_args.fnames[SS_FINDEX_WTOR], O_RDONLY );
	if(fifo_winman_to_recver<0) { perror("SS-RECVER: Cannot open fifo_winman_to_recver"); *retVal = 2; return retVal; }
	flags = fcntl(fifo_winman_to_recver, F_GETFL); fcntl(fifo_winman_to_recver, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
	/// Sync Fifo with Log Thread
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( thr_args.fnames[SS_FINDEX_TOLOG], O_WRONLY );
	if(fifo_TOlog<0) { perror("SS-RECVER: Cannot open fifo_TOlog"); *retVal = 2; return retVal; }

	/*...*/
	flags = fcntl(thr_args.sockfd, F_GETFL);
	fcntl(thr_args.sockfd, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
	if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "Recver: Ready\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
	if(DEBUG_SENDER>=L1_DEBUG) { printf("> Recver: Ready\n");fflush(stdout); }
	int WORKING = 1;

	/* Start Routine */
	while(WORKING) {
//		usleep(TIMEPOLL);
		
		/* Check for new Ack */
		nreadSOCKFD = recvfrom (thr_args.sockfd, buf_ACK, ACK_SIZE, 0, NULL, NULL);
		if ( nreadSOCKFD<0 && errno!=EAGAIN) { 
			perror("SS-RECVER: Read Error in sockfd"); *retVal = 2; return retVal; 
		}	else if (nreadSOCKFD>0) {
			if ( buf_ACK[0]=='A' &&
					 buf_ACK[1]=='C' &&
					 buf_ACK[2]=='K' ) {
				// OK, i recived a new [ACK]
				memcpy(&seq, buf_ACK+3, sizeof(int)); // duplocate? is for debug log
				if(LOG_TO_FILE>=L2_LOG) { snprintf(buf_LOG, LOG_SIZE, "Recver: Ack Recved <%d>\n", seq); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_SENDER>=L2_DEBUG) { printf("> Recver: Ack Recved <%d>\n", seq);fflush(stdout); }
				write( fifo_recver_to_winman, buf_ACK, MSG_SIZE);
			}	else {
				// No, the packet is not well formatted, rejected
				if(DEBUG_ERROR) { printf("> ERR - Recver: Recved something wrong from socket! \"%s\" ", buf_ACK); fflush(stdout); }
				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "> ERR - Recver: Recved something wrong from socket!! \n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
			}
		} else if(errno==EAGAIN) {
			// NO Packet Arrived

			/* Check for Window Manager Comunication */
			nreadWINMAN = read(fifo_winman_to_recver, buf_WINMAN, MSG_SIZE);
			if(nreadWINMAN<0 && errno!=(EAGAIN)) {
				perror("SS-WINMAN: Read Error in fifo_winman_to_recver"); *retVal = 2; return retVal;
			} else if(nreadWINMAN>0) {
				if( buf_WINMAN[0]=='Q' && 
						buf_WINMAN[1]=='U' && 
						buf_WINMAN[2]=='I' && 
						buf_WINMAN[3]=='T' ) {
					// OK, is a [QUIT] Packet
					if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "Recver: Quitting...\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
					if(DEBUG_SENDER>=L1_DEBUG) { printf("> Recver: Quitting...\n", seq);fflush(stdout); }
					WORKING = 0;
				} else {
					// NO, is a not well formatted message
					if(DEBUG_ERROR) { printf("> ERR - RECVER: Received something wrong from winman %s\n", buf_WINMAN);fflush(stdout); }
					if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "ERR - Recver: Received something wrong from winman \"%s\"\n", buf_WINMAN); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				}
			} else if(nreadWINMAN==0) {
				perror("SS-RECVER: fifo_winman_to_recver Closed"); *retVal = 2; return retVal;			
			}	else {
				// NO Packet Arrived
			}

		} else {
			perror("SS-RECVER: sockfd Closed"); *retVal = 2; return retVal;
		}
 
	// end working cycle here
	}
	close(fifo_TOlog);
	close(fifo_recver_to_winman);
	close(fifo_winman_to_recver);
	return retVal;
}

int * f_s_logManager(ss_threads_args * threadArgs) {

	/* Inizialize Strctures and Files */
	int * retVal = malloc(sizeof(int));
	*retVal = 0;
	ss_threads_args thr_args = (ss_threads_args) *(threadArgs);
	char * logFilePath = calloc(LOG_SIZE,sizeof(char));
	char * counttime = calloc(LOG_SIZE,sizeof(char));
	char * AuxString = calloc(10,sizeof(char));
	time_t timer;
	timer=time(NULL);
	struct tm * t;
	t = localtime(&timer);
	strftime(counttime, LOG_SIZE, "%y-%m-%d|%H-%M-%S--", t);
	strncat(logFilePath, "log/", LOG_SIZE);
	strncat(logFilePath, counttime, LOG_SIZE);
	snprintf(AuxString, 10, "%d", getpid());
	strncat(logFilePath, AuxString, LOG_SIZE);
	strncat(logFilePath, "-RUDP_transfer_sender.log", LOG_SIZE);
	FILE * logSendFile = fopen(logFilePath, "a");
	if(logSendFile<0) { perror("SS-LOG: Cannot open log_send_sender.log"); *retVal = 2; return retVal;}

	/* Sync Fifos */
	int fifo_TOlog;
	/// Sync Fifo with Log Thread
	int nreadFIFO=0;
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( thr_args.fnames[SS_FINDEX_TOLOG], O_RDONLY );
	if(fifo_TOlog<0) { perror("SS-LOG: Cannot open fifo_TOlog"); *retVal = 2; return retVal; }

	/*...*/
	// Set Title
	fputs("\n---NEW--EXECUTION: ", logSendFile); fputs(asctime(localtime(&timer)), logSendFile); fputs("\n", logSendFile);
	if(DEBUG_SENDER>=L1_DEBUG) { printf("> LogManager: Ready\n");fflush(stdout); }
	int WORKING = 1;

	/* Start Routine */
	while(WORKING) {

		nreadFIFO = read(fifo_TOlog, buf_LOG, LOG_SIZE);
		if(nreadFIFO<0 && errno!=EAGAIN) {
			perror("SS-LOGMAN: Read Error in fifo_TOlog"); *retVal = 2; return retVal;
		} else if(nreadFIFO>0) {
			if( buf_LOG[0]=='Q' && 
					buf_LOG[1]=='U' && 
					buf_LOG[2]=='I' && 
					buf_LOG[3]=='T') {
				// OK, is a [QUIT] Notification
				WORKING = 0;
				if(DEBUG_SENDER>=L1_DEBUG) { printf("> LogMan: Quitting...\n");fflush(stdout); }
			} else {
				// OK, I Suppose is a Well Formatted Readble String which ends with '\0'
				timer=time(NULL);
				t = localtime(&timer);
				memset(counttime, 0, LOG_SIZE);
				strftime(counttime, LOG_SIZE, "%x-%X ", t);
				fputs(counttime, logSendFile);
				fputs(buf_LOG, logSendFile);
				fflush(logSendFile);
			}
		} else if(errno==EAGAIN) {
			// NO Message Arrived
		} else {
			perror("SS-LOGMAN: fifo_TOlog Closed"); *retVal = 2; return retVal;
		}

	// end working cycle here
	}
	fclose(logSendFile);
	close(fifo_TOlog);
	return retVal;
}

int * f_s_timerManager(ss_threads_args * threadArgs) {

	/* Inizialize Strctures */
	ss_threads_args thr_args = (ss_threads_args) *(threadArgs);
	struct timeval *tp=malloc(sizeof(struct timeval));
	my_list_p * list = createListP(list);
	int numSeq=0;
	char * buf_FMwinman = malloc(MSG_SIZE*sizeof(char));
	char * buf_TOwinman = malloc(MSG_SIZE*sizeof(char));
	buf_TOwinman[0]='O'; 
	buf_TOwinman[1]='U'; 
	buf_TOwinman[2]='T';
	int nreadWINMAN=0;
	int * retVal = malloc(sizeof(int));
	*retVal = 0;
	int timeout = TIMEOUT;

	/* Sync Fifos */
	int fifo_winman_to_timman, fifo_timman_to_winman, fifo_TOlog, flags;
	/// Sync Fifo with Window Manager
	//// Fifo From Window Manager, is a NON-BLOCKING Fifo
	fifo_winman_to_timman = open( thr_args.fnames[SS_FINDEX_WTOT], O_RDONLY );
	if(fifo_winman_to_timman<0) { perror("SS-TIMMAN: Cannot open fifo_winman_to_timman"); *retVal = 2; return retVal; }
	flags = fcntl(fifo_winman_to_timman, F_GETFL); fcntl(fifo_winman_to_timman, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
	//// Fifo To Window Manager
	fifo_timman_to_winman = open( thr_args.fnames[SS_FINDEX_TTOW], O_WRONLY );
	if(fifo_timman_to_winman<0) { perror("SS-TIMMAN: Cannot open fifo_timman_to_winman"); *retVal = 2; return retVal; }
	/// Sync Fifo with Log Thread
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( thr_args.fnames[SS_FINDEX_TOLOG], O_WRONLY );
	if(fifo_TOlog<0) { perror("SS-TIMMAN: Cannot open fifo_TOlog"); *retVal = 2; return retVal; }

	/*...*/
	gettimeofday(tp,NULL);
	long secondStart = (tp->tv_sec);
	long actualTime = (tp->tv_sec);

	if(LOG_TO_FILE>=L2_LOG) { snprintf(buf_LOG, LOG_SIZE, "Timer Manager: Ready\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
	if(DEBUG_SENDER>=L2_DEBUG) { printf("> Timer Manager: Ready\n");fflush(stdout); }
	int WORKING = 1;

	/* Start Routine */
	while(WORKING) {
	//	usleep(TIMEPOLL);
		gettimeofday(tp,NULL);

			// 2 Main Routines:
			// > Check for new or ack timer
			// > Check for timeout

		/* new Timer or Ack Routine */
		nreadWINMAN = read(fifo_winman_to_timman, buf_FMwinman, MSG_SIZE);
		if( nreadWINMAN<0 && errno!=EAGAIN) {
			perror("SS-TIMMAN: Read Error fifo_winman_to_timman"); *retVal = 2; return retVal;
		} else if(nreadWINMAN>0) {
			// New Message Received from Window Manager

			if( buf_FMwinman[0]=='N' && 
					buf_FMwinman[1]=='E' &&
					buf_FMwinman[2]=='W' ) {
				// OK, is a [NEW] timer Notification
				memcpy(&numSeq, (buf_FMwinman+3), sizeof(int));
				gettimeofday(tp,NULL);
				actualTime=(((tp->tv_sec)-secondStart)*1000000)+(tp->tv_usec);
				appendP(list, numSeq, actualTime+timeout);
				if(LOG_TO_FILE>=L2_LOG) { snprintf(buf_LOG,LOG_SIZE, "TimMan: new Timer Arrived >%d<\n", numSeq); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_SENDER>=L2_DEBUG) { printf("> TimerManager: new Timer Arrived %d:>%d<\n",nreadWINMAN, numSeq); fflush(stdout); }

			} else if( buf_FMwinman[0]=='A' && 
								 buf_FMwinman[1]=='C' &&
								 buf_FMwinman[2]=='K' ) {
				// OK, is an [ACK] timer Notification
				memcpy(&numSeq, (buf_FMwinman+3), sizeof(int));
				delNodeP(list,numSeq);

				if(LOG_TO_FILE>=L2_LOG) { snprintf(buf_LOG,LOG_SIZE, "TimMan: new Ack Arrived >%d<\n", numSeq); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_SENDER>=L2_DEBUG) { printf("> TimerManager: new Ack Arrived %d:>%d<\n",nreadWINMAN, numSeq); fflush(stdout); } 

			} else if( buf_FMwinman[0]=='D' && 
								 buf_FMwinman[1]=='E' &&
								 buf_FMwinman[2]=='C' ) {
				// OK, is an [DEC] timer Notification
				if(timeout>thr_args.intSendTime) {
					timeout-=(thr_args.intSendTime)/4;
					if(LOG_TO_FILE>=L2_LOG) { snprintf(buf_LOG,LOG_SIZE, "TimMan: new Decrementation Arrived >%d<\n", timeout); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
					if(DEBUG_SENDER>=L2_DEBUG) { printf("> TimerManager: new Decrementation Arrived >%d<\n", timeout); fflush(stdout); } 
				}

			} else if( buf_FMwinman[0]=='I' && 
								 buf_FMwinman[1]=='N' &&
								 buf_FMwinman[2]=='C' ) {
				// OK, is an [INC] timer Notification
				timeout+=(thr_args.intSendTime)/3;
				if(LOG_TO_FILE>=L2_LOG) { snprintf(buf_LOG,LOG_SIZE, "TimMan: new Incrementation Arrived >%d<\n", timeout); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_SENDER>=L2_DEBUG) { printf("> TimerManager: new Incrementation Arrived >%d<\n", timeout); fflush(stdout); } 

			} else if( buf_FMwinman[0]=='Q' && 
								 buf_FMwinman[1]=='U' &&
								 buf_FMwinman[2]=='I' &&
								 buf_FMwinman[3]=='T' ) {
				// OK, is a [QUIT] Notification
				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "TimMan: Quitting...\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_SENDER>=L1_DEBUG) { printf("> TimMan: Quitting...\n");fflush(stdout); }
				WORKING = 0;

			} else {
				printf("> ERR - TimerManager: Received something wrong!!!\n");fflush(stdout);
			}
		} else if(errno==EAGAIN) {
			// NO Notification Arrived
			/* check Timeout Expiry */
			if(WORKING) {
				gettimeofday(tp,NULL);
				actualTime=(((tp->tv_sec)-secondStart)*1000000)+(tp->tv_usec);
				numSeq = checkTimer(list, actualTime);
				if( numSeq != (-1) ){
					// new Timeout!!
					memcpy(buf_TOwinman+3, &numSeq, sizeof(int));
					write( fifo_timman_to_winman, buf_TOwinman, MSG_SIZE);
					if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG,LOG_SIZE, "TimMan: Timeout!! >%d<\n", numSeq); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
					if(DEBUG_SENDER>=L1_DEBUG) { printf("> TimerManager: Timeout!! >%d<\n", numSeq); fflush(stdout); } 
				} else {
					// No Timer Expired
				}
			}
		} else {
			perror("SS-TIMMAN: fifo_winman_to_timman Closed");*retVal = 2; return retVal;			
		}
		
	// end working cycle here
	}
	close(fifo_TOlog);
	close(fifo_winman_to_timman);
	close(fifo_timman_to_winman);
	return retVal;
}

int * f_s_windowManager(ss_threads_args * threadArgs) {

	/* Inizialize Strctures */
	ss_threads_args thr_args = (ss_threads_args) *(threadArgs);
	int TOT_FLP = (thr_args.fileSize/FLP_SIZE)+1; // Total Packets Number to Send
	TransferWindow * t_window= new_Window(t_window, TOT_FLP, 0, thr_args.windowSize);
	int nextSEQ = 0;
	int recvSEQ = 0;
	int exprSEQ = 0;
	int i = 0; // Auxiliary variable
	int posMoved=0;
	int * retVal = malloc(sizeof(int));
	*retVal = 0;
	int timeouti = 0;
	int timeoutd = 0;
	int totSleep=0;
	int acked = 0;
	double meanTry=0.0;
	int count=0;

	char * buf_NXT = malloc(MSG_SIZE*sizeof(char));
	buf_NXT[0]='N'; 
	buf_NXT[1]='X';
	buf_NXT[2]='T';
	char * buf_NEW = malloc(MSG_SIZE*sizeof(char));
	buf_NEW[0]='N'; 
	buf_NEW[1]='E';
	buf_NEW[2]='W';
	char * buf_ACK = malloc(MSG_SIZE*sizeof(char));
	buf_ACK[0]='A'; 
	buf_ACK[1]='C';
	buf_ACK[2]='K';
	char * buf_INC = calloc(MSG_SIZE,sizeof(char));
	buf_INC[0]='I'; 
	buf_INC[1]='N';
	buf_INC[2]='C';
	char * buf_DEC = calloc(MSG_SIZE,sizeof(char));
	buf_DEC[0]='D'; 
	buf_DEC[1]='E';
	buf_DEC[2]='C';
	char * buf_RECVER = malloc(MSG_SIZE*sizeof(char));
	char * buf_TIMMAN = malloc(MSG_SIZE*sizeof(char));
	char * win_STRING = malloc(LOG_SIZE*sizeof(char));
	char * win_AUX = malloc(LOG_SIZE*sizeof(char));
	int nreadRECVER=0;	
	int nreadTIMER=0;
	*retVal = 0;
	useconds_t intervalTime = thr_args.intSendTime;

	/* Sync Fifos */
	int fifo_winman_to_sender, fifo_winman_to_timman, fifo_recver_to_winman, fifo_winman_to_recver, fifo_timman_to_winman, fifo_TOlog, flags;
	/// Sync Fifo with Log Thread
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( thr_args.fnames[SS_FINDEX_TOLOG], O_WRONLY );
	if(fifo_TOlog<0) { perror("SS-WINMAN: Cannot open fifo_TOlog"); *retVal = 2; return retVal; }
	/// Sync Fifo with Sender
	fifo_winman_to_sender = open( thr_args.fnames[SS_FINDEX_WTOS], O_WRONLY );
	if(fifo_winman_to_sender<0) { perror("SS-WINMAN: Cannot open fifo_winman_to_sender"); *retVal = 2; return retVal; }
	/// Sync Fifo with Recver
	//// Fifo From Recver, is a Not-Blocking Fifo!!
	fifo_recver_to_winman = open( thr_args.fnames[SS_FINDEX_RTOW], O_RDONLY );
	if(fifo_recver_to_winman<0) { perror("SS-WINMAN: Cannot open fifo_recver_to_winman"); *retVal = 2; return retVal; }
	flags = fcntl(fifo_recver_to_winman, F_GETFL); fcntl(fifo_recver_to_winman, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
	/// Fifo To Recver
	fifo_winman_to_recver = open( thr_args.fnames[SS_FINDEX_WTOR], O_WRONLY );
	if(fifo_winman_to_recver<0) { perror("SS-WINMAN: Cannot open fifo_winman_to_recver"); *retVal = 2; return retVal; }
	/// Sync Fifo with Timer Manager
	//// Fifo To Timer Manager
	fifo_winman_to_timman = open( thr_args.fnames[SS_FINDEX_WTOT], O_WRONLY);
	if(fifo_winman_to_timman<0) { perror("SS-WINMAN: Cannot open fifo_winman_to_timman"); *retVal = 2; return retVal; }
	//// Fifo From Timer Manager, is a Not-Blocking Fifo!!
	fifo_timman_to_winman = open(thr_args.fnames[SS_FINDEX_TTOW], O_RDONLY );
	if(fifo_timman_to_winman<0) { perror("SS-WINMAN: Cannot open fifo_timman_to_winman"); *retVal = 2; return retVal; }
	flags = fcntl(fifo_timman_to_winman, F_GETFL); fcntl(fifo_timman_to_winman, F_SETFL, flags | O_NONBLOCK | O_NDELAY);

	/*...*/
	if(LOG_TO_FILE>=L1_LOG) write(fifo_TOlog, "WinMan: Inizialized, Starting routine\n", LOG_SIZE ); // Log
	if(DEBUG_SENDER>=L1_DEBUG) { printf("> WinMan: Inizialized, Starting routine\n");fflush(stdout); }
	int WORKING = 1;

	/* Start Routine Window Manager */
	while(WORKING) {
		usleep(TIMEPOLL); // Interval between routines
		totSleep+=TIMEPOLL;

		// Log Transfer Window
		if(LOG_TO_FILE>=L2_DEBUG) { 
			memset(win_STRING,0,LOG_SIZE);
			memset(win_AUX,0,LOG_SIZE);
			int aux=0;
			for(i=0; i<t_window->TOT_FLP; i++) {
				if(i == t_window->WIN_START) {
					snprintf(win_AUX, LOG_SIZE, "window: %d [ %d(%d)",aux, t_window->PKTS_TRY[i], t_window->PKTS_STATE[i] );
					strncat(win_STRING,win_AUX,LOG_SIZE); 
					memset(win_AUX,0,LOG_SIZE);}
				else if( (i>t_window->WIN_START) && (i < (t_window->WIN_START+t_window->WIN_SIZE))) { 
					snprintf(win_AUX, LOG_SIZE, "- %d(%d)", t_window->PKTS_TRY[i], t_window->PKTS_STATE[i] );
					strncat(win_STRING,win_AUX,LOG_SIZE); 
					memset(win_AUX,0,LOG_SIZE); }
				else if(i == (t_window->WIN_START+t_window->WIN_SIZE)) { 
					snprintf(win_AUX, LOG_SIZE, "] %d", TOT_FLP-t_window->WIN_SIZE-aux );
					strncat(win_STRING,win_AUX,LOG_SIZE); 
					memset(win_AUX,0,LOG_SIZE); }
				else { 
					aux++; }
			}
			snprintf(win_AUX, LOG_SIZE, "\n");	 fflush(stdout);
			strncat(win_STRING,win_AUX,LOG_SIZE); 
			memset(win_AUX,0,LOG_SIZE);
			write(fifo_TOlog, win_STRING, LOG_SIZE ); // Log
		}

		/* 
 		 * Consist in 3 Main Routine:
		 *  > Send next avaiable packet
		 *	> Check if received any new acks
		 *	> Check for timeout
		 */
	
		/* Next Send Routine */
		if(WORKING && totSleep>=intervalTime) {
			totSleep=0;
			// Find and Send the Next Sequence Number to Sender, if there is!
			nextSEQ=FindNextToSendInWindow(t_window);
			if(nextSEQ>=0){
				// send [NXT] to Sender
				memcpy(buf_NXT+3, &nextSEQ, sizeof(int));
				write( fifo_winman_to_sender, buf_NXT, MSG_SIZE);
				// Notificate [NEW] to Timer Manager
				memcpy(buf_NEW+3, &nextSEQ, sizeof(int));
				write( fifo_winman_to_timman, buf_NEW, MSG_SIZE);

				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "WinMan: Send FLP >%d<\n", nextSEQ); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_SENDER>=L1_DEBUG) { printf("> WindowManager: new Send: %d\n", nextSEQ); fflush(stdout); }

			} else {
				// No Packet to Send
				// Maybe WINSIZE too short, or TIMEOUT too long,
				// Notificate [INC] to Timer Manager
				
				meanTry = MeanSendTry(t_window);
				if(meanTry<1.0/thr_args.lossProb) {
					timeouti--;
					if(timeouti<(-40)) {
						timeouti=0;
						timeoutd=0;
						write( fifo_winman_to_timman, buf_DEC, MSG_SIZE);
					}
				}
			

				if(LOG_TO_FILE>=L2_LOG) { snprintf(buf_LOG,LOG_SIZE,  "WinMan: No PKT Avaiable to send\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_SENDER>=L2_DEBUG) { printf("> WindowManager: no PKT Avaible to send\n"); fflush(stdout); }
			}
		}
	
		/* Check Receiver Routine */
		if(WORKING) {
			nreadRECVER = read( fifo_recver_to_winman, buf_RECVER, MSG_SIZE);
			if(nreadRECVER<0 && errno!=(EAGAIN)) {
				perror("SS-WINMAN: Read Error in fifo_recver_to_winman"); *retVal = 2; return retVal;
			} else if(nreadRECVER>0) {
				if( buf_RECVER[0]=='A' && 
						buf_RECVER[1]=='C' && 
						buf_RECVER[2]=='K' ) {
					// OK, is an [ACK] notification
					acked++;
					memcpy(&recvSEQ, buf_RECVER+3, sizeof(int));
					posMoved=moveWindow(t_window, recvSEQ, IAMSENDER);
					if(posMoved==(-1)) {
						timeoutd++;
						if(timeoutd>5) {
							write( fifo_winman_to_timman, buf_INC, MSG_SIZE);
							timeoutd=0;
							timeouti=0;
						}
					} else {
						acked-=posMoved;
					}

					if(DEBUG_SENDER>=L1_DEBUG) { printf("> WindowManager: new Ack recived <%d>, i move window in <%d>, pktsStock <%d>\n",recvSEQ, t_window->WIN_START, t_window->PKTS_STOCK);fflush(stdout); }
					if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "WinMan: new Ack recived <%d>, i move window in <%d>, pktsStock <%d>, move->%d\n",recvSEQ, t_window->WIN_START, t_window->PKTS_STOCK, posMoved); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
					// Notificate the Timer Manager
					memcpy(buf_ACK+3, &recvSEQ, sizeof(int));
					write( fifo_winman_to_timman, buf_ACK, MSG_SIZE);
					if(t_window->PKTS_STOCK == 0) {
						/* Start Closing Procedure ! */
						if(DEBUG_SENDER>=L1_DEBUG) { printf("> WindowManager: File Transfert Successfully\n"); fflush(stdout); }
						if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "WinMan: File transfer completed successfully\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
						WORKING=0;
					}
				} else {
					// NO, is a not well formatted message
					if(DEBUG_SENDER>=L1_DEBUG) { printf("> ERR - WindowManager: Received something wrong from recver %s\n", buf_RECVER);fflush(stdout); }
					if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "ERR - WinMan: Received something wrong from recver \"%s\"\n", buf_RECVER); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				}
			} else if(nreadRECVER==0) {
				perror("SS-WINMAN: fifo_recver_to_winman Closed"); *retVal = 2; return retVal;			
			}	else {
				// NO Ack Arrived
			} 
		}

		/* Check Timer Routine */
		if(WORKING) {
			nreadTIMER = read( fifo_timman_to_winman, buf_TIMMAN, MSG_SIZE);
			if(nreadTIMER<0 && errno!=(EAGAIN)) {
				perror("SS-WINMAN: Read Error in fifo_timman_to_winman"); *retVal = 2; return retVal;
			} else if(nreadTIMER>0) {
				if( buf_TIMMAN[0]=='O' && 
						buf_TIMMAN[1]=='U' && 
						buf_TIMMAN[2]=='T' ) {
					// OK, is a TIME[OUT] notification
					memcpy(&exprSEQ, (buf_TIMMAN+3), sizeof(int));
					// set packet <exprSEQ> to send!
					posMoved=resendIt(t_window, exprSEQ);
					if(posMoved==(-1)) {
						timeoutd++;
						if(timeoutd>5) {
							write( fifo_winman_to_timman, buf_INC, MSG_SIZE);
							timeoutd=0;
							timeouti=0;
						}
					}

				} else {
					// NO, is a not well formatted message
					if(DEBUG_SENDER>=NO_DEBUG) { printf("> ERR - WinMan: Received something wrong from timman!!\n"); fflush(stdout); }
					if(LOG_TO_FILE>=NO_LOG) { snprintf(buf_LOG, LOG_SIZE, "ERR - WinMan: Received something wrong from timman \"%s\"\n", buf_TIMMAN); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				}
			} else if(nreadTIMER==0) {
				perror("SS-WINMAN: fifo_timman_to_winman Closed"); *retVal = 2; return retVal;		
			}	else {
				// NO TimeOut Occurred
			} 
		}

	// end working cycle here
	}
	/* Closing Procedure */
		char * quitMSG = malloc(MSG_SIZE * sizeof(char));
		quitMSG[0]='Q';
		quitMSG[1]='U';
		quitMSG[2]='I';
		quitMSG[3]='T';
		quitMSG[4]=0;
		quitMSG[5]=0;
		quitMSG[6]=0;
		write(fifo_winman_to_recver, quitMSG, MSG_SIZE);
		write(fifo_winman_to_timman, quitMSG, MSG_SIZE);
		write(fifo_winman_to_sender, quitMSG, MSG_SIZE);

		usleep(300000);
		write(fifo_TOlog, quitMSG, MSG_SIZE);

		usleep(300000);
		close(fifo_TOlog);
		close(fifo_winman_to_sender);
		close(fifo_recver_to_winman);
		close(fifo_winman_to_recver);
		close(fifo_winman_to_timman);
		close(fifo_timman_to_winman);
		
		for(i=0; i<6; i++) 
			unlink(thr_args.fnames[i]);

		usleep(300000);
		fcloseall();
		return retVal;
}

void closingSender() {
	printf("\nClosing Sender... '%d' ", getpid());
	fcloseall();
	exit(2);
}

