#ifndef LIBRUDPCOSTANTS_H
#define	LIBRUDPCOSTANTS_H


/* Generic Parameters */
	#define SHARED_FOLDER "RUDPServerFiles/" // Parameter configurable, this is a default value
	#define CLIENT_FOLDER "RUDPClientFiles/" // Parameter configurable, this is a default value
	#define LOSS_PROB 0.8 // Parameter configurable, this is a default value
	#define MAXFILENUM 100 // Parameter configurable, this is a default value

/* Reliable UDP Transfer Parameters */
	#define FLP_SIZE 993
	#define HDR_SIZE 7 // "PKT" +  Seq Number
	#define PKT_SIZE 1000 // FLP_SIZE + HDR_SIZE
	#define ACK_SIZE 7
	#define TRANSFERWINDOW_SIZE 10 // Parameter configurable, this is a default value
	#define INTERVALSENDTIME 5000 // Parameter configurable, this is a default value
	#define TIMEOUT 50000 // Advised Value: INTERVALSENDTIME * WINDOWSIZE
	#define TIMEPOLL 5000

/* Client/Server Communication Standard Parameters */
	#define SERVCONNPORT 25490 // Parameter configurable, this is a default value
	#define SERVIP "127.0.0.1" // Parameter configurable, this is a default value
	#define CONN_SIZE 4
	#define CMDPKT_SIZE 100 // =CMD_SIZE+TXT_SIZE
	#define CMDCMD_SIZE 4
	#define CMDTXT_SIZE 96

/* Control Parameters */
 // Handle Log on File
	#define LOG_TO_FILE 2  // 0->No Log / 1->only imp Log / 2->big Log
	#define NO_LOG 0
	#define L1_LOG 1
	#define L2_LOG 2
	#define LOG_SIZE 150
	#define CLIENT_FIFO_TO_LOG "tmp/fifo.client_to_log"
	#define SERVER_FIFO_TO_LOG "tmp/fifo.server_to_log"

 // Handle STDOUT
	#define DEBUG_RECVER 0 // 0->No Debug / 1->only imp Debug / 2->big Debug
	#define DEBUG_SENDER 0 // 0->No Debug / 1->only imp Debug / 2->big Debug
	#define DEBUG_SERVER 2 // 0->No Debug / 1->only imp Debug / 2->big Debug
	#define DEBUG_CLIENT 0 // 0->No Debug / 1->only imp Debug / 2->big Debug
	#define DEBUG_ERROR 1  // 0->No Debug / 1->only imp Debug / 2->big Debug
	#define DEBUG_RUDP 0  // 0->No Debug / 1->only imp Debug / 2->big Debug
	#define NO_DEBUG 0
	#define L1_DEBUG 1
	#define L2_DEBUG 2

 // Parameters for Functions
	#define IAMSERVER 1
	#define IAMCLIENT 2
	#define IAMSENDER 3
	#define IAMRECVER 4

	#define MAXFIFONAME_SIZE 100
/* Sender Side, Fifos Names For Internal Threads Communication in Trnasfer Process, and other Parameters */
	#define SS_FIFO_WINMAN_TO_SENDER "tmp/fifo.ss_winman_to_sender."
	#define SS_FIFO_WINMAN_TO_TIMMAN "tmp/fifo.ss_winman_to_timman."
	#define SS_FIFO_TIMMAN_TO_WINMAN "tmp/fifo.ss_timman_to_winman."
	#define SS_FIFO_RECVER_TO_WINMAN "tmp/fifo.ss_recver_to_winman."
	#define SS_FIFO_WINMAN_TO_RECVER "tmp/fifo.ss_winman_to_recver."
	#define SS_FIFO_TO_LOG           "tmp/fifo.ss_to_log."
	#define SS_FINDEX_TOLOG 0
	#define SS_FINDEX_WTOS 1
	#define SS_FINDEX_RTOW 2
	#define SS_FINDEX_WTOT 3
	#define SS_FINDEX_TTOW 4
	#define SS_FINDEX_WTOR 5


/* Receiver Side, Fifos Names For Internal Threads Communication in Trnasfer Process, and other Parameters */
	#define RS_FIFO_WINMAN_TO_SENDER "tmp/fifo.rs_winman_to_sender."
	#define RS_FIFO_RECVER_TO_WINMAN "tmp/fifo.rs_recver_to_winman."
	#define RS_FIFO_WINMAN_TO_RECVER "tmp/fifo.rs_winman_to_recver."
	#define RS_FIFO_WINMAN_TO_WRITER "tmp/fifo.rs_winman_to_writer."
	#define RS_FIFO_TO_LOG           "tmp/fifo.rs_to_log."
	#define RS_FINDEX_TOLOG 0
	#define RS_FINDEX_WTOS 1
	#define RS_FINDEX_RTOW 2
	#define RS_FINDEX_WTOR 3
	#define RS_FINDEX_WTOW 4

/* Inter-comunication Thread Parameters */
	#define MSG_SIZE 7

#endif	/* LIBRUDPCOSTANTS_H */

