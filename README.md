reliable-udp
============

Simple and dirty file sharing app, using Reliable transmission over UDP (selective repeat)

RUDPTransf_Recver.c,RUDPTransf_Sender.c and libs/libRUDP-trasnsfer_sender.c,libs/libRUDP-trasnsfer_recver.c contain the basic functions to open UDP sockets 
and carry out the reliable UDP protocol using "Selective Repeat" technique.

The transfer is a multi-thread (pthread) operation.

The file sharing code is a simple multi-process application.


