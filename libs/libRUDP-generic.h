#ifndef LIBRUDPGENERIC_H
#define LIBRUDPGENERIC_H

#include "libRUDP-transfer.h"

 char * showConsoleUserInterface( char * cmd, int errors, LogCommandsList * logcmdlist);
 char * getCUIFileName(char * fileRequested);
 void showCUIMenu(int err, LogCommandsList * logcmdlist);

 void clnt_logManager(char * logfifoname);
 void srvr_logManager();
 void closing_clnt_srvr_LogManager();
 void closing_client();
 void closing_server();
 void logit(char * buffer, int fifo_TOlog, int size, int debug, int flog);
 long fileSize(char * FileName);

#endif /* LIBRUDPGENERIC_H */
