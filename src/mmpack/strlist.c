/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmstring.h"
#include "strchunk.h"
#include "strlist.h"


/**
 * strlist_init() - init strlist structure
 * @list: strlist structure to initialize
 *
 * To be cleansed by calling strlist_deinit()
 */
LOCAL_SYMBOL
void strlist_init(struct strlist* list)
{
	*list = (struct strlist) {0};
}


/**
 * strlist_deinit() - cleanup strlist structure
 * @list: strlist structure to cleanse
 *
 * NOTE: this function is idempotent
 */
LOCAL_SYMBOL
void strlist_deinit(struct strlist* list)
{
	struct strlist_elt * elt, * next;

	elt = list->head;
	list->head = NULL;
	list->last = NULL;

	while (elt) {
		next = elt->next;
		free(elt);
		elt = next;
	}
}


LOCAL_SYMBOL
int strlist_add_strchunk(struct strlist* list, struct strchunk sv)
{
	struct strlist_elt* elt;

	// Create the new element
	elt = xx_malloc(sizeof(*elt) + sv.len + 1);
	elt->str.max = sv.len;
	elt->str.len = sv.len;
	memcpy(elt->str.buf, sv.buf, sv.len);
	elt->str.buf[sv.len] = '\0';
	elt->next = NULL;

	// Set as new head if list is empty
	if (list->head == NULL) {
		list->head = elt;
		list->last = elt;
		return 0;
	}

	// Add new element at the end of list
	list->last->next = elt;
	list->last = elt;
	return 0;
}


/**
 * strlist_add() - add string to the list
 * @list: initialized strlist structure
 * @str: string to add (standard char array)
 *
 * Return: always return 0
 */
LOCAL_SYMBOL
int strlist_add(struct strlist* list, const char* str)
{
	struct strchunk sv = {.buf = str, .len = strlen(str)};
	return strlist_add_strchunk(list, sv);
}


/**
 * strlist_drop_after() - remove element from list after the one specified
 * @list:       initialized strlist structure.
 * @prev:       element after which the element to drop appear. If NULL, the
 *              first element of the list must be dropped.
 */
LOCAL_SYMBOL
void strlist_drop_after(struct strlist* list, struct strlist_elt* prev)
{
	struct strlist_elt* elt;

	if (prev) {
		elt = prev->next;
		prev->next = elt->next;
	} else {
		elt = list->head;
		list->head = elt->next;
	}

	// Update last element if applicable
	if (!elt->next)
		list->last = prev;

	free(elt);
}


/**
 * strlist_remove() - remove string from list
 * @list: initialized strlist structure
 * @str: mmstr structure to remove
 */
LOCAL_SYMBOL
void strlist_remove(struct strlist* list, const mmstr* str)
{
	struct strlist_elt * elt, * prev;

	prev = NULL;
	elt = list->head;
	while (elt) {
		if (mmstrequal(elt->str.buf, str))
			break;

		prev = elt;
		elt = elt->next;
	}

	// Not found
	if (!elt)
		return;

	strlist_drop_after(list, prev);
}


