#include "helpers.h"
#include <stdlib.h>

/* Linked list */

ListElement *listElementPrepend(ListElement *head)
{
	ListElement *newNode = malloc(sizeof(ListElement));
	if(!newNode)
	{
		return head;
	}

	newNode->next = head;
	return newNode;

}

ListElement *listElementDelete(ListElement *head, ListElement *toDelNode, void(*itemDel)(void *item))
{
	ListElement *prevNode = NULL;
	ListElement *curNode = head;

	while(curNode)
	{
		if(curNode == toDelNode)
		{
			if(!prevNode)
			{
				head = curNode->next;
			}
			else
			{
				prevNode->next = curNode->next;
			}

			itemDel(curNode->item);
			free(curNode);
			return head;
		}
		prevNode = curNode;
		curNode = curNode->next;
	}

	return head;
}

ListElement *listElementDeleteAll(ListElement *head, void(*itemDel)(void *item))
{
	ListElement *curNode = head;

	while(curNode)
	{
		ListElement *toDelNode = curNode;
		curNode = curNode->next;

		itemDel(toDelNode->item);
		free(toDelNode);
	}

	return NULL;
}

ListElement *listElementGet(ListElement *head, unsigned int id)
{
	ListElement *curNode = head;
	int i = 0;

	while(curNode)
	{
		if (i == id)
			return curNode;

		curNode = curNode->next;
		++i;
	}

	return NULL;
}

unsigned int listLength(ListElement *head)
{
	ListElement *curNode = head;
	unsigned int count = 0;

	while (curNode)
	{
		curNode = curNode->next;
		++count;
	}

	return count;
}
