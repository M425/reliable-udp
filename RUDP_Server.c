#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

#include "libs/libRUDP-generic.h"
#include "libs/libRUDP-constants.h"

int main(int argc, char **argv) {
	if(argc>1) {
	int error=0;
/******* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 ****                                                                  *
 **                       CONFIGURATION MODE                          **
 *                                                                  ****
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *******/
		if(argc==4) {
			if(!(strcmp(argv[1],"-Config")) || !(strcmp(argv[1],"-config")) || !(strcmp(argv[1],"-C")) || !(strcmp(argv[1],"-c")) ) {

				// Change Shared Folder Path
				if( !(strcmp(argv[2],"Path")) || !(strcmp(argv[2],"path")) ) 
				{
					FILE * param;
					mkdir("ServerParams", 0777);
					if( !(param=fopen("ServerParams/ServerFolderPath.param","w+"))){perror("Error to Retrieve ServerFolderPath.param"); exit(1);}
					if( !(strcmp(argv[3], "Default")) || !(strcmp(argv[3], "default")) ) {
						fputs(SHARED_FOLDER, param);
					} else {
						fputs(argv[3], param);
					}
				}
				// Change Size of Selective Repeat Window
				else if ( !(strcmp(argv[2],"WinSize")) || !(strcmp(argv[2],"winsize")) )
				{
					FILE * param;
					mkdir("ServerParams", 0777);
					if( !(param=fopen("ServerParams/WindowSize.param","w+"))) {perror("Error to Retrieve WindowSize.param"); exit(1);}
					if( !(strcmp(argv[3], "Default")) || !(strcmp(argv[3], "default")) ) {
						char * winsize = malloc(10*sizeof(char));
						snprintf(winsize, 10, "%d", TRANSFERWINDOW_SIZE);
						fputs(winsize, param);
					} else {
						fputs(argv[3], param);
					}
				}
				// Change Connection Port
				else if ( !(strcmp(argv[2],"ConnPort")) || !(strcmp(argv[2],"connport")) )
				{
					FILE * param;
					mkdir("ServerParams", 0777);
					if( !(param=fopen("ServerParams/ConnPort.param","w+"))) {perror("Error to Retrieve ConnPort.param"); exit(1);}
					if( !(strcmp(argv[3], "Default")) || !(strcmp(argv[3], "default")) ) {
						char * connectionport = malloc(10*sizeof(char));
						snprintf(connectionport, 10, "%d", SERVCONNPORT);
						fputs(connectionport, param);
					} else {
						fputs(argv[3], param);
					}
				}
				// Change Connection Port
				else if ( !(strcmp(argv[2],"IntTime")) || !(strcmp(argv[2],"inttime")) )
				{
					FILE * param;
					mkdir("ServerParams", 0777);
					if( !(param=fopen("ServerParams/IntervalSendTime.param","w+"))) {perror("Error to Retrieve IntervalSendTime.param"); exit(1);}
					if( !(strcmp(argv[3], "Default")) || !(strcmp(argv[3], "default")) ) {
						char * inttime = malloc(10*sizeof(char));
						snprintf(inttime, 10, "%d", INTERVALSENDTIME);
						fputs(inttime, param);
					} else {
						fputs(argv[3], param);
					}
				}
				// Change Loss Probability
				else if ( !(strcmp(argv[2],"LossProb")) || !(strcmp(argv[2],"lossprob")) )
				{
					FILE * param;
					mkdir("ServerParams", 0777);
					if( !(param=fopen("ServerParams/LossProb.param","w+"))) {perror("Error to Retrieve LossProb.param"); exit(1);}
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
			printf("#-----Wrong-use------\n");
			printf("# Configuration Mode:\n");
			printf("#   %s -Config Path     \"path\"   To Change Shared Path Directory\n", argv[0]); 
			printf("#   %s -Config WinSize  \"size\"   To Change Selective Repeat Window Size\n", argv[0]);
			printf("#   %s -Config ConnPort \"port\"   To Change Connection Port\n", argv[0]);
			printf("#   %s -Config IntTime  \"time\"   To Change The Interval Time between 2 Send\n", argv[0]);
			printf("#   %s -Config LossProb \"prob\"   To Change Packet Loss Pobability\n", argv[0]);
			printf("#   \"default\" To Change with Default Value:\n");
			printf("# Start Mode:\n");
			printf("#   %s\n", argv[0]);  
		}
	} else {
/******* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 ****                                                                  *
 **                       EXECUTION MODE                              **
 *                                                                  ****
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *******/

		/* Initialize Log Manager and his Fifo*/	
		mkdir("tmp",0777);
		if(mkfifo(SERVER_FIFO_TO_LOG, 0666)) { 
			if(errno!=EEXIST) { perror("SERVER: Cannot create SERVER_FIFO_TO_LOG"); exit(1); } 
		}

		pid_t logPid = fork();
		if(logPid == 0) { 
			// This is LOG Process
			srvr_logManager();
		} else {
			// This is SERVER Process
			signal(SIGINT, closing_server);

			/* Sync LogManager Fifo*/
			char * buf_LOG = calloc(LOG_SIZE,sizeof(char));
			int fifo_TOlog = open( SERVER_FIFO_TO_LOG, O_WRONLY );
			if(fifo_TOlog<0) { perror("SERVER: Cannot open fifo_TOlog"); exit(1); }

			// log...
			snprintf(buf_LOG+8, LOG_SIZE-8, "\n\n\n\n\n\n\n\n\n\n\n\n\n\nStarting...\nFor Quit, Send SIGINT [Ctrl+C]\n----====== PARAMETERS ======----\n");
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, 3);

			/* Get Parameters from params Files */
			FILE * param;
			short connPort;
			int WindowSize;
			int IntervalSendTime;
			double lossProb;
			char * path = calloc(200,sizeof(char));
			char * AuxString = calloc(10,sizeof(char));
			if( !(param=fopen("ServerParams/ServerFolderPath.param","r"))){
				path = SHARED_FOLDER;
				// log...
				snprintf(buf_LOG+8, LOG_SIZE-8, "Error to Retrieve ServerFolderPath.param, setted as Default: %s\n", path);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				mkdir("ServerParams",0777);
				mkdir(path,0777);
				param=fopen("ServerParams/ServerFolderPath.param","w");
				fputs(path, param);
				fclose(param);
			} else {
				fgets(path, 200, param);
				// log...
				snprintf(buf_LOG+8, LOG_SIZE-8, "Shared Folder Path: %s\n", path);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				mkdir(path,0777);
				fclose(param);
			}
			if( !(param=fopen("ServerParams/ConnPort.param","r"))){
				connPort = SERVCONNPORT;
				// log...
				snprintf(buf_LOG+8, LOG_SIZE-8, "Error to Retrieve ConnPort.param, setted as Default: %d\n", connPort);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				mkdir("ServerParams",0777);
				snprintf(AuxString, 10, "%d", SERVCONNPORT);
				param=fopen("ServerParams/ConnPort.param","w");
				fputs(AuxString, param);
				fclose(param);
			} else {
				memset(AuxString, 0, 10);
				fgets(AuxString, 10, param);
				connPort = (short) atoi(AuxString);
				// log...
				snprintf(buf_LOG+8, LOG_SIZE-8, "Connection Port:    %d\n", connPort);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				fclose(param);
			}
			if( !(param=fopen("ServerParams/WindowSize.param","r"))){
				WindowSize = TRANSFERWINDOW_SIZE;
				// log...
				snprintf(buf_LOG+8, LOG_SIZE-8, "Error to Retrieve WindowSize.param, setted as Default: %d\n", WindowSize);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				mkdir("ServerParams",0777);
				snprintf(AuxString, 10, "%d", TRANSFERWINDOW_SIZE);
				param=fopen("ServerParams/WindowSize.param","w");
				fputs(AuxString, param);
				fclose(param);
			} else {
				memset(AuxString, 0, 10);
				fgets(AuxString, 10, param);
				WindowSize = atoi(AuxString);
				// log...
				snprintf(buf_LOG+8, LOG_SIZE-8, "Window Size:        %d\n",WindowSize);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				fclose(param);
			}
			if( !(param=fopen("ServerParams/IntervalSendTime.param","r"))){
				IntervalSendTime = INTERVALSENDTIME;
				// log...
				snprintf(buf_LOG+8, LOG_SIZE-8, "Error to Retrieve IntervalSendTime.param, setted as Default: %d\n", IntervalSendTime);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				mkdir("ServerParams",0777);
				snprintf(AuxString, 10, "%d", INTERVALSENDTIME);
				param=fopen("ServerParams/IntervalSendTime.param","w");
				fputs(AuxString, param);
				fclose(param);
			} else {
				memset(AuxString, 0, 10);
				fgets(AuxString, 10, param);
				IntervalSendTime = atoi(AuxString);\
				// log...
				snprintf(buf_LOG+8, LOG_SIZE-8, "IntervalSendTime:   %d\n",IntervalSendTime);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				fclose(param);
			}
			if( !(param=fopen("ServerParams/LossProb.param","r"))){
				lossProb = LOSS_PROB;
				// log...
				snprintf(buf_LOG+8, LOG_SIZE-8, "Error to Retrieve LossProb.param, setted as Default: %f\n", lossProb);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				mkdir("ServerParams",0777);
				snprintf(AuxString, 10, "%f", LOSS_PROB);
				param=fopen("ServerParams/LossProb.param","w");
				fputs(AuxString, param);
				fclose(param);
			} else {
				memset(AuxString, 0, 10);
				fgets(AuxString, 10, param);
				lossProb = atof(AuxString);
				// log...
				snprintf(buf_LOG+8, LOG_SIZE-8, "Loss Probability:   %f\n",lossProb);
				logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				fclose(param);
			}
			// log...
			snprintf(buf_LOG+8, LOG_SIZE-8, "----======xxxxxxxxxxxx======----\n");
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

			/* * * * * * * * * * * * * * * * * * * * * * * * * *
			 *           Start Connection Session              *
			 * * * * * * * * * * * * * * * * * * * * * * * * * */

			/* Creates Socket to Accept Connections of Clients *
			 *  Bind it in 'connPort' Port                     *
			 *          in Any Address                         */
			char * connPkt = calloc(CONN_SIZE, sizeof(char));
			int sockConn;
			if( (sockConn = socket (AF_INET, SOCK_DGRAM, 0)) < 0) { perror("SERVER: Cannot Create Socket for Connection"); exit(1); }
			struct sockaddr_in * addrConnServer = malloc(sizeof(struct sockaddr_in));
			memset((void *)addrConnServer, 0, sizeof(*addrConnServer));
			addrConnServer->sin_family = AF_INET;
			addrConnServer->sin_addr.s_addr = htonl(INADDR_ANY);
			addrConnServer->sin_port = htons(connPort);
			if( bind (sockConn, (struct sockaddr *)addrConnServer, sizeof(*addrConnServer)) < 0) { perror("SERVER: Cannot Bind Connection Socket"); exit(1); }
		
			// log...
			snprintf(buf_LOG+8, LOG_SIZE-8, "%d:%d: -> Connection Socket Created [port = %d]\n",getpid(), sockConn,ntohs(addrConnServer->sin_port));
			logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

			/* Prepare Structures for next Client         *
			 * This will be filled with Client Parameters */
			struct sockaddr_in * addrConnClient = malloc(sizeof(struct sockaddr_in));
			socklen_t len = sizeof(*addrConnClient);
			pid_t pidSons = 0;

			/* Listen and Handle every new connection *
			 *  Block Process to listen to the socket */
			while(1) {
				if ( recvfrom (sockConn, connPkt, CONN_SIZE, 0, (struct sockaddr *) addrConnClient, &len) < 0) { perror("SERVER: Error in Recvfrom in 1st Connection Step"); exit(1); }

				if(	strncmp(connPkt,"CONN", CONN_SIZE) == 0 ) {
					/* Received [CONN] Packet, *  
					 *     1st Connection Step */

					// log...
					snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> 1st Connection Step: Received a Connection Request\n",getpid(), sockConn);
					logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

					pidSons = fork();

					if(pidSons == 0 ) {
						/* This process will handle                 *
						 *    the connection of the new Client      *
						 *    all command request of the new Client */

						/* Start Handle Connection Session  */
						/* Creates Socket to Serve the new Client *
						 *  Bind it in new Random Port            */
						struct sockaddr_in * addrServeServer = malloc(sizeof(struct sockaddr_in));
						memset((void *)addrServeServer, 0, sizeof(*addrServeServer));
						addrServeServer->sin_family = AF_INET;
						addrServeServer->sin_addr.s_addr = htonl(INADDR_ANY);
						socklen_t len = sizeof(*addrServeServer);		
						int sockServe;
						if ( (sockServe = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { perror("SERVER: Error in socket SERVICE SOCKET"); exit(1); }
						if ( bind(sockServe, (struct sockaddr *)addrServeServer, sizeof(*addrServeServer)) < 0) { perror("SERVER: Cannot Bind Serve Socket"); exit(1); }

						/* Build [SETT][TransferWindowSize][IntervalSendTime] *  
						 *     2nd Connection Step                            */
						char * cmdPkt = calloc(CMDPKT_SIZE, sizeof(char));
						strncpy(cmdPkt, "SETT", CONN_SIZE);
						memcpy(cmdPkt+4,&WindowSize, sizeof(int));
						memcpy(cmdPkt+8,&IntervalSendTime, sizeof(int));
						usleep(TIMEPOLL);
						if ( sendto (sockServe, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrConnClient, len) < 0) { perror("SERVER: Error in Send in 2nd Connection Step"); exit(1); }
						// log...
						snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> 2nd Connection Step: Created Serve Socket and Sent Connection Setting Packets\n",getpid(), sockServe);
						logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
					
						// Wait for the response
						if ( recvfrom (sockServe, connPkt, CONN_SIZE, 0, (struct sockaddr *) addrConnClient, &len) < 0) {perror("SERVER: Error in recvfrom in 3rd Connection Step");exit(1);}

						if ( strncmp(connPkt, "OKOK", CONN_SIZE) == 0) {
							/* Received [OKOK] Packet, *  
							 *     3rd Connection Step */

							// log...
							snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> 3rd Connection Step: Received Ack from user\n",getpid(), sockServe);
							logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

							// log...
							snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> Accepted, new User active [ip = %s] [port = %d]\n",getpid(),inet_ntoa(addrConnClient->sin_addr), ntohs(addrConnClient->sin_port));
							logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
							snprintf(buf_LOG, LOG_SIZE,"USR<%s : %d> Handle by %d\n",inet_ntoa(addrConnClient->sin_addr), ntohs(addrConnClient->sin_port), getpid());
							if(LOG_TO_FILE>NO_LOG) {  write (fifo_TOlog, buf_LOG, LOG_SIZE ); }

							/* * * * * * * * * * * * * * * * * * * * * * * * * * *
							 *           Start Working Session                   *
							 * now this process will handle the Client Commands  *
							 * * * * * * * * * * * * * * * * * * * * * * * * * * */
							char * fileName = calloc(CMDTXT_SIZE, sizeof(char));
							char * filePath = calloc(CMDTXT_SIZE, sizeof(char));
							int RUDPstatus=0;
							int n=0;
							DIR *dir;
							struct dirent ** namelist;

							while(1) {
								// Wait for Next Command Request
								if ( recvfrom (sockServe, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrConnClient, &len) < 0) {perror("SERVER: Error in recvfrom for new cmd Packet");exit(1);}
								// log...
								snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> Received new Command Packet [cmdPkt = %s]\n",getpid(), sockServe, cmdPkt);
								logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);

								/* start QUIT ROUTINE */
								if (strncmp(cmdPkt, "QUIT", CMDCMD_SIZE) == 0) {
									/* Received [QUIT] Command Packet */
									close(sockServe);
									// log...
									snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> one user leave [ip = %s] [port = %d]\n",getpid(), sockServe, inet_ntoa(addrConnClient->sin_addr), ntohs(addrConnClient->sin_port));
									logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
									snprintf(buf_LOG, LOG_SIZE,"EXT<%s : %d> Handled by %d\n",inet_ntoa(addrConnClient->sin_addr), ntohs(addrConnClient->sin_port), getpid());
									if(LOG_TO_FILE>NO_LOG) {  write (fifo_TOlog, buf_LOG, LOG_SIZE ); }
									exit(0); // end process
								}

								/* start LIST ROUTINE */
								else if (strncmp(cmdPkt, "LIST", CMDCMD_SIZE) == 0) {
									/* Received [LIST] Command Packet */
									n = scandir(path, &namelist, 0, alphasort); 
									strncpy(cmdPkt,"NAME", CMDCMD_SIZE);
									while (n--) {
										if(*(namelist[n]->d_name)!='.') {
											usleep(TIMEPOLL);
											memcpy(cmdPkt+CMDCMD_SIZE, namelist[n]->d_name, CMDTXT_SIZE);
											if((sendto(sockServe, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrConnClient, sizeof(*addrConnClient))) < 0) { perror("SERVER: List Routine -> Error in SendTo"); exit(1); }
										}
									}
									memset(cmdPkt, 0, CMDPKT_SIZE);
									strncpy(cmdPkt,"DONE", CMDCMD_SIZE);
									usleep(TIMEPOLL);
									if ((sendto(sockServe, cmdPkt, CMDPKT_SIZE, 0, (struct sockaddr *) addrConnClient, sizeof(*addrConnClient))) < 0) { perror("SERVER: List Routine -> Error in SendTo"); exit(1); }
								}

								/* start GET  ROUTINE */
								else if (strncmp(cmdPkt, "GET ", CMDCMD_SIZE) == 0) {
									/* Received [GET ][FileName] Command Packet */
									memcpy(fileName, cmdPkt+CMDCMD_SIZE, CMDTXT_SIZE);
									// Building filePath: "FolderPath/fileName"
									memset(filePath, 0, CMDTXT_SIZE);
									strncpy(filePath, path, CMDTXT_SIZE);
									strncat(filePath, fileName, CMDTXT_SIZE); 
									pid_t trasfPid = fork();
									if(trasfPid == 0) {
										/* This Process will handle the File Transfer */
										RUDPstatus = RUDP_Send(sockServe, addrConnClient, filePath, fifo_TOlog, IAMSERVER);
										if( RUDPstatus == 1 ) {
											// log...
											snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> File \"%s\" Not Found, Abort Operation!\n",getpid(), fileName);
											logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
											exit(1);
										} else if ( RUDPstatus == 2 ) {
											// log...
											snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> Send of \"%s\" Unsuccess, Generic Error!\n", getpid(), fileName);
											logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
											exit(1);
										} else if( RUDPstatus == 0 ) {
											// log...
											snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> File \"%s\" Sent Successfully!\n", getpid(), fileName);
											logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
											exit(0);
										} else {
											exit(0);
										}
									} else {
										usleep(TIMEPOLL); usleep(TIMEPOLL); usleep(TIMEPOLL); usleep(TIMEPOLL); usleep(TIMEPOLL);
									}		
								}

								/* start PUT  ROUTINE */
								else if (strncmp(cmdPkt, "PUT ", CMDCMD_SIZE) == 0) {
									/* Received [PUT ][FileName] Command Packet */
									/* start PUT  ROUTINE */
									memcpy(fileName, cmdPkt+CMDCMD_SIZE, CMDTXT_SIZE);
									pid_t trasfPid = fork();
									if(trasfPid == 0) {
										/* This Process will handle the File Transfer */
										int RUDPstatus = 0;
										memset(filePath, 0, CMDTXT_SIZE);
										strcpy(filePath, path);
										strcat(filePath, fileName); // FolderPath+fileName
										RUDPstatus = RUDP_Recv(sockServe, addrConnClient, filePath, fifo_TOlog, IAMSERVER);
										if ( RUDPstatus == 1 ) {
											// log...
											snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> Send of \"%s\" Aborted by Client!\n", getpid(), fileName);
											logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
											exit(1);
										} else if ( RUDPstatus == 2 ) {
											// log...
											snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> Send of \"%s\" Unsuccess, Generic Error!\n", getpid(), fileName);
											logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
											exit(1);
										} else if ( RUDPstatus == 0 ) {
											// log...
											snprintf(buf_LOG+8, LOG_SIZE-8,"%d: : -> File \"%s\" Received Successfully!\n", getpid(), fileName);
											logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
											exit(0);
										} else {
											exit(0);
										}
									} else {
										usleep(TIMEPOLL); usleep(TIMEPOLL); usleep(TIMEPOLL); usleep(TIMEPOLL); usleep(TIMEPOLL);
									}
								}

								/* no routine found */
								else {
									// log...
									snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> ERR - Error command receive: No command found <%s>\n", getpid(), sockServe, cmdPkt);
									logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
								}//end Command ifelse

							}//end Command Listening while

						} else {
							// log...
							snprintf(buf_LOG+8, LOG_SIZE-8,"%d:%d: -> ERR - Error in receive 3rd connection step <%s>\n", getpid(), sockServe, connPkt);
							logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
						}//end 3rd Connection Step ifelse

					}//end Connection Handler pid
					  else {
						// This is the Connection Pid, do nothing
						// Continue to Listen sockConn for other Connection Request
					}

				} else {
					// NO, connPkt is not [CONN]
					// log...
					snprintf(buf_LOG+8, LOG_SIZE-8, "%d:%d: -> ERR - Received Something Wrong from Connection Socket, Reject \"%s\" \n",getpid(), sockConn, connPkt);
					logit( buf_LOG, fifo_TOlog, LOG_SIZE, L1_DEBUG, L1_LOG);
				}//end 1st Connection Step  ifelse

			}//end Connection Listening while
			return 1;
		
		}//end Connection Pid

	}//end Execution Mode
}//end Main

