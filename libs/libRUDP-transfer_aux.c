
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

char * addHeader(char * dest, int *seq, char * buff, int sizeH, int sizeB) {
	*dest     = 'P'; 
	*(dest+1) = 'K';
	*(dest+2) = 'T';
	memcpy(dest+3, seq, sizeof(int));
	memcpy((dest+sizeH), buff, sizeB);
	return dest;
}

TransferWindow * new_Window(TransferWindow * window, int tot_flp, int win_start, int win_size) {
	window = calloc(1,sizeof(struct _RUDP_TRANSFER_WINDOW));
	
	window->TOT_FLP = tot_flp;
	window->PKTS_STOCK = tot_flp;
	window->WIN_START = win_start;
	window->WIN_SIZE = win_size;
	
	window->PKTS_STATE = calloc(tot_flp, sizeof(int));
	window->PKTS_TRY = calloc(tot_flp, sizeof(int));
	int i = 0;
	for(i=0; i<window->TOT_FLP; i++) {
		window->PKTS_STATE[i] = WINSTATE_TOSEND;
	}
	for(i=0; i<window->TOT_FLP; i++) {
		window->PKTS_TRY[i] = 0;
	}
	return window;
}

int FindNextToSendInWindow(TransferWindow * window) {
/* return the relative position of the first '2'(WINSTATE_TOSEND) in the array even if is in the window, else return 1 */
	int i=0;
	int currentPosition=0;
	for(i=0; i<window->WIN_SIZE && ((window->WIN_START)+i)<(window->TOT_FLP); i++) {
		currentPosition = (window->WIN_START)+i;
		if( (window->PKTS_STATE[currentPosition]) == WINSTATE_TOSEND ) {
			window->PKTS_STATE[currentPosition] = WINSTATE_SENT;
			window->PKTS_TRY[currentPosition]++;
			return currentPosition;
		}
	}
	return (-1);
}

double MeanSendTry(TransferWindow * window) {
/* return the relative position of the first '2'(WINSTATE_TOSEND) in the array even if is in the window, else return 1 */
	int i=0;
	int currentPosition=0;
	double tot=0.0;
	int move=0;
	for(i=0; i<window->WIN_SIZE && ((window->WIN_START)+i)<(window->TOT_FLP); i++) {
		currentPosition = (window->WIN_START)+i;
		if(window->PKTS_TRY[currentPosition] != 0); {
			tot+=(double) window->PKTS_TRY[currentPosition];
			move++;
		}
	}
	return (double)(tot/(double)move);
}

int moveWindow(TransferWindow * window , int seqRcvd, int whoiam) {
	int stopcycle = 0;
	int curPos = window->WIN_START;
	int i = 0;
	int posMoved = 0;
	if(whoiam == IAMSENDER) {
		if( (seqRcvd >= window->WIN_START) && (seqRcvd < (window->WIN_START+window->WIN_SIZE)) ) {
			// Set Packet as Acked
			if( window->PKTS_STATE[seqRcvd] == WINSTATE_SENT) {
				window->PKTS_STATE[seqRcvd] = WINSTATE_ACKED;
				window->PKTS_STOCK--;
			} else if( window->PKTS_STATE[seqRcvd] == WINSTATE_TOSEND) {
				if(DEBUG_SENDER) { printf("> ERR - moveWindow: not Send yet %d\n", seqRcvd); fflush(stdout); }
				window->PKTS_STATE[seqRcvd] = WINSTATE_ACKED;
				window->PKTS_STOCK--;
			} else {
				if(DEBUG_SENDER) { printf("> ERR - moveWindow: Duplicate Ack %d\n", seqRcvd); fflush(stdout); }
				return -1;
			}
			// Move Window
			for(i=0;
				(i<window->WIN_SIZE) &&
				(((window->WIN_START)+i)<(window->TOT_FLP)) &&
				!(stopcycle); 
				i++) {
				curPos = window->WIN_START + i;
				if( window->PKTS_STATE[curPos] == WINSTATE_ACKED ) {
					posMoved ++;
					// Continue to look window
				} else {
					stopcycle = 1;
				}
			}
			// Set new WinStart
			window->WIN_START = curPos;
		} else {
				if(DEBUG_SENDER) { printf("> ERR - moveWindow: not In Window %d\n", seqRcvd); fflush(stdout); }
			return -1;
		}	
	}
	if(whoiam == IAMRECVER) {
		if( (seqRcvd >= window->WIN_START) && (seqRcvd < (window->WIN_START+window->WIN_SIZE)) ) {
			// Set Packet as Recved
			if( window->PKTS_STATE[seqRcvd] == WINSTATE_TORECV) {
				window->PKTS_STATE[seqRcvd] = WINSTATE_RECVED;
				window->PKTS_STOCK--;
			} else {
				if(DEBUG_RECVER>0) { printf("< ERR - moveWindow: packet ALREADY RECVED! Rejected\n"); fflush(stdout); return -1;}
			}
			// Move Window
			for( i=0; 
				i<window->WIN_SIZE && 
				((window->WIN_START)+i)<(window->TOT_FLP) && 
				!(stopcycle); 
				i++) {
				curPos = window->WIN_START + i;
				if( window->PKTS_STATE[curPos] == WINSTATE_RECVED ) {
					posMoved ++;
					// Continue to look window
				} else {
					stopcycle = 1;
				}
			}
			// Set new WinStart
			window->WIN_START += posMoved;
		} else {
			if(DEBUG_RECVER>0) { printf("< ERR - moveWindow: packet NOT IN WINDOW! Rejected!\n"); return -1; }
		}	
	}
	return posMoved;
}

int resendIt(TransferWindow * window, int occSeq){
	/* set as TOSEND the occSeq packet */
	if(window->PKTS_STATE[occSeq] == WINSTATE_ACKED) {
		if(DEBUG_SENDER) { printf("< resendIt : Packet <%d> to resend, ALREADY ACKED! Rejected!\n", occSeq); fflush(stdout); }
		return (-1);
	} else if (window->PKTS_STATE[occSeq] == WINSTATE_TOSEND) {
		if(DEBUG_SENDER) { printf("< ERR - resendIt: Packet <%d> to resend, NOT SEND YET! Rejected!\n", occSeq); fflush(stdout); }
		return (-1);
	} else {
		window->PKTS_STATE[occSeq] = WINSTATE_TOSEND;
	}
	return (0);
}

int checkTimer(my_list_p * list, long actualTime) {
	int seq = (-1);
	int i = 0;
	if((list->first)) {
		my_piece_d * node = getMinP(list);
		if( actualTime > node->time ){
			memcpy(&seq,&(node->id),sizeof(int));
			delNodeP(list, node->id);
			return seq;
		} else {
			return seq;
		}
	} else {
		return seq;
	}
}

LogCommandsList * logcmd(LogCommandsList * logcmdlist, int type, char * fileName) {
	memset(logcmdlist->logCommands[logcmdlist->pos], 0, logcmdlist->strlength);
	memset(logcmdlist->currentstring, 0, logcmdlist->strlength);
	// LIST COMMAND
	if (type == 0)
		snprintf(logcmdlist->currentstring, logcmdlist->strlength,"List of Files");
	// GET COMMAND
	else if (type == 1)
		snprintf(logcmdlist->currentstring, logcmdlist->strlength,"Get \"%s\"", fileName);
	// PUT COMMAND
	else if (type == 2)
		snprintf(logcmdlist->currentstring, logcmdlist->strlength,"Put \"%s\"", fileName);
	memcpy(logcmdlist->logCommands[logcmdlist->pos], logcmdlist->currentstring, logcmdlist->strlength);
	logcmdlist->pos = (logcmdlist->pos+1)%8;
	return logcmdlist;
}

LogCommandsList * newLogCommandsList(LogCommandsList * logcmd, int dime, int stringl) {
	int i=0;
	logcmd = malloc(sizeof(LogCommandsList));
	logcmd->dim = dime;
	logcmd->pos = 0;
	logcmd->strlength = stringl;
	logcmd->logCommands = malloc(logcmd->dim * sizeof(char*));
	for(i=0; i<logcmd->dim; i++) logcmd->logCommands[i] = malloc(logcmd->strlength*sizeof(char));
	logcmd->currentstring=malloc(logcmd->strlength*sizeof(char));
	return logcmd;
}

BufferingWindow * new_BufWindow(BufferingWindow * window, int dimension) {
	window = calloc(1,sizeof(struct _RUDP_BUFFERING_WINDOW));
	window->dim = dimension;
	window->PKT_BUF  = malloc(dimension*sizeof(char *));
	window->temp_BUF = malloc(dimension*sizeof(char *));
	int i = 0;
	for(i=0; i<dimension; i++) {
		window->PKT_BUF[i] = malloc(PKT_SIZE * sizeof(char));
	}
	return window;
}

BufferingWindow * shift_BufWindow(BufferingWindow * window, int posMov) {
	int i = 0;
	for(i=0; i<window->dim; i++) {
		window->temp_BUF[i] = window->PKT_BUF[i];
	}
	for(i=0; i<window->dim; i++) {
		window->PKT_BUF[i] = window->temp_BUF[ (i+posMov) % window->dim ];
	}
	return window;
}

