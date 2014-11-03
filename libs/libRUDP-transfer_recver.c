
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


int * f_r_sender(rs_threads_args * threadArgs) {

	/* Inizializing Structures */
	rs_threads_args thr_args = (rs_threads_args) *(threadArgs);
	int * nextSeq=malloc(sizeof(int));
	char * buf_PKT = malloc(ACK_SIZE*sizeof(char));
	int nreadWINMAN = 0;
	int * retVal = malloc(sizeof(int));
	*retVal = 0;

	/* Sync Fifos */
	int fifo_winman_to_sender, fifo_TOlog;
	/// Sync Fifo with Window Manager
	fifo_winman_to_sender = open( thr_args.fnames[RS_FINDEX_WTOS], O_RDONLY );
	open( thr_args.fnames[RS_FINDEX_WTOS], O_WRONLY ); // to not be blocked
	if(fifo_winman_to_sender<0) { perror("RS-SENDER: Cannot open fifo_winman_to_sender"); *retVal = 2; return retVal; }
	/// Sync Fifo with Log Thread
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( thr_args.fnames[RS_FINDEX_TOLOG], O_WRONLY );
	if(fifo_TOlog<0) { perror("RS-SENDER: Cannot open fifo_TOlog"); *retVal = 2; return retVal; }

	/*...*/
	if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "Sender: Ready\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log	
	if(DEBUG_RECVER>=L1_DEBUG) { printf("< Sender: Ready\n"); fflush(stdout); }
	int WORKING =1;

	/* Start Routine */
	while(WORKING) {
		// Read nextSeq from Window Manager
		nreadWINMAN = read( fifo_winman_to_sender, buf_PKT, MSG_SIZE);
		if(nreadWINMAN<0) { 
			perror("RS-SENDER: Read Error in SS-sender from fifo_winman_to_sender"); *retVal = 2; return retVal;
		} else if(nreadWINMAN>0) {

			if( buf_PKT[0]=='A' &&
					buf_PKT[1]=='C' &&
					buf_PKT[2]=='K' ) { 
				// OK, i have next [ACK] to send!
				memcpy(nextSeq, buf_PKT+3, sizeof(int));
			 	if ( sendto(thr_args.sockfd, buf_PKT, ACK_SIZE, 0, (struct sockaddr *)(thr_args.addrSender), *(thr_args.addrSenderSize) ) < 0) { perror("RS-SENDER: Error in sendto"); *retVal = 2; return retVal; } 
				if(LOG_TO_FILE>=L2_LOG) { snprintf(buf_LOG, LOG_SIZE, "Sender: send new ACK >%d<\n", *nextSeq); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_RECVER>=L2_DEBUG) { printf("< Sender: send new ACK >%d<\n", *nextSeq);fflush(stdout); }
			}

			else if ( buf_PKT[0]=='Q' &&
								buf_PKT[1]=='U' &&
								buf_PKT[2]=='I' &&
								buf_PKT[3]=='T') {
				// OK, is the [QUIT] Notification
				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "Sender: Quitting...\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_RECVER>=L1_DEBUG) { printf("< Sender: Quitting...\n");fflush(stdout); }
				WORKING = 0;
			} 

			else {
				// No, this is a not-well formatted packet, rejected
				if(DEBUG_ERROR) { printf("< ERR - Sender: Received something wrong from window manager %d:%s\n",nreadWINMAN, buf_PKT);fflush(stdout); }
				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "ERR - Sender: Received something wrong from window manager %d:<%s>\n",nreadWINMAN, buf_PKT); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log			
			}
		} else {
			perror("RS-SENDER: fifo_winman_to_sender Closed"); *retVal = 2; return retVal;
		}
	}
	close(fifo_TOlog);
	close(fifo_winman_to_sender);
	return retVal;
}

int * f_r_recver(rs_threads_args * threadArgs) {

	/* Inizialize Strctures */
	rs_threads_args thr_args = (rs_threads_args) *(threadArgs);
	char * buf_RECVED = calloc(PKT_SIZE,sizeof(char));
	int seq=0;
	int nreadSOCKFD = 0;
	char * buf_WINMAN = malloc(MSG_SIZE*sizeof(char));
	int nreadWINMAN=0;
	int * retVal = malloc(sizeof(int));
	*retVal = 0;

	/* Sync Fifos */
	int fifo_recver_to_winman, fifo_TOlog, fifo_winman_to_recver, flags;
	/// Sync Fifo with Window Manager
	fifo_recver_to_winman = open( thr_args.fnames[RS_FINDEX_RTOW], O_WRONLY );
	if(fifo_recver_to_winman<0) { perror("RS-RECVER: Cannot open fifo_recver_to_winman"); *retVal = 2; return retVal; }
	/// Sync Fifo with Window Manager, Non Blocking Fifo
	fifo_winman_to_recver = open( thr_args.fnames[RS_FINDEX_WTOR], O_RDONLY );
	if(fifo_winman_to_recver<0) { perror("RS-RECVER: Cannot open fifo_winman_to_recver"); *retVal = 2; return retVal; }
	flags = fcntl(fifo_winman_to_recver, F_GETFL); fcntl(fifo_winman_to_recver, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
	/// Sync Fifo with Log Thread
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( thr_args.fnames[RS_FINDEX_TOLOG], O_WRONLY );
	if(fifo_TOlog<0) { perror("RS-RECVER: Cannot open fifo_TOlog"); *retVal = 2; return retVal; }

	/*...*/
	flags = fcntl(thr_args.sockfd, F_GETFL);
	fcntl(thr_args.sockfd, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
	if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "Recver: Ready\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
	if(DEBUG_RECVER>=L1_DEBUG) { printf("< Recver: Ready\n");fflush(stdout); }
	int WORKING = 1;

	/* Start Routine */
	while(WORKING) {
//		usleep(TIMEPOLL);

		/* Check for new PKT */
		nreadSOCKFD = recvfrom (thr_args.sockfd, buf_RECVED, PKT_SIZE, 0,(struct sockaddr *) thr_args.addrSender, thr_args.addrSenderSize);
		if ( nreadSOCKFD<0 && errno!=EAGAIN ) { 
			perror("RS-RECVER: Read Error in sockfd"); *retVal = 2; return retVal;
		} else if ( nreadSOCKFD>0 ) {
			if ( buf_RECVED[0]=='P' && 
					 buf_RECVED[1]=='K' && 
					 buf_RECVED[2]=='T' ) {
			 // OK, i recived a new [PKT]
				memcpy(&seq,buf_RECVED+3, sizeof(int)); // why? is for debug log
				if(LOG_TO_FILE>=L2_LOG) { snprintf(buf_LOG, LOG_SIZE, "Recver: Pkt Recved %d:<%d>\n",nreadSOCKFD, seq); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_RECVER>=L2_DEBUG) { printf("< Recver: Pkt Recved %d:<%d>\n",nreadSOCKFD, seq);fflush(stdout); }
				write( fifo_recver_to_winman, buf_RECVED, PKT_SIZE);
			} else {
				// NO, the packet is not well formatted, rejected
				if(DEBUG_ERROR) { printf("< ERR - Recver: Recved something wrong from socket!! %s", buf_RECVED); }
				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "< ERR - Recver: Recved something wrong from socket!! \n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
			}
		} else if(errno==EAGAIN) {
			// NO Packet Arrived

			/* Check for Window Manager Comunication */
			nreadWINMAN = read(fifo_winman_to_recver, buf_WINMAN, MSG_SIZE);
			if(nreadWINMAN<0 && errno!=(EAGAIN)) {
				perror("RS-RECVER: Read Error in fifo_winman_to_recver"); *retVal = 2; return retVal;
			} else if(nreadWINMAN>0) {
				if( buf_WINMAN[0]=='Q' &&
						buf_WINMAN[1]=='U' &&
						buf_WINMAN[2]=='I' &&
						buf_WINMAN[3]=='T' ) {
				 // OK, is a [QUIT] Packet
					if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "Recver: Quitting...\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
					if(DEBUG_RECVER>=L1_DEBUG) { printf("< Recver: Quitting...\n", seq);fflush(stdout); }
					WORKING = 0;
				} else {
					// NO, is a not well formatted message
					if(DEBUG_ERROR) { printf("> ERR - RECVER: Received something wrong from winman %s\n", buf_WINMAN);fflush(stdout); }
					if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "ERR - Recver: Received something wrong from winman \"%s\"\n", buf_WINMAN); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				}
			} else if(nreadWINMAN==0) {
				perror("RS-RECVER: fifo_winman_to_recver Closed"); *retVal = 2; return retVal;			
			}	else {
				// NO Packet Arrived
			}

		} else {
			perror("RS-RECVER: sockfd Closed"); *retVal = 2; return retVal;
		}

	// End While Cicle
	}
	close(fifo_TOlog);
	close(fifo_recver_to_winman);
	close(fifo_winman_to_recver);
	return retVal;	
}

int * f_r_logManager(rs_threads_args * threadArgs) {

	/* Inizialize Strctures and Files */
	int * retVal = malloc(sizeof(int));
	*retVal = 0;
	rs_threads_args thr_args = (rs_threads_args) *(threadArgs);
	char * logFilePath = calloc(LOG_SIZE,sizeof(char));
	char * counttime = calloc(LOG_SIZE,sizeof(char));
	char * AuxString = calloc(10,sizeof(char));
	time_t timer;
	timer=time(NULL);
	struct tm * t;
	t = localtime(&timer);
	strftime(counttime, LOG_SIZE, "%y-%m-%d|%H-%M-%S--", t);
	strncat(logFilePath,"log/", LOG_SIZE);
	strncat(logFilePath, counttime, LOG_SIZE);
	snprintf(AuxString, 10, "%d", getpid());
	strncat(logFilePath, AuxString, LOG_SIZE);
	strncat(logFilePath,"-RUDP_transfer_recver.log", LOG_SIZE);
	
	FILE * logSendFile = fopen(logFilePath, "a");
	if(logSendFile<0) { perror("RS-LOG: Error in Open log_send_recver.log"); *retVal = 2; return retVal;}

	/* Sync Fifo */
	int fifo_TOlog;
	int nreadFIFO=0;
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( thr_args.fnames[RS_FINDEX_TOLOG], O_RDONLY );
	if(fifo_TOlog<0) { perror("RS-LOG: Cannot open fifo_TOlog"); *retVal = 2; return retVal; }

	// Set Title
	fputs("\n---NEW--EXECUTION: ", logSendFile);
	fputs(asctime(localtime(&timer)), logSendFile);
	fputs("\n", logSendFile);
	fflush(logSendFile);
	if(DEBUG_RECVER>=L1_DEBUG) { printf("< LogManager: Ready\n");fflush(stdout); }
	int WORKING = 1;

	/* Start Routine Log Manager */
	while(WORKING) {
		nreadFIFO = read(fifo_TOlog, buf_LOG, LOG_SIZE);
		if( nreadFIFO<0 ) { 
			perror("RS-LOG: Read Error"); *retVal = 2; return retVal; 
		} else if ( nreadFIFO>0 ) {
			if(buf_LOG[0]=='Q' && 
				 buf_LOG[1]=='U' && 
				 buf_LOG[2]=='I' && 
				 buf_LOG[3]=='T' ) {
				// OK, is the [QUIT] Notification
				WORKING = 0;
				if(DEBUG_RECVER>=L1_DEBUG) { printf("< LogMan: Quitting...\n");fflush(stdout); }
			} else {
				// OK, I suppose buf_LOG is a string with sense which ends with '\0' char
				timer=time(NULL);
				t = localtime(&timer);
				memset(counttime, 0, LOG_SIZE);
				strftime(counttime, LOG_SIZE, "%x-%X ", t);
				fputs(counttime, logSendFile);
				fputs(buf_LOG, logSendFile);
				fflush(logSendFile);
			}
		} else {
			perror("RS-LOGMAN: fifo_TOlog Closed"); *retVal = 2; return retVal;			
		}
	
	// end working cycle here
	}
	fclose(logSendFile);
	close(fifo_TOlog);
	return retVal;
}

int * f_r_writer(rs_threads_args * threadArgs) {

	/* Inizialize Strctures */
	int * retVal = malloc(sizeof(int));
	*retVal = 0;
	rs_threads_args thr_args = (rs_threads_args) *(threadArgs);
	FILE * reqFile = fopen(thr_args.fileName, "w");
	if(reqFile<0) { perror("RS-WRITER: Error in Open reqFile"); *retVal = 2; return retVal;}
	int nread=0;
	int flp_read=0;
	int byteStock = thr_args.fileSize;
	int seq=0;
	int oldseq=-1;
	int totRead=0;
	char * buffer = malloc(PKT_SIZE*sizeof(char));

	/* Sync Fifos */
	int fifo_winman_to_writer, fifo_TOlog;
	/// Sync Fifo with Window Manager
	fifo_winman_to_writer = open( thr_args.fnames[RS_FINDEX_WTOW], O_RDONLY );
	if(fifo_winman_to_writer<0) { perror("RS-WRITER: Cannot open fifo_winman_to_writer"); *retVal = 2; return retVal; }
	/// Sync Fifo with Log Thread
	char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
	fifo_TOlog = open( thr_args.fnames[RS_FINDEX_TOLOG], O_WRONLY );
	if(fifo_TOlog<0) { perror("RS-WRITER: Cannot open fifo_TOlog"); *retVal = 2; return retVal; }

	/*...*/
	if(LOG_TO_FILE>=L1_LOG) write(fifo_TOlog, "Writer: Start\n", LOG_SIZE ); // Log
	if(DEBUG_RECVER>=L1_DEBUG) { printf("< Writer: Start\n");fflush(stdout); }
	int WORKING = 1;

	/* Start Routine Writer */
	while(WORKING) {		
		/* Check for something to read */
		nread = read(fifo_winman_to_writer, buffer, PKT_SIZE);
		if(nread<0) { 
			perror("RS-WRITER: Read Error in fifo_winman_to_writer"); *retVal = 2; return retVal; 
		}	else if(nread>0) {
			if( buffer[0] == 'P' && 
					buffer[1] == 'K' &&
					buffer[2] == 'T' ) {
				// OK, is a new [PKT] to write
				flp_read = nread-HDR_SIZE;
				if(byteStock>flp_read)
					byteStock-=flp_read;
				else
					flp_read = byteStock;
				totRead += flp_read;
				// Save Seq Num
				memcpy(&seq, buffer+3, sizeof(int));
				if(oldseq != seq-1) {
					if(LOG_TO_FILE>=L1_LOG) write (fifo_TOlog, "ERR - Writer: Wrong Sequence of Packet\n", LOG_SIZE ); // Log
					if(DEBUG_ERROR) { printf("< ERR - Writer: Wrong Sequence of Packet %d->%d\n", oldseq, seq);fflush(stdout); }
				}
				oldseq = seq;
				// Save FLP
				fwrite((buffer+HDR_SIZE), 1, flp_read, reqFile);
			} else if( buffer[0] == 'Q' &&
								 buffer[1] == 'U' &&
								 buffer[2] == 'I' &&
								 buffer[3] == 'T') {
				// OK, is a [QUIT] Notification
				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "Writer: Quitting...\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
				if(DEBUG_RECVER>=L1_DEBUG) { printf("< Writer: Quitting...\n");fflush(stdout); }
				WORKING = 0;
			} else {
				if(LOG_TO_FILE>=L1_LOG) write (fifo_TOlog, "ERR - Writer: Received something wrong from Window Manager\n", LOG_SIZE ); // Log
				if(DEBUG_ERROR) { printf("< ERR - Writer: Received something wrong from Window Manager\n");fflush(stdout); }
			}
		} else {
		}

	// end working cycle here
	}
	close(fifo_TOlog);
	close(fifo_winman_to_writer);
	fclose(reqFile);
	return retVal;
}

int * f_r_windowManager(rs_threads_args * threadArgs) {

	/* Inizialize Strctures */
		rs_threads_args thr_args = (rs_threads_args) *(threadArgs);
		int TOT_FLP = (thr_args.fileSize/FLP_SIZE)+1; // Total Packets Number to Send
		TransferWindow * t_window = new_Window(t_window, TOT_FLP, 0, thr_args.windowSize);
		BufferingWindow * b_window= new_BufWindow(b_window, thr_args.windowSize);
		int posMoved = 0;
		int posBuffered = 0;
		int seqRcvd = 0;
		int i =0;
		int * retVal = malloc(sizeof(int));
		*retVal = 0;

		char * buf_RECVER = malloc(PKT_SIZE*sizeof(char)); int nreadRECVER=0;	
		char * buf_ACK = malloc(MSG_SIZE*sizeof(char));	
		char * win_STRING = malloc(LOG_SIZE*sizeof(char));
		char * win_AUX = malloc(LOG_SIZE*sizeof(char));
		buf_ACK[0]='A';	
		buf_ACK[1]='C';	
		buf_ACK[2]='K';

		printf("+++++++++++++++\n");
		printf("+ Start receive\n");
		printf("+ name: \"%s\"\n", thr_args.fileName);
		printf("+ size: %d bytes\n", thr_args.fileSize);
		printf("+ total Pkts: %d\n", TOT_FLP);
		printf("+ each one of: %d bytes\n", FLP_SIZE);
		printf("+ window Size: \"%d\"\n", thr_args.windowSize);
		printf("+++++++++++++++\n");
		fflush(stdout);


	/* Sync Fifos */
		int fifo_winman_to_sender, fifo_recver_to_winman, fifo_winman_to_recver, fifo_winman_to_writer, fifo_TOlog;
		/// Sync Fifo with Log Thread
		char * buf_LOG = malloc(LOG_SIZE*sizeof(char));
		fifo_TOlog = open( thr_args.fnames[RS_FINDEX_TOLOG], O_WRONLY );
		if(fifo_TOlog<0) { perror("RS-WINMAN: Cannot open fifo_TOlog\n"); *retVal = 2; return retVal; }
		/// Sync Fifo with Sender
		fifo_winman_to_sender = open( thr_args.fnames[RS_FINDEX_WTOS], O_WRONLY );
		if(fifo_winman_to_sender<0) { perror("RS-WINMAN: Cannot open fifo_winman_to_sender\n"); *retVal = 2; return retVal; }
		/// Sync Fifo with Recver
		fifo_recver_to_winman = open( thr_args.fnames[RS_FINDEX_RTOW], O_RDONLY );
		if(fifo_recver_to_winman<0) { perror("RS-WINMAN: Cannot open fifo_recver_to_winman\n"); *retVal = 2; return retVal; }
		/// Sync Fifo with Recver
		fifo_winman_to_recver = open( thr_args.fnames[RS_FINDEX_WTOR], O_WRONLY );
		if(fifo_winman_to_recver<0) { perror("RS-WINMAN: Cannot open fifo_winman_to_recver"); *retVal = 2; return retVal; }
		/// Sync Fifo with Writer
		fifo_winman_to_writer = open( thr_args.fnames[RS_FINDEX_WTOW], O_WRONLY );
		if(fifo_winman_to_writer<0) { perror("RS-WINMAN: Cannot open fifo_winman_to_writer\n"); *retVal = 2; return retVal; }

	/*...*/
	if(LOG_TO_FILE>=L1_LOG) write (fifo_TOlog, "WinMan: Inizialized, Starting routine\n", LOG_SIZE ); // Log
	if(DEBUG_RECVER>=L1_DEBUG) { printf("< WinMan: Inizialized, Starting routine\n");fflush(stdout); }
	int WORKING = 1;
	
	/* Start Routine Window Manager */
	while(WORKING) {
	//	usleep(TIMEPOLL); 

		// Log Transfer Window
		if(LOG_TO_FILE>=L2_DEBUG) { 
			memset(win_STRING,0,LOG_SIZE);
			memset(win_AUX,0,LOG_SIZE);
			int aux=0;
			for(i=0; i<t_window->TOT_FLP; i++) {
				if(i == t_window->WIN_START) {
					snprintf(win_AUX, LOG_SIZE, "window: %d [ (%d)",aux, t_window->PKTS_STATE[i] );
					strncat(win_STRING,win_AUX,LOG_SIZE); 
					memset(win_AUX,0,LOG_SIZE);}
				else if( (i>t_window->WIN_START) && (i < (t_window->WIN_START+t_window->WIN_SIZE))) { 
					snprintf(win_AUX, LOG_SIZE, "- (%d)", t_window->PKTS_STATE[i] );
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
 		 * Consists in 1 Main Routine:
		 *  > it Handles the packet receives from receiver
		 */
		
		/* Check for new Pkt from Recver */
		nreadRECVER = read( fifo_recver_to_winman, buf_RECVER, PKT_SIZE);
		if(nreadRECVER<0 && errno!=(EAGAIN)) {
			perror("RS-WINMAN: Read Error in fifo_recver_to_winman\n"); *retVal = 2; return retVal;
		} else if(nreadRECVER>0) {
			if( buf_RECVER[0]=='P' &&
				  buf_RECVER[1]=='K' &&
				  buf_RECVER[2]=='T') {
				// OK, is a new [PKT]
				memcpy(&seqRcvd, buf_RECVER+3, sizeof(int));
				posBuffered = seqRcvd - t_window->WIN_START; // it is the relative sequence number!
				if(posBuffered>=0) {
					memcpy(b_window->PKT_BUF[posBuffered], buf_RECVER, PKT_SIZE);
				}
				posMoved = moveWindow(t_window, seqRcvd, IAMRECVER);
				if(posMoved == 0) {
					// I have to bufferize the packet
				} else if(posMoved>0) {
					// Send all buffered Packets
					for(i=0; i<posMoved; i++){
						write( fifo_winman_to_writer, b_window->PKT_BUF[i], PKT_SIZE);
					}
					b_window = shift_BufWindow(b_window, posMoved);
				}
				if(DEBUG_RECVER>=L1_DEBUG) { printf("< WindowManager: new Pkt recived <%d> with '%d' relative seq, i move window in <%d> for %d steps, pktsStock <%d>\n",seqRcvd, posBuffered, t_window->WIN_START, posMoved, t_window->PKTS_STOCK);fflush(stdout); }
				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "WinMan: new Pkt recived <%d>, i move window in <%d>, pktsStock <%d>\n",seqRcvd, t_window->WIN_START, t_window->PKTS_STOCK); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log					

				// Send [ACK] to Sender
				memcpy(buf_ACK+3, &seqRcvd, sizeof(int));
				write( fifo_winman_to_sender, buf_ACK, MSG_SIZE);
				if(t_window->PKTS_STOCK == 0) {
					/* Start Closing Procedure ! */
					if(DEBUG_RECVER>=L1_DEBUG) { printf("< WindowManager: File Transfert Successfully\n"); fflush(stdout); }
					if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "WinMan: File transfer Completed Successfully\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
					WORKING=0;
				}
			} else {
				// NO, is a not well formatted message
				if(DEBUG_ERROR) { printf("< ERR - WindowManager: Received something wrong from recver \"%s\"\n", buf_RECVER);fflush(stdout); }
				if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "ERR - WinMan: Received something wrong from recver \"%s\"\n", buf_RECVER); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
			}
		} else if(nreadRECVER==0) {
			perror("RS-WINMAN: fifo_recver_to_winman Closed"); *retVal = 2; return retVal;			
		}	else {
			// NO Ack Arrived
		} 

	// end working cycle here
	}
	/* Closing Procedure */
		// OK, is the [QUIT] Notification
		if(LOG_TO_FILE>=L1_LOG) { snprintf(buf_LOG, LOG_SIZE, "WinMan: Quitting...\n"); write (fifo_TOlog, buf_LOG, LOG_SIZE ); } // Log
		if(DEBUG_RECVER>=L1_DEBUG) { printf("< WinMan: Quitting...\n");fflush(stdout); }
		char * quitMSG = malloc(MSG_SIZE * sizeof(char));
		quitMSG[0]='Q'; 
		quitMSG[1]='U'; 
		quitMSG[2]='I'; 
		quitMSG[3]='T';
		write(fifo_winman_to_recver, quitMSG, MSG_SIZE);
		write(fifo_winman_to_sender, quitMSG, MSG_SIZE);
		write(fifo_winman_to_writer, quitMSG, MSG_SIZE);

		usleep(300000);
		write(fifo_TOlog, quitMSG, MSG_SIZE);

		usleep(300000);
		close(fifo_TOlog);
		close(fifo_winman_to_sender);
		close(fifo_recver_to_winman);
		close(fifo_winman_to_recver);
		close(fifo_winman_to_writer);

		for(i=0; i<5; i++) 
			unlink(thr_args.fnames[i]);

		usleep(300000);
		fcloseall();
		return retVal;
}

void closingRecver() {
	printf("Closing Receiver\n");
	int i =0;
	fcloseall();
	exit(2);
}
