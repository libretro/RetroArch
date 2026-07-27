/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LIBNFS_SLIST_H_
#define _LIBNFS_SLIST_H_

#define LIBNFS_LIST_ADD(list, item) \
	do {							\
		(item)->next = (*list);				\
		(*list) = (item);				\
	} while (0);

#define LIBNFS_LIST_ADD_END(list, item)				\
	if ((*list) == NULL) {	 				\
	   LIBNFS_LIST_ADD((list), (item));				\
	} else {						\
	   void *head = (*list);				\
	   while ((*list)->next)				\
	     (*list) = (*list)->next;				\
	   (*list)->next = (item);				\
	   (item)->next = NULL;					\
	   (*list) = head;					\
	}

#define LIBNFS_LIST_REMOVE(list, item) \
	if ((*list) == (item)) { 				\
	   (*list) = (item)->next;				\
	} else {						\
	   void *head = (*list);				\
	   while ((*list)->next && (*list)->next != (item))     \
	     (*list) = (*list)->next;				\
	   if ((*list)->next != NULL) {		    	    	\
	      (*list)->next = (*list)->next->next;		\
	   }  		      					\
	   (*list) = head;					\
	}

#endif /* !_LIBNFS_SLIST_H_ */
