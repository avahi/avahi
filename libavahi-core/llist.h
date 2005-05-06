#ifndef foollistfoo
#define foollistfoo

#include <glib.h>

/* Some macros for maintaining doubly linked lists */

/* The head of the linked list. Use this in the structure that shall
 * contain the head of the linked list */
#define AVAHI_LLIST_HEAD(t,name) t *name

/* The pointers in the linked list's items. Use this in the item structure */
#define AVAHI_LLIST_FIELDS(t,name) t *name##_next, *name##_prev

/* Initialize the list's head */
#define AVAHI_LLIST_HEAD_INIT(t,head) do { (head) = NULL; } while(0)

/* Initialize a list item */
#define AVAHI_LLIST_INIT(t,name,item) do { \
                               t *_item = (item); \
                               g_assert(_item); \
                               _item->name##_prev = _item->name##_next = NULL; \
                               } while(0)

/* Prepend an item to the list */
#define AVAHI_LLIST_PREPEND(t,name,head,item) do { \
                                        t **_head = &(head), *_item = (item); \
                                        g_assert(_item); \
                                        if ((_item->name##_next = *_head)) \
                                           _item->name##_next->name##_prev = _item; \
                                        _item->name##_prev = NULL; \
                                        *_head = _item; \
                                        } while (0)

/* Remove an item from the list */
#define AVAHI_LLIST_REMOVE(t,name,head,item) do { \
                                    t **_head = &(head), *_item = (item); \
                                    g_assert(_item); \
                                    if (_item->name##_next) \
                                       _item->name##_next->name##_prev = _item->name##_prev; \
                                    if (_item->name##_prev) \
                                       _item->name##_prev->name##_next = _item->name##_next; \
                                    else {\
                                       g_assert(*_head == _item); \
                                       *_head = _item->name##_next; \
                                    } \
                                    _item->name##_next = _item->name##_prev = NULL; \
                                    } while(0)

#endif
