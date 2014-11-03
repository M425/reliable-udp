#ifndef LISTP_H
#define	LISTP_H

typedef struct PIECED
 {
   long time;
   int id;
 } my_piece_d;


typedef struct NODEP
 {
   struct PIECED *element;
   struct NODEP *next;
   struct NODEP *prev;
 } my_node_p;

typedef struct LISTP
 {
	struct NODEP *first;
	struct NODEP *last;
  int numElem;
 }my_list_p;

    int delNodeP(my_list_p *list, int idsearch);
    my_piece_d * getMinP(my_list_p *list);
    my_list_p * createListP(my_list_p * list);
    int numElemP(my_list_p *list);
		void appendP(my_list_p *list, int idtoset, long timetoset);
    void displayP(my_list_p *list);
    long getFirst(my_list_p *list);
		void deleteFirst(my_list_p *list);

#endif	/* LISTP_H */

