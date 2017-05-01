/*
 * $Id: linklist.c,v 2.3 1996/10/15 20:16:35 hzoli Exp $
 *
 * linklist.c - linked lists
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

/*
Null list: node (node->next, node->last) = not created,
					 list->first = NULL, list->last = list,
					 [(LinkNode)list->next = NULL, (LinkNode)list->last = list]

One element list: node->next = NULL, node->last = list,
           list->first = node, list->last = node,
           [(LinkNode)list->next = node, (LinkNode)list->last = node]

Three-elements list:

list->last=n1=V
NULL		    	n1					n2						n3						list
    		    	n1->last=n2=^	n2->last=n3=^ n3->last=list=^  >>>> LAST
  ^=0=n1->next n1=n2->next  n2=n3->next                    <<<< NEXT
  																			n3=list->first
[(LinkNode)list->next = list->first=n3, (LinkNode)list->last = n1]
*/

#include "zsh.h"

/* Get an empty linked list header */

/**/
LinkList
newlinklist(void)
{
    LinkList list;

    list = (LinkList) alloc(sizeof *list);
    list->first = NULL;
    list->last = (LinkNode) list;
    return list;
}

/* Insert a node in a linked list after a given node */

/**/
void
insertlinknode(LinkList list, LinkNode node, void *dat)
{
    LinkNode tmp;

    tmp = node->next;
    node->next = (LinkNode) alloc(sizeof *tmp);
    node->next->last = node;
    node->next->dat = dat;
    node->next->next = tmp;
    if (tmp)
	tmp->last = node->next;
    else
	list->last = node->next;
}


/**/
void
WalkList(LinkList list)
{
    LinkNode node;

    printf("list = %p\n",list);
    printf("list->next = %p\n",((LinkNode)list)->next);
    printf("list->last = %p\n",((LinkNode)list)->last);
		if (! ((LinkNode)list)->next) {
	    printf("first = %p\n",list->first);
	    printf("last  = %p\n",list->last);
			return;
		}
    printf("first = %p = %s\n",list->first,list->first->dat);
    printf("last  = %p = %s\n",list->last,list->last->dat);
    printf("-------\n");
    for (node = list->first; node; node=node->next) // read direction (pop=first to last)
    	printf("node=%p next=%p last=%p dat=%s\n",node, node->next, node->last, node->dat);
    printf("-------\n");
    for (node = list->last;node!= (LinkNode)list; node=node->last) // print direction (push=last to first)
    	printf("node=%p next=%p last=%p dat=%s\n",node, node->next, node->last, node->dat);
    printf("-------\n");
}

/* Insert a node as the first one of a linked list */

/**/
void
insertFirstLinkNode(LinkList list, void *dat)
{
    LinkNode node, end=list->last;

//WalkList(list);
    node = (LinkNode) alloc(sizeof *node);
    node->dat = dat;
    node->next = end->next;
    node->last = end;
    end->next=node;
    list->last=node;
//WalkList(list);
    return;
}

/* Insert a list in another list */

/**/
void
insertlinklist(LinkList l, LinkNode where, LinkList x)
{
    LinkNode nx;

    nx = where->next;
    if (!l->first)
	return;
    where->next = l->first;
    l->last->next = nx;
    l->first->last = where;
    if (nx)
	nx->last = l->last;
    else
	x->last = l->last;
}

/* Get top node in a linked list */

/**/
void *
getlinknode(LinkList list)
{
    void *dat;
    LinkNode node;

    if (!(node = list->first))
	return NULL;
    dat = node->dat;
    list->first = node->next;
    if (node->next)
	node->next->last = (LinkNode) list;
    else
	list->last = (LinkNode) list;
    zfree(node, sizeof(struct linknode));
    return dat;
}

/* Get bottom node in a linked list */

/**/
void *
getFirstLinknode(LinkList list)
{
    void *dat;
    LinkNode node;

//WalkList(list);
    if ((node = list->last) == (LinkNode) list)
	return NULL;
    dat = node->dat;
    list->last = node->last;
		if (node->last == (LinkNode) list)
			list->first = node->next;
		else
			node->last->next = node->next;
    zfree(node, sizeof(struct linknode));
    return dat;
}

/* Get top node in a linked list without freeing */

/**/
void *
ugetnode(LinkList list)
{
    void *dat;
    LinkNode node;

    if (!(node = list->first))
	return NULL;
    dat = node->dat;
    list->first = node->next;
    if (node->next)
	node->next->last = (LinkNode) list;
    else
	list->last = (LinkNode) list;
    return dat;
}

/* Remove a node from a linked list */

/**/
void *
remnode(LinkList list, LinkNode nd)
{
    void *dat;

    nd->last->next = nd->next;
    if (nd->next)
	nd->next->last = nd->last;
    else
	list->last = nd->last;
    dat = nd->dat;
    zfree(nd, sizeof(struct linknode));

    return dat;
}

/* Remove a node from a linked list without freeing */

/**/
void *
uremnode(LinkList list, LinkNode nd)
{
    void *dat;

    nd->last->next = nd->next;
    if (nd->next)
	nd->next->last = nd->last;
    else
	list->last = nd->last;
    dat = nd->dat;
    return dat;
}

/* Free a linked list */

/**/
void
freelinklist(LinkList list, FreeFunc freefunc)
{
    LinkNode node, next;

    for (node = list->first; node; node = next) {
	next = node->next;
	if (freefunc)
	    freefunc(node->dat);
	zfree(node, sizeof(struct linknode));
    }
    zfree(list, sizeof(struct linklist));
}

/* Count the number of nodes in a linked list */

/**/
int
countlinknodes(LinkList list)
{
    LinkNode nd;
    int ct = 0;

    for (nd = firstnode(list); nd; incnode(nd), ct++);
    return ct;
}

/**/
void
rolllist(LinkList l, LinkNode nd)
{
    l->last->next = l->first;
    l->first->last = l->last;
    l->first = nd;
    l->last = nd->last;
    nd->last = (LinkNode) l;
    l->last->next = 0;
}

