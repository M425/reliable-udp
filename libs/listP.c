#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#include "listP.h"

my_list_p * createListP(my_list_p * list) {
  	if( (list=calloc(1, sizeof(my_list_p)) ) == NULL) {perror("ERRORE IN MALLOC");exit(1);}

  	list->first = NULL;
  	list->last = NULL;
  	list->numElem=0;
		
		return list;
}

int numElemP(my_list_p *list) {
	return list->numElem;
}

int delNodeP(my_list_p *list, int idsearch) {
	my_node_p *temp, *m;
	temp=list->first;
	m=list->first;
	while(temp) {
		if(temp->element->id==idsearch) {
			// FOUND
			list->numElem--;
   		if(temp==list->first) {
				if(!(temp->next))	{
   				list->first=NULL;
   				list->last=NULL;
				} else {
					list->first =temp->next;
					temp->next->prev = NULL;
				}
				temp->next=NULL;
				temp->prev=NULL;
   		 	free(temp);
				return 1;
			} else {
				if(temp==list->last) {   			
					m->next=NULL;
					list->last = m;
				} else {
					m->next = temp->next;
					temp->next->prev = m;
				}
				temp->next=NULL;
				temp->prev=NULL;
   			free(temp);
   			return 1;
   		}
		} else {
			m=temp;
   		temp=temp->next;
   	}
	}
	return 0;
}
my_piece_d * getMinP(my_list_p *list){
	my_node_p *temp;
	my_node_p *found;
	temp=list->first;
	found=list->first;
	long min;
	memcpy(&min,&(temp->element->time),sizeof(long));
	while(temp) {
		if(temp->element->time < min) {
			// FOUND
			memcpy(&min,&(temp->element->time),sizeof(long));
			found = temp;
		}
		temp = temp->next;
	}
	return found->element;
}


void appendP(my_list_p *list, int idtoset, long timetoset) {
	
//	printf("{%d.",idtoset);
	my_node_p *nodoToAdd, *nodoLast;
	my_piece_d * elemento;

	my_node_p * temp;

	if(!( nodoToAdd =    (my_node_p *)  malloc(sizeof(my_node_p))  )) {perror("errore in malloc");exit(1);}	
	if(!( elemento = (my_piece_d *) malloc(sizeof(my_piece_d)) )) {perror("errore in malloc");exit(1);}

	elemento->time=timetoset;
	memcpy(&(elemento->id),&(idtoset),sizeof(int));
	nodoToAdd->element=elemento;
	list->numElem++;
	if (!(list->first)) { // IF EMPTY
//	printf("ADD.%d->%dV}",elemento->id,list->numElem);
		list->first=nodoToAdd;
		list->first->next = NULL;
		list->first->prev = NULL;
		list->last=nodoToAdd;
		list->last->next = NULL;
		list->last->prev = NULL;

	} else {
//		printf("ADD.%d->%d}",elemento->id,list->numElem);
		nodoLast=list->last;
//		printf("(L:%d,",nodoLast->element->id);
//		printf("A:%d,",nodoToAdd->element->id);
		nodoToAdd->prev = nodoLast;
		nodoToAdd->next = NULL;
		nodoLast->next = nodoToAdd;
		list->last=nodoToAdd;
//		printf("L:%d)",list->last->element->id);
	}
	temp = list->first;
//	printf("[");
	while(temp) {
//		printf(".%d",temp->element->id);
		temp= temp->next;
	}
//	printf("]");
//	fflush(stdout);
}

void displayP(my_list_p *list)
{
	my_node_p *r = list->first;
  if(!r) {
	 	printf("NO ELEMENT IN THE LIST");
  } else {
   	while(r) {
   		printf("|<%d>-",r->element->id);
   		printf("%ld", r->element->time);
			r=r->next;
  	}
  }
	fflush(stdout);
}

void deleteFirst(my_list_p *list){
	my_node_p *temp2;
	my_node_p *temp1;
	if(list->first){
		list->numElem--;
		temp2=list->first;
		temp1=list->first->next;
		if(!temp1) {
			list->first=NULL;
			list->last=NULL;
		} else {
			temp1->prev=NULL;
			list->first=temp1;
		}
		temp2->next = NULL;
		temp2->prev = NULL;
		free(temp2);
	}
}
long getFirst(my_list_p *list) {
	if(!(list->first)) {
		return -1.0;
	} else {
		return list->first->element->time;
	}
	return 0;
}
