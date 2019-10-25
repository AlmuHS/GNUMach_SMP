#include <device/intr.h>
#include <device/ds_routines.h>
#include <kern/queue.h>
#include <kern/printf.h>

#ifndef MACH_XEN
// TODO this is only for x86 system
#define sti() __asm__ __volatile__ ("sti": : :"memory")
#define cli() __asm__ __volatile__ ("cli": : :"memory")

static boolean_t deliver_intr (int line, ipc_port_t dest_port);

struct intr_entry
{
  queue_chain_t chain;
  ipc_port_t dest;
  int line;
  /* The number of interrupts occur since last run of intr_thread. */
  int interrupts;
};

static queue_head_t intr_queue;
/* The total number of unprocessed interrupts. */
static int tot_num_intr;

static struct intr_entry *
search_intr (int line, ipc_port_t dest)
{
  struct intr_entry *e;
  queue_iterate (&intr_queue, e, struct intr_entry *, chain)
    {
      if (e->dest == dest && e->line == line)
	return e;
    }
  return NULL;
}

/* This function can only be used in the interrupt handler. */
void
queue_intr (int line, ipc_port_t dest)
{
  extern void intr_thread ();
  struct intr_entry *e;
  
  cli ();
  e = search_intr (line, dest);
  assert (e);
  e->interrupts++;
  tot_num_intr++;
  sti ();

  thread_wakeup ((event_t) &intr_thread);
}

/* insert an interrupt entry in the queue.
 * This entry exists in the queue until
 * the corresponding interrupt port is removed.*/
int
insert_intr_entry (int line, ipc_port_t dest)
{
  int err = 0;
  struct intr_entry *e, *new;
  int free = 0;

  new = (struct intr_entry *) kalloc (sizeof (*new));
  if (new == NULL)
    return D_NO_MEMORY;

  /* check whether the intr entry has been in the queue. */
  cli ();
  e = search_intr (line, dest);
  if (e)
    {
      printf ("the interrupt entry for line %d and port %p has been inserted\n",
	  line, dest);
      free = 1;
      err = D_ALREADY_OPEN;
      goto out;
    }
  new->line = line;
  new->dest = dest;
  new->interrupts = 0;
  queue_enter (&intr_queue, new, struct intr_entry *, chain);
out:
  sti ();
  if (free)
    kfree ((vm_offset_t) new, sizeof (*new));
  return err;
}

/* this function should be called when line is disabled. */
void mark_intr_removed (int line, ipc_port_t dest)
{
  struct intr_entry *e;

  e = search_intr (line, dest);
  if (e)
    e->dest = NULL;
}

void
intr_thread ()
{
  struct intr_entry *e;
  int line;
  ipc_port_t dest;
  queue_init (&intr_queue);
  
  for (;;)
    {
      assert_wait ((event_t) &intr_thread, FALSE);
      cli ();
      while (tot_num_intr)
	{
	  int del = 0;

	  queue_iterate (&intr_queue, e, struct intr_entry *, chain)
	    {
	      /* if an entry doesn't have dest port,
	       * we should remove it. */
	      if (e->dest == NULL)
		{
		  clear_wait (current_thread (), 0, 0);
		  del = 1;
		  break;
		}

	      if (e->interrupts)
		{
		  clear_wait (current_thread (), 0, 0);
		  line = e->line;
		  dest = e->dest;
		  e->interrupts--;
		  tot_num_intr--;

		  sti ();
		  deliver_intr (line, dest);
		  cli ();
		}
	    }

	  /* remove the entry without dest port from the queue and free it. */
	  if (del)
	    {
	      assert (!queue_empty (&intr_queue));
	      queue_remove (&intr_queue, e, struct intr_entry *, chain);
	      sti ();
	      kfree ((vm_offset_t) e, sizeof (*e));
	      cli ();
	    }
	}
      sti ();
      thread_block (NULL);
    }
}

static boolean_t
deliver_intr (int line, ipc_port_t dest_port)
{
  ipc_kmsg_t kmsg;
  mach_intr_notification_t *n;
  mach_port_t dest = (mach_port_t) dest_port;

  if (dest == MACH_PORT_NULL)
    return FALSE;

  kmsg = ikm_alloc(sizeof *n);
  if (kmsg == IKM_NULL) 
    return FALSE;

  ikm_init(kmsg, sizeof *n);
  n = (mach_intr_notification_t *) &kmsg->ikm_header;

  mach_msg_header_t *m = &n->intr_header;
  mach_msg_type_t *t = &n->intr_type;

  m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND, 0);
  m->msgh_size = sizeof *n;
  m->msgh_seqno = INTR_NOTIFY_MSGH_SEQNO;
  m->msgh_local_port = MACH_PORT_NULL;
  m->msgh_remote_port = MACH_PORT_NULL;
  m->msgh_id = MACH_INTR_NOTIFY;

  t->msgt_name = MACH_MSG_TYPE_INTEGER_32;
  t->msgt_size = 32;
  t->msgt_number = 1;
  t->msgt_inline = TRUE;
  t->msgt_longform = FALSE;
  t->msgt_deallocate = FALSE;
  t->msgt_unused = 0;

  n->intr_header.msgh_remote_port = dest;
  n->line = line;

  ipc_port_copy_send (dest_port);
  ipc_mqueue_send_always(kmsg);

  return TRUE;
}
#endif	/* MACH_XEN */
