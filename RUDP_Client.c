#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#include "libs/libRUDP-generic.h"
#include "libs/libRUDP-constants.h"

int main(int argc, char *argv[ ]) {
	if(argc>1) {
	int error=0;
/******* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 ****                                                                  *
 **                       CONFIGURATION MODE                          **
 *                                                                  ****
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *******/
		if(argc==4) {
			if(!(strcmp(argv[1],"-Config")) || !(strcmp(argv[1],"-config")) || !(strcmp(argv[1],"-c")) || !(strcmp(argv[1],"-C")) ) {

				// Change Shared Folder Path
				if( !(strcmp(argv[2],"Path")) || !(strcmp(argv[2],"path")) ) 
				{
					FILE * param;
					mkdir("ClientParams", 0777);
					if( !(param=fopen("ClientParams/ClientFolderPath.param","w"))){perror("Error in Open ClientFolderPath.param"); exit(1);}
					if( !(strcmp(argv[3], "Default")) || !(strcmp(argv[3], "default")) ) {
						fputs(CLIENT_FOLDER, param);
					} else {
						fputs(argv[3], param);
					}
				}
				// Change Connection Port
				else if ( !(strcmp(argv[2],"ConnPort")) || !(strcmp(argv[2],"connport")) )
				{
					FILE * param;
					mkdir("ClientParams", 0777);
					if( !(param=fopen("ClientParams/ConnPort.param","w"))) {perror("Error in Open ConnPort.param"); exit(1);}
					if( !(strcmp(argv[3], "Default")) || !(strcmp(argv[3], "default")) ) {
						char * connectionport = malloc(10*sizeof(char));
						snprintf(connectionport, 10, "%d", SERVCONNPORT);
						fputs(connectionport, param);
					} else {
						fputs(argv[3], param);
					}
				}
				// Change Server IP
				else if ( !(strcmp(argv[2],"Server")) || !(strcmp(argv[2],"server")) )
				{
					FILE * param;
					mkdir("ClientParams", 0777);
					if( !(param=fopen("ClientParams/ServerIP.param","w"))) {perror("Error in Open ServerIP.param"); exit(1);}
					if( !(strcmp(argv[3], "Default")) || !(strcmp(argv[3], "default")) ) {
						fputs(SERVIP, param);
					} else {
						fputs(argv[3], param);
					}
				}
				// Change Loss Prob
				else if ( !(strcmp(argv[2],"LossProb")) || !(strcmp(argv[2],"lossprob")) )
				{
					FILE * param;
					mkdir("ClientParams", 0777);
					if( !(param=fopen("ClientParams/LossProb.param","w"))) {perror("Error in Open LossProb.param"); exit(1);}
					if( !(strcmp(argv[3], "Default")) || !(strcmp(argv[3], "default")) ) {
						char * lossprobstring = malloc(10*sizeof(char));
						snprintf(lossprobstring, 10, "%f", LOSS_PROB);
						fputs(lossprobstring, param);
					} else {
						fputs(argv[3], param);
					}
				}

				else
					error=1;
			} else
				error=1;
		} else
			error=1;
		if(error) {
			printf("##### #### ### ## #\n");
			printf("#-----Wrong-use------\n");
			printf("# Configuration Mode:\n");
			printf("#   %s -Config Path     \"path\"   To Change Shared Path Directory\n", argv[0]); 
			printf("#   %s -Config Server   \"ip\"     To Change Server IP\n", argv[0]);
			printf("#   %s -Config ConnPort \"port\"   To Change Connection Port\n", argv[0]);
			printf("#   %s -Config LossProb \"prob\"   To Change Packet Loss Pobability\n", argv[0]);
			printf("#         \"default\" To Change with Default Value:\n");
			printf("# Start Mode:\n");
			printf("#   %s\n", argv[0]);  
			printf("##### #### ### ## #\n");
  	}
	} else {
	/******* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
	 ****                                                                  *
	 **                       EXECUTION MODE                              **
	 *                                                                  ****
	 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *******/

		pid_t fatpid = getpid();

		char * logfifoname = calloc(100, sizeof(char));
		char * pidAUX = calloc (10, sizeof(char));
		strncat(logfifoname, CLIENT_FIFO_TO_LOG, 100);
		snprintf(pidAUX, 10, "%d", fatpid);
		strncat(logfifoname, pidAUX, 100);
		
		
		/* Initialize Log Manager and his Fifo*/
		mkdir("tmp", 0777);
		if(mkfifo(logfifoname, 0777)) { 
			if(errno!=EEXIST) { perror("CLIENT: Cannot create client_fifo_to_log"); exit(1); } 
		}

		pid_t logPid = fork();
		if(logPid == 0) { 
			// This is LOG Process
			clnt_logManager(logfifoname);
		} else {
			// This is CLIENT Process
			signal(SIGINT, closing_client);

			/* Sync LogManager Fifo*/
			char * buf_LOG = calloc(LOG_SIZE,sizeof(char));
			int fifo_TOlog = open( logfifoname, O_WRONLY );
			if(fifo_TOlog<0) { perror("CLIENT: Cannot open fifo_TOlog\n"); exit(1); }

			// log...
			snprintf(buf_LOG+8, LOG_SIZE-8, "\n\n\n\n\n\n\n\n\n\n\n\n\n\nStarting...\nFor Quit, Send SIGINT [Ctrl+C]\n----====== PARAMETERS ======----\n");
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, 0, 3);

			usleep(TIMEPOLL);
			/* Get Parameters from params Files */
			FILE * param;
			char * AuxString = calloc(10,sizeof(char));
			char * path = calloc(200,sizeof(char));
			char * ServerIP = malloc(20*sizeof(char));
			short connPort = 0;
			double lossProb = 0.0;
			if( !(param=fopen("ClientParams/ClientFolderPath.param","r"))) {
				path = CLIENT_FOLDER;
				printf("Error to Retrieve Folder Path, try with default: %s\n", path);fflush(stdout);
				mkdir("ClientParams", 0777);
				mkdir(path, 0777);
				param=fopen("ClientParams/ClientFolderPath.param","w");
				fputs(path, param);
				fclose(param);
			} else {
				fgets(path, 200, param);
				printf("Folder Path:      %s\n",path); fflush(stdout);
				mkdir(path, 0777);
				fclose(param);
			}
			if( !(param=fopen("ClientParams/ServerIP.param","r"))){
				ServerIP = SERVIP;
				printf("Error to Retrieve Server IP, try with default: %s\n", ServerIP); fflush(stdout);
				mkdir("ClientParams", 0777);
				param=fopen("ClientParams/ServerIP.param","w");
				fputs(ServerIP, param);
				fclose(param);
			} else {
				fgets(ServerIP, 20, param);
				printf("Server IP:        %s\n", ServerIP); fflush(stdout);
				fclose(param);
			}
			if( !(param=fopen("ClientParams/ConnPort.param","r"))){
				connPort = SERVCONNPORT;
				printf("Error to Retrieve Connection Port, try with default: %d\n", connPort); fflush(stdout);
				mkdir("ClientParams", 0777);
				memset(AuxString, 0, 10);
				snprintf(AuxString, 10, "%d", SERVCONNPORT);
				param=fopen("ClientParams/ConnPort.param","w");
				fputs(AuxString, param);
				fclose(param);
			} else {
				fgets(AuxString, 10, param);
				connPort = (short) atoi(AuxString);
				printf("Connection Port:  %d\n", connPort); fflush(stdout);
				fclose(param);
			}
			if( !(param=fopen("ClientParams/LossProb.param","r"))){
				lossProb = LOSS_PROB;
				printf("Error to Retrieve Loss Probability, try with default: %f\n",lossProb); fflush(stdout);
				mkdir("ClientParams", 0777);
				memset(AuxString, 0, 10);
				snprintf(AuxString, 10, "%f", LOSS_PROB);
				param=fopen("ClientParams/LossProb.param","w");
				fputs(AuxString, param);
				fclose(param);
			} else {
				fgets(AuxString, 10, param);
				lossProb = (double) atof(AuxString);
				printf("Loss Probability: %f\n", lossProb); fflush(stdout);
				fclose(param);
			}
			// log...
			snprintf(buf_LOG+8, LOG_SIZE-8, "----======xxxxxxxxxxxx======----\n");
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, 0, 3);

			/* Initialize Structures                                      *
			 *   >logcmdlist is auxiliary structure                       *
			 *    used to a mini log in the User Interface Menu           *
			 *   >connPkt is the buffer used for connections packets      *
			 *   >cmdPkt is the buffer used for commands packets          *
			 *   >WindowSize is the RUDP Transfert Window Size Parameter  *
			 *   >IntervalSendTime is the RUDP IntervalSendTime Parameter */
			int i=0; 
			LogCommandsList * logcmdlist = newLogCommandsList(logcmdlist, 8,35);
			char * connPkt = malloc(CONN_SIZE*sizeof(char));
			char * cmdPkt = calloc(CMDPKT_SIZE, sizeof(char));
			int WindowSize=0;
			int IntervalSendTime=0;

			/* * * * * * * * * * * * * * * * *
			 * Start the Connection Session  *
			 * * * * * * * * * * * * * * * * */

			// log...
			snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> Start Connection Session\n",getpid());
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

			/* Create Socket and SockAddr to Send the Connection Request  *
			 *     Fill them with parameters got before from files        */
			int sockConn;
			struct sockaddr_in * addrConnServer = malloc(sizeof(struct sockaddr_in));
			memset((void *)addrConnServer, 0, sizeof(*addrConnServer));
			addrConnServer->sin_family = AF_INET;
			addrConnServer->sin_port = htons(connPort); 
			if(inet_pton(AF_INET, ServerIP , &(*addrConnServer).sin_addr) <= 0) { fprintf(stderr, "CLIENT: Error in INET_PTON for \"%s\"", ServerIP); exit(1); }
			if ((sockConn = socket (AF_INET, SOCK_DGRAM, 0)) < 0) { perror("CLIENT: Error in SOCKET"); exit(1); }
			// log...
			snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> Socket Created, Connection Address: [ip = %s] [port = %d]\n",getpid(),sockConn,inet_ntoa(addrConnServer->sin_addr), ntohs(addrConnServer->sin_port));
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

			/* Build [CONN] Packet     *  
			 *     1st Connection Step */
			memset(connPkt,0,CONN_SIZE);
			strncpy(connPkt, "CONN", CONN_SIZE);
			usleep(TIMEPOLL);
			if ((sendto(sockConn, connPkt, CONN_SIZE, 0, (struct sockaddr *) addrConnServer, sizeof(*addrConnServer))) < 0) { perror("CLIENT: Error in SENDTO for CONN PACKET"); exit(1); }
			// log...
			snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> 1st Conn. Step: Send Connection Request\n",getpid(),sockConn,inet_ntoa(addrConnServer->sin_addr), ntohs(addrConnServer->sin_port));
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

			/* Prepare Structures for Serve             *
			 * This will be filled with                 *
			 * Client personal Server socket parameters */
			struct sockaddr_in * addrServeServer = malloc(sizeof(struct sockaddr_in));
			memset((void *)addrServeServer, 0, sizeof(*addrServeServer));	
			socklen_t servlen = sizeof(*addrServeServer);

			// ConnectionTry is not yet developed
			// it wants to limit the number of connections try to the server
			int ConnectionTry = 5;
			while(ConnectionTry) {
				//Wait for the response
				if ((recvfrom(sockConn, cmdPkt, CMDPKT_SIZE, 0,  (struct sockaddr *) addrServeServer, &servlen)) < 0) { perror("CLIENT: Error in RECVFROM for Connection ACK"); exit(1);}
			
				if( strncmp(cmdPkt,"SETT", CONN_SIZE) == 0)  {
					/* Received [SETT][WindowSize][IntervalSendTime] Packet *  
					 *     2nd Connection Step                              *  
					 *     Read and save the parameters                     */
					memcpy(&WindowSize, cmdPkt+4, sizeof(int));
					memcpy(&IntervalSendTime, cmdPkt+8, sizeof(int));
					// log...
					snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> 2nd Conn. Step: Received Connection Settings [ip = %s] [port = %d] [WS = %d] [IST = %d]\n",getpid(),sockConn,inet_ntoa(addrServeServer->sin_addr), ntohs(addrServeServer->sin_port), WindowSize, IntervalSendTime);
					logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
					// Save Parameters to file
					FILE * param;
					mkdir("ClientParams", 0777);
					if( !(param=fopen("ClientParams/WindowSize.param","w"))) {perror("Error in Open WindowSize.param"); exit(1);}
					memset(AuxString, 0, 10);
					snprintf(AuxString, 10, "%d", WindowSize);
					fputs(AuxString, param);
					fclose(param);
					if( !(param=fopen("ClientParams/IntervalSendTime.param","w"))) {perror("Error in Open IntervalSendTime.param"); exit(1);}
					memset(AuxString, 0, 10);
					snprintf(AuxString, 10, "%d", IntervalSendTime);
					fputs(AuxString, param);
					fclose(param);

					/* Build [OKOK] Packet     *  
					 *     3rd Connection Step */
					memset(connPkt,0,CONN_SIZE);
					strncpy(connPkt, "OKOK", CONN_SIZE);
					usleep(TIMEPOLL);
					if ((sendto(sockConn, connPkt, CONN_SIZE, 0, (struct sockaddr *) addrServeServer, sizeof(*addrServeServer))) < 0) { perror("CLIENT: Error in SENDTO for CONN PACKET"); exit(1); }

					// log...
					snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> 3rd Conn. Step : Ack Sent, Connection Established\n",getpid(),sockConn);
					logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
					/* Connection Successfully Established */


					/* * * * * * * * * * * * * * * * *
					 * Start the Working Session     *
					 * * * * * * * * * * * * * * * * */

					// log...
					snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> Start Working Session\n",getpid());
					logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

					/* Initialize structures                                           *
					 *  >fileList and fileListSize are used to save the Server's files *
					 *  >fileRequested is used to save the file to download or upload  *
					 *  >filePath is used to save the exact file path                  *
					 *  >cmd is used to get the user command from user interface       *
					 *  >backtomenu and cmdNotFoun are auxiliary variables             */
					char * fileList[MAXFILENUM]; for(i=0;i<MAXFILENUM;i++) fileList[i]=malloc(CMDTXT_SIZE*sizeof(char));
					int fileListSize=0;
					char * fileRequested = malloc((CMDTXT_SIZE)*sizeof(char)); 
					char * filePath = malloc(CMDTXT_SIZE*sizeof(char));
					char * cmd = malloc(sizeof(char));
					int backtomenu=0;
					int errors = 0;
					int cicloList=0;
					while(1) {
						cmd = showConsoleUserInterface(cmd, errors, logcmdlist);
						errors = 0;

						/* Start Routines */

						/* Quit Routine */
						if( *cmd=='Q' || *cmd=='q') {
							printf("Starting closing procedure... :(\n"); fflush(stdout);
							usleep(TIMEPOLL);
							/* Build [QUIT][] PACKET */
							memset(cmdPkt, 0, CMDPKT_SIZE);
							strncpy(cmdPkt,"QUIT", CMDCMD_SIZE);
							usleep(TIMEPOLL);
							if ( sendto (sockConn, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrServeServer, sizeof(*addrServeServer)) < 0) {perror("CLIENT: Error in SENDTO for CMD PKT");exit(1);}
							// log...
							snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> Request: Send Quit Request\n",getpid(),sockConn);
							logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
							/* START Closing Procedure */
							close(sockConn);
							usleep(200000); printf("\n."); fflush(stdout); usleep(200000); printf(".");	fflush(stdout);	usleep(200000);	printf("."); fflush(stdout); usleep(200000);	printf("."); fflush(stdout); usleep(200000); printf("."); fflush(stdout);
							usleep(200000);   printf("."); fflush(stdout); usleep(200000); printf(".");	fflush(stdout); usleep(200000); printf("."); fflush(stdout); usleep(200000);	printf("."); fflush(stdout); usleep(200000); printf("."); fflush(stdout);
							printf( "\nSee you soon!!\n");
							kill(logPid, SIGINT);
							kill(getpid(), SIGINT);
							fcloseall();
							exit(1);
						}

						/* LIST  Routine */
						else if( *cmd=='L' || *cmd =='l') {
							// Log Command, auxiliary log for user interface
							logcmdlist = logcmd(logcmdlist, 0, NULL); 

							fileListSize=0;
							usleep(TIMEPOLL);
							/* Build [LIST][] Packet */
							memset(cmdPkt, 0, CMDPKT_SIZE);
							strncpy(cmdPkt,"LIST", CMDCMD_SIZE);
							usleep(TIMEPOLL);
							if ( sendto (sockConn, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrServeServer, sizeof(*addrServeServer)) < 0) {perror("CLIENT: Error in SENDTO for CMD PKT");exit(1);}
							// log...
							snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> Request: Send List Request\n",getpid(),sockConn);
							logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

							cicloList = 1;
							printf("--List-of-Files:--\n");
							fflush(stdout);
							servlen = sizeof(*addrServeServer);
							while(cicloList) {
								if ( recvfrom(sockConn, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrServeServer, &servlen) < 0) { perror("CLIENT: Error in SENDTO for CMD PKT"); exit(1);}

								if ( strncmp(cmdPkt,"DONE", CMDCMD_SIZE)  == 0) {
									/* Received [DONE][] Packet, the list of file is finished */
									cicloList = 0;
								}
								else if ( strncmp(cmdPkt,"NAME", CMDCMD_SIZE)  == 0) {
									/* Received [NAME][fileName] Packet, is a new Server File */
									fileList[fileListSize] = strncpy(fileList[fileListSize], cmdPkt+CMDCMD_SIZE, CMDTXT_SIZE);
									printf("-> \"%s\"\n",fileList[fileListSize]);
									fileListSize++;
									fflush(stdout);
								} 
								else {
									// log...
									snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> Error, received something wrong from Server in LIST Routine\n",getpid(),sockConn);
									logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
								}
							}
							printf("\n--End-------------\n...press <Enter> to continue... ");
							fflush(stdout);
							while(fgetc(stdin)!='\n'); // Block Screen
						} 

						/* GET  Routine */
						else if( *cmd=='G' || *cmd=='g') {
							/* choose file */
							backtomenu = 0;
							fileRequested = getCUIFileName(fileRequested);
							if ( strncmp(fileRequested,"..", 2)  == 0) {
								backtomenu=1;
							}
							if(!backtomenu) {
								// Log Command, auxiliary log for user interface
								logcmdlist = logcmd(logcmdlist, 1, fileRequested); 

								// log...
								snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> Request: Send Get \"%s\" Request\n",getpid(),sockConn, fileRequested);
								logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

								usleep(TIMEPOLL);
								pid_t trasfPid= fork();
								if(trasfPid == 0) {
									int RUDPstatus=0;
									/* Start RUDP GET Procedure */
									RUDPstatus = RUDP_GET(sockConn, addrServeServer, path, fileRequested, fifo_TOlog, IAMCLIENT);
									if ( RUDPstatus == 1 ) {
										// log...
										snprintf(buf_LOG+8, LOG_SIZE-8, "File \"%s\" Not Found!\n", fileRequested);
										logit( buf_LOG, fifo_TOlog, LOG_SIZE, NO_DEBUG, L1_LOG);
										exit(1);
									} else if ( RUDPstatus == 2 ) {
										// log...
										snprintf(buf_LOG+8, LOG_SIZE-8, "File \"%s\" Transfer Unsuccess, Generic Error!\n", fileRequested);
										logit( buf_LOG, fifo_TOlog, LOG_SIZE, NO_DEBUG, L1_LOG);
										exit(1);
									} else if ( RUDPstatus == 0 ) {
										// log...
										snprintf(buf_LOG+8, LOG_SIZE-8, "File \"%s\" Received Successfully!\n", fileRequested);
										logit( buf_LOG, fifo_TOlog, LOG_SIZE, NO_DEBUG, L1_LOG);
										exit(0);
									} else {
										exit(0);
									}
								} else {
									sleep(1);
								}
							}
						} 

						/* PUT  Routine */
						else if( *cmd=='P' || *cmd=='p') {

							/* Choose File */
							backtomenu = 0;
							fileRequested = getCUIFileName(fileRequested);

							if (
								fileRequested[0]=='.' && 
								fileRequested[1]=='.' ) 
							{
								backtomenu=1;
							}

							if(!backtomenu) {
								// Log Command, auxiliary log for user interface
								logcmdlist = logcmd(logcmdlist, 2, fileRequested); 
	
								// log...
								snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> Request: Send Put \"%s\" Request\n",getpid(),sockConn, fileRequested);
								logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

								/* * * * * * * * * * * * * * * * *
								 * Start the Transfer Session    *
								 * * * * * * * * * * * * * * * * */

								// log...
								snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> Start Transfer Session\n",getpid());
								logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

								usleep(TIMEPOLL);
								pid_t trasfPid= fork();
								if(trasfPid == 0) {
									int RUDPstatus=0;
									/* Start RUDP PUT Procedure */
									RUDPstatus = RUDP_PUT(sockConn, addrServeServer, path, fileRequested, fifo_TOlog, IAMCLIENT);
									if( RUDPstatus == 1 ) {
										// log...
										snprintf(buf_LOG+8, LOG_SIZE-8,"File \"%s\" Not Found!\n", fileRequested);
										logit( buf_LOG, fifo_TOlog, LOG_SIZE, NO_DEBUG, L1_LOG);
										exit(1);
									} else if ( RUDPstatus == 2 ) {
										// log...
										snprintf(buf_LOG+8, LOG_SIZE-8,"File \"%s\" Unsuccess, Generic Error!\n", fileRequested);
										logit( buf_LOG, fifo_TOlog, LOG_SIZE, NO_DEBUG, L1_LOG);
										exit(1);
									} else if( RUDPstatus == 0 ) {
										// log...
										snprintf(buf_LOG+8, LOG_SIZE-8,"File \"%s\" Sent Successfully!\n", fileRequested);
										logit( buf_LOG, fifo_TOlog, LOG_SIZE, NO_DEBUG, L1_LOG);
										exit(0);
									} else {
										exit(0);
									}
								} else {
									sleep(1);
								}
							}
						} 

						/* No Command Found Routine */
						else {
							errors = 1;
						}//end Command ifelse
					}//end Command while cycle

				} else {
					// NO, it is not [SETT]
					// log...
					snprintf(buf_LOG+8, LOG_SIZE-8, "%d:%d: -> ERR - 2nd ConnectionStep - Received something wrong: \"%s\", Closing...\n",getpid(),sockConn, connPkt);
					logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

					close(sockConn);
					usleep(200000);	printf("\n."); fflush(stdout); usleep(200000); printf(".");	fflush(stdout);	usleep(200000);	printf("."); fflush(stdout); usleep(200000);	printf("."); fflush(stdout); usleep(200000); printf("."); fflush(stdout);
					usleep(200000);   printf("."); fflush(stdout); usleep(200000); printf(".");	fflush(stdout); usleep(200000); printf("."); fflush(stdout); usleep(200000);	printf("."); fflush(stdout); usleep(200000); printf("."); fflush(stdout);
					printf( "\nSee you soon!!\n");
					kill(logPid, SIGINT);
					kill(getpid(), SIGINT);
					exit(1);
				}//end 2nd Connection Step ifelse

				ConnectionTry--; // Function not used
				sleep(1);
			}//end ConnectionTry while, Function now used

		printf("\n\n");
		fflush( stdout );
		}
	}//end Execution Mode
	return 1;
}//end Main



