#ifndef _LINUX_WAIT_H
#define _LINUX_WAIT_H

#define WNOHANG		0x00000001
#define WUNTRACED	0x00000002

#define __WALL		0x40000000	/* Wait on all children, regardless of type */
#define __WCLONE	0x80000000	/* Wait only on non-SIGCHLD children */ 

#ifdef __KERNEL__

#include <asm/page.h>

struct wait_queue {
	struct task_struct * task;
	struct wait_queue * next;
};

typedef struct wait_queue wait_queue_t;
typedef struct wait_queue *wait_queue_head_t;

#define WAIT_QUEUE_HEAD(x) ((struct wait_queue *)((x)-1))
#define DECLARE_WAITQUEUE(wait, current)	struct wait_queue wait = { current, NULL }
#define DECLARE_WAIT_QUEUE_HEAD(wait)		wait_queue_head_t wait
#define init_waitqueue_head(x)			*(x)=NULL
#define init_waitqueue_entry(q,p)		((q)->task)=(p)

static inline void init_waitqueue(struct wait_queue **q)
{
	*q = WAIT_QUEUE_HEAD(q);
}

static inline int waitqueue_active(struct wait_queue **q)
{
	struct wait_queue *head = *q;
	return head && head != WAIT_QUEUE_HEAD(q);
}

struct select_table_entry {
	struct wait_queue wait;
	struct wait_queue ** wait_address;
};

typedef struct select_table_struct {
	int nr;
	struct select_table_entry * entry;
} select_table;

#define __MAX_SELECT_TABLE_ENTRIES (4096 / sizeof (struct select_table_entry))

#endif /* __KERNEL__ */

#endif
