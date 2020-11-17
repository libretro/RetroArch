#ifndef _HELPERS_H_
#define _HELPERS_H_

typedef struct ListElement
{
	struct ListElement *next;
	void *item;
} ListElement;

ListElement *listElementPrepend(ListElement *head);
ListElement *listElementDelete(ListElement *head, ListElement *toDelNode, void(*itemDel)());
ListElement *listElementDeleteAll(ListElement *head, void(*itemDel)(void *item));
ListElement *listElementGet(ListElement *head, unsigned int id);
unsigned int listLength(ListElement *head);

#endif /* _HELPERS_H_ */
