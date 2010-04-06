/*
 * Mach Operating System
 * Copyright (c) 1992-1989 Carnegie Mellon University.
 * Copyright (c) 1995-1993 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * Bootstrap the various built-in servers.
 */

#include <alloca.h>
#include <string.h>

#include <mach/port.h>
#include <mach/message.h>
#include <machine/locore.h>
#include <machine/vm_param.h>
#include <ipc/ipc_port.h>
#include <ipc/mach_port.h>
#include <kern/debug.h>
#include <kern/host.h>
#include <kern/printf.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_user.h>
#include <device/device_port.h>

#if	MACH_KDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#endif

#if OSKIT_MACH
#include <stddef.h>
#include <oskit/machine/base_multiboot.h>
#include <oskit/exec/exec.h>
#include <oskit/c/stdio.h>
#define safe_gets(s, n) fgets((s),(n),stdin)
#else
#include <mach/machine/multiboot.h>
#include <mach/exec/exec.h>
#ifdef	MACH_XEN
#include <mach/xen.h>
extern struct start_info boot_info;	/* XXX put this in a header! */
#else	/* MACH_XEN */
extern struct multiboot_info boot_info;	/* XXX put this in a header! */
#endif	/* MACH_XEN */
#endif

#include "boot_script.h"


static mach_port_t	boot_device_port;	/* local name */
static mach_port_t	boot_host_port;		/* local name */

extern char *kernel_cmdline;

static void user_bootstrap();	/* forward */
static void user_bootstrap_compat();	/* forward */
static void bootstrap_exec_compat(void *exec_data); /* forward */
static void get_compat_strings(char *flags_str, char *root_str); /* forward */

static mach_port_t
task_insert_send_right(
	task_t task,
	ipc_port_t port)
{
	mach_port_t name;

	for (name = 1;; name++) {
		kern_return_t kr;

		kr = mach_port_insert_right(task->itk_space, name,
			    port, MACH_MSG_TYPE_PORT_SEND);
		if (kr == KERN_SUCCESS)
			break;
		assert(kr == KERN_NAME_EXISTS);
	}

	return name;
}

void bootstrap_create()
{
  int compat;
#ifdef	MACH_XEN
  struct multiboot_module *bmods = ((struct multiboot_module *)
                                   boot_info.mod_start);
  int n = 0;
  if (bmods)
    for (n = 0; bmods[n].mod_start; n++) {
      bmods[n].mod_start = kvtophys(bmods[n].mod_start + (vm_offset_t) bmods);
      bmods[n].mod_end = kvtophys(bmods[n].mod_end + (vm_offset_t) bmods);
      bmods[n].string = kvtophys(bmods[n].string + (vm_offset_t) bmods);
    }
  boot_info.mods_count = n;
  boot_info.flags |= MULTIBOOT_MODS;
#else	/* MACH_XEN */
  struct multiboot_module *bmods = ((struct multiboot_module *)
				    phystokv(boot_info.mods_addr));

#endif	/* MACH_XEN */
  if (!(boot_info.flags & MULTIBOOT_MODS)
      || (boot_info.mods_count == 0))
    panic ("No bootstrap code loaded with the kernel!");

  compat = boot_info.mods_count == 1;
  if (compat)
    {
      char *p = strchr((char*)phystokv(bmods[0].string), ' ');
      if (p != 0)
	do
	  ++p;
	while (*p == ' ' || *p == '\n');
      compat = p == 0 || *p == '\0';
    }

  if (compat)
    {
      printf("Loading single multiboot module in compat mode: %s\n",
	     (char*)phystokv(bmods[0].string));
      bootstrap_exec_compat(&bmods[0]);
    }
  else
    {
      int i, losers, maxlen;

      /* Initialize boot script variables.  We leak these send rights.  */
      losers = boot_script_set_variable
	("host-port", VAL_PORT,
	 (int)ipc_port_make_send(realhost.host_priv_self));
      if (losers)
	panic ("cannot set boot-script variable host-port: %s",
	       boot_script_error_string (losers));
      losers = boot_script_set_variable
	("device-port", VAL_PORT,
	 (int) ipc_port_make_send(master_device_port));
      if (losers)
	panic ("cannot set boot-script variable device-port: %s",
	       boot_script_error_string (losers));

      losers = boot_script_set_variable ("kernel-command-line", VAL_STR,
					 (int) kernel_cmdline);
      if (losers)
	panic ("cannot set boot-script variable %s: %s",
	       "kernel-command-line", boot_script_error_string (losers));

      {
	/* Set the same boot script variables that the old Hurd's
	   serverboot did, so an old Hurd and boot script previously
	   used with serverboot can be used directly with this kernel.  */

	char *flag_string = alloca(1024);
	char *root_string = alloca(1024);

	/*
	 * Get the (compatibility) boot flags and root name strings.
	 */
	get_compat_strings(flag_string, root_string);

	losers = boot_script_set_variable ("boot-args", VAL_STR,
					   (int) flag_string);
	if (losers)
	  panic ("cannot set boot-script variable %s: %s",
		 "boot-args", boot_script_error_string (losers));
	losers = boot_script_set_variable ("root-device", VAL_STR,
					   (int) root_string);
	if (losers)
	  panic ("cannot set boot-script variable %s: %s",
		 "root-device", boot_script_error_string (losers));
      }

#if OSKIT_MACH
      {
	/* The oskit's "environ" array contains all the words from
	   the multiboot command line that looked like VAR=VAL.
	   We set each of these as boot-script variables, which
	   can be used for things like ${root}.  */

	extern char **environ;
	char **ep;
	for (ep = environ; *ep != 0; ++ep)
	  {
	    size_t len = strlen (*ep) + 1;
	    char *var = memcpy (alloca (len), *ep, len);
	    char *val = strchr (var, '=');
	    *val++ = '\0';
	    losers = boot_script_set_variable (var, VAL_STR, (int) val);
	    if (losers)
	      panic ("cannot set boot-script variable %s: %s",
		     var, boot_script_error_string (losers));
	  }
      }
#else  /* GNUmach, not oskit-mach */
      {
	/* Turn each `FOO=BAR' word in the command line into a boot script
	   variable ${FOO} with value BAR.  This matches what we get from
	   oskit's environ in the oskit-mach case (above).  */

	int len = strlen (kernel_cmdline) + 1;
	char *s = memcpy (alloca (len), kernel_cmdline, len);
	char *word;
	while ((word = strsep (&s, " \t")) != 0)
	  {
	    char *eq = strchr (word, '=');
	    if (eq == 0)
	      continue;
	    *eq++ = '\0';
	    losers = boot_script_set_variable (word, VAL_STR, (int) eq);
	    if (losers)
	      panic ("cannot set boot-script variable %s: %s",
		     word, boot_script_error_string (losers));
	  }
      }
#endif

      maxlen = 0;
      for (i = 0; i < boot_info.mods_count; ++i)
	{
	  int err;
	  char *line = (char*)phystokv(bmods[i].string);
	  int len = strlen (line) + 1;
	  if (len > maxlen)
	    maxlen = len;
	  printf ("\rmodule %d: %*s", i, -maxlen, line);
	  err = boot_script_parse_line (&bmods[i], line);
	  if (err)
	    {
	      printf ("\n\tERROR: %s", boot_script_error_string (err));
	      ++losers;
	    }
	}
      printf ("\r%d multiboot modules %*s", i, -maxlen, "");
      if (losers)
	panic ("%d of %d boot script commands could not be parsed",
	       losers, boot_info.mods_count);
      losers = boot_script_exec ();
      if (losers)
	panic ("ERROR in executing boot script: %s",
	       boot_script_error_string (losers));
    }
  /* XXX at this point, we could free all the memory used
     by the boot modules and the boot loader's descriptors and such.  */
}

static void
bootstrap_exec_compat(void *e)
{
	task_t		bootstrap_task;
	thread_t	bootstrap_thread;

	/*
	 * Create the bootstrap task.
	 */

	(void) task_create(TASK_NULL, FALSE, &bootstrap_task);
	(void) thread_create(bootstrap_task, &bootstrap_thread);

	/*
	 * Insert send rights to the master host and device ports.
	 */

	boot_host_port =
		task_insert_send_right(bootstrap_task,
			ipc_port_make_send(realhost.host_priv_self));

	boot_device_port =
		task_insert_send_right(bootstrap_task,
			ipc_port_make_send(master_device_port));

	/*
	 * Start the bootstrap thread.
	 */
	bootstrap_thread->saved.other = e;
	thread_start(bootstrap_thread, user_bootstrap_compat);
	(void) thread_resume(bootstrap_thread);
}

/*
 * The following code runs as the kernel mode portion of the
 * first user thread.
 */

/*
 * Convert an unsigned integer to its decimal representation.
 */
static void
itoa(
	char		*str,
	vm_size_t	num)
{
	char	buf[sizeof(vm_size_t)*2+3];
	register char *np;

	np = buf + sizeof(buf);
	*--np = 0;

	do {
	    *--np = '0' + num % 10;
	    num /= 10;
	} while (num != 0);

	strcpy(str, np);
}

/*
 * Collect the boot flags into a single argument string,
 * for compatibility with existing bootstrap and startup code.
 * Format as a standard flag argument: '-qsdn...'
 */
static void get_compat_strings(char *flags_str, char *root_str)
{
	register char *ip, *cp;

	strcpy (root_str, "UNKNOWN");

	cp = flags_str;
	*cp++ = '-';

	for (ip = kernel_cmdline; *ip; )
	{
		if (*ip == ' ')
		{
			ip++;
		}
		else if (*ip == '-')
		{
			ip++;
			while (*ip > ' ')
				*cp++ = *ip++;
		}
		else if (strncmp(ip, "root=", 5) == 0)
		{
			char *rp = root_str;

			ip += 5;
			if (strncmp(ip, "/dev/", 5) == 0)
				ip += 5;
			while (*ip > ' ')
				*rp++ = *ip++;
			*rp = '\0';
		}
		else
		{
			while (*ip > ' ')
				ip++;
		}
	}

	if (cp == &flags_str[1])	/* no flags */
	    *cp++ = 'x';
	*cp = '\0';
}

#if 0
/*
 * Copy boot_data (executable) to the user portion of this task.
 */
static boolean_t	load_protect_text = TRUE;
#if MACH_KDB
		/* if set, fault in the text segment */
static boolean_t	load_fault_in_text = TRUE;
#endif

static vm_offset_t
boot_map(
	void *		data,	/* private data */
	vm_offset_t	offset)	/* offset to map */
{
	vm_offset_t	start_offset = (vm_offset_t) data;

	return pmap_extract(kernel_pmap, start_offset + offset);
}


#if BOOTSTRAP_SYMBOLS
static boolean_t load_bootstrap_symbols = TRUE;
#else
static boolean_t load_bootstrap_symbols = FALSE;
#endif
#endif



static int
boot_read(void *handle, vm_offset_t file_ofs, void *buf, vm_size_t size,
	  vm_size_t *out_actual)
{
  struct multiboot_module *mod = handle;

  if (mod->mod_start + file_ofs + size > mod->mod_end)
    return -1;

  memcpy(buf, (const char*) phystokv (mod->mod_start) + file_ofs, size);
  *out_actual = size;
  return 0;
}

static int
read_exec(void *handle, vm_offset_t file_ofs, vm_size_t file_size,
		     vm_offset_t mem_addr, vm_size_t mem_size,
		     exec_sectype_t sec_type)
{
  struct multiboot_module *mod = handle;

	vm_map_t user_map = current_task()->map;
	vm_offset_t start_page, end_page;
	vm_prot_t mem_prot = sec_type & EXEC_SECTYPE_PROT_MASK;
	int err;

	if (mod->mod_start + file_ofs + file_size > mod->mod_end)
	  return -1;

	if (!(sec_type & EXEC_SECTYPE_ALLOC))
		return 0;

	assert(mem_size > 0);
	assert(mem_size >= file_size);

	start_page = trunc_page(mem_addr);
	end_page = round_page(mem_addr + mem_size);

#if 0
	printf("reading bootstrap section %08x-%08x-%08x prot %d pages %08x-%08x\n",
		mem_addr, mem_addr+file_size, mem_addr+mem_size, mem_prot, start_page, end_page);
#endif

	err = vm_allocate(user_map, &start_page, end_page - start_page, FALSE);
	assert(err == 0);
	assert(start_page == trunc_page(mem_addr));

	if (file_size > 0)
	{
		err = copyout((char *)phystokv (mod->mod_start) + file_ofs,
			      (void *)mem_addr, file_size);
		assert(err == 0);
	}

	if (mem_prot != VM_PROT_ALL)
	{
		err = vm_protect(user_map, start_page, end_page - start_page, FALSE, mem_prot);
		assert(err == 0);
	}

	return 0;
}

static void copy_bootstrap(void *e, exec_info_t *boot_exec_info)
{
	//register vm_map_t	user_map = current_task()->map;
	int err;

	if ((err = exec_load(boot_read, read_exec, e, boot_exec_info)))
		panic("Cannot load user-bootstrap image: error code %d", err);

#if	MACH_KDB
	/*
	 * Enter the bootstrap symbol table.
	 */

#if 0 /*XXX*/
	if (load_bootstrap_symbols)
	(void) X_db_sym_init(
		(char*) boot_start+lp->sym_offset,
		(char*) boot_start+lp->sym_offset+lp->sym_size,
		"bootstrap",
		(char *) user_map);
#endif

#if 0 /*XXX*/
	if (load_fault_in_text)
	  {
	    vm_offset_t lenp = round_page(lp->text_start+lp->text_size) -
	      		     trunc_page(lp->text_start);
	    vm_offset_t i = 0;

	    while (i < lenp)
	      {
		vm_fault(user_map, text_page_start +i,
		        load_protect_text ?
			 VM_PROT_READ|VM_PROT_EXECUTE :
			 VM_PROT_READ|VM_PROT_EXECUTE | VM_PROT_WRITE,
			 0,0,0);
		i = round_page (i+1);
	      }
	  }
#endif
#endif	/* MACH_KDB */
}

/*
 * Allocate the stack, and build the argument list.
 */
extern vm_offset_t	user_stack_low();
extern vm_offset_t	set_user_regs();

static void
build_args_and_stack(struct exec_info *boot_exec_info,
		     char **argv, char **envp)
{
	vm_offset_t	stack_base;
	vm_size_t	stack_size;
	register
	char *		arg_ptr;
	int		arg_count, envc;
	int		arg_len;
	char *		arg_pos;
	int		arg_item_len;
	char *		string_pos;
	char *		zero = (char *)0;
	int i;

#define	STACK_SIZE	(64*1024)

	/*
	 * Calculate the size of the argument list.
	 */
	arg_len = 0;
	arg_count = 0;
	while (argv[arg_count] != 0) {
	    arg_ptr = argv[arg_count++];
	    arg_len += strlen(arg_ptr) + 1;
	}
	envc = 0;
	if (envp != 0)
	  while (envp[envc] != 0)
	    arg_len += strlen (envp[envc++]) + 1;

	/*
	 * Add space for:
	 *	arg count
	 *	pointers to arguments
	 *	trailing 0 pointer
	 *	pointers to environment variables
	 *	trailing 0 pointer
	 *	and align to integer boundary
	 */
	arg_len += (sizeof(integer_t)
		    + (arg_count + 1 + envc + 1) * sizeof(char *));
	arg_len = (arg_len + sizeof(integer_t) - 1) & ~(sizeof(integer_t)-1);

	/*
	 * Allocate the stack.
	 */
	stack_size = round_page(STACK_SIZE);
	stack_base = user_stack_low(stack_size);
	(void) vm_allocate(current_task()->map,
			&stack_base,
			stack_size,
			FALSE);

	arg_pos = (char *)
		set_user_regs(stack_base, stack_size, boot_exec_info, arg_len);

	/*
	 * Start the strings after the arg-count and pointers
	 */
	string_pos = (arg_pos
		      + sizeof(integer_t)
		      + (arg_count + 1 + envc + 1) * sizeof(char *));

	/*
	 * first the argument count
	 */
	(void) copyout((char *)&arg_count,
			arg_pos,
			sizeof(integer_t));
	arg_pos += sizeof(integer_t);

	/*
	 * Then the strings and string pointers for each argument
	 */
	for (i = 0; i < arg_count; ++i) {
	    arg_ptr = argv[i];
	    arg_item_len = strlen(arg_ptr) + 1; /* include trailing 0 */

	    /* set string pointer */
	    (void) copyout((char *)&string_pos,
			arg_pos,
			sizeof (char *));
	    arg_pos += sizeof(char *);

	    /* copy string */
	    (void) copyout(arg_ptr, string_pos, arg_item_len);
	    string_pos += arg_item_len;
	}

	/*
	 * Null terminator for argv.
	 */
	(void) copyout((char *)&zero, arg_pos, sizeof(char *));
	arg_pos += sizeof(char *);

	/*
	 * Then the strings and string pointers for each environment variable
	 */
	for (i = 0; i < envc; ++i) {
	    arg_ptr = envp[i];
	    arg_item_len = strlen(arg_ptr) + 1; /* include trailing 0 */

	    /* set string pointer */
	    (void) copyout((char *)&string_pos,
			arg_pos,
			sizeof (char *));
	    arg_pos += sizeof(char *);

	    /* copy string */
	    (void) copyout(arg_ptr, string_pos, arg_item_len);
	    string_pos += arg_item_len;
	}

	/*
	 * Null terminator for envp.
	 */
	(void) copyout((char *)&zero, arg_pos, sizeof(char *));
}


static void
user_bootstrap_compat()
{
	exec_info_t boot_exec_info;

	char	host_string[12];
	char	device_string[12];
	char	flag_string[1024];
	char	root_string[1024];

	/*
	 * Copy the bootstrap code from boot_exec into the user task.
	 */
	copy_bootstrap(current_thread()->saved.other, &boot_exec_info);

	/*
	 * Convert the host and device ports to strings,
	 * to put in the argument list.
	 */
	itoa(host_string, boot_host_port);
	itoa(device_string, boot_device_port);

	/*
	 * Get the (compatibility) boot flags and root name strings.
	 */
	get_compat_strings(flag_string, root_string);

	/*
	 * Build the argument list and insert in the user task.
	 * Argument list is
	 * "bootstrap -<boothowto> <host_port> <device_port> <root_name>"

$0 ${boot-args} ${host-port} ${device-port} ${root-device} $(task-create) $(task-resume)

	 */
	{
	  char *argv[] = { "bootstrap",
			   flag_string,
			   host_string,
			   device_string,
			   root_string,
			   0 };
	  char *envp[] = { 0, 0 };
	  if (kernel_cmdline[0] != '\0')
	    {
	      static const char cmdline_var[] = "MULTIBOOT_CMDLINE=";
	      envp[0] = alloca (sizeof cmdline_var + strlen (kernel_cmdline));
	      memcpy (envp[0], cmdline_var, sizeof cmdline_var - 1);
	      strcpy (envp[0] + sizeof cmdline_var - 1, kernel_cmdline);
	    }
	  build_args_and_stack(&boot_exec_info, argv, envp);
	}

	/*
	 * Exit to user thread.
	 */
	thread_bootstrap_return();
	/*NOTREACHED*/
}


struct user_bootstrap_info
{
  struct multiboot_module *mod;
  char **argv;
  int done;
  decl_simple_lock_data(,lock)
};

int
boot_script_exec_cmd (void *hook, task_t task, char *path, int argc,
		      char **argv, char *strings, int stringlen)
{
  struct multiboot_module *mod = hook;

  int err;

  if (task != MACH_PORT_NULL)
    {
      thread_t thread;
      struct user_bootstrap_info info = { mod, argv, 0, };
      simple_lock_init (&info.lock);
      simple_lock (&info.lock);

      err = thread_create ((task_t)task, &thread);
      assert(err == 0);
      thread->saved.other = &info;
      thread_start (thread, user_bootstrap);
      thread_resume (thread);

      /* We need to synchronize with the new thread and block this
	 main thread until it has finished referring to our local state.  */
      while (! info.done)
	{
	  thread_sleep ((event_t) &info, simple_lock_addr(info.lock), FALSE);
	  simple_lock (&info.lock);
	}
      printf ("\n");
    }

  return 0;
}

static void user_bootstrap()
{
  struct user_bootstrap_info *info = current_thread()->saved.other;
  exec_info_t boot_exec_info;
  int err;
  char **av;

  /* Load this task up from the executable file in the module.  */
  err = exec_load(boot_read, read_exec, info->mod, &boot_exec_info);
  if (err)
    panic ("Cannot load user executable module (error code %d): %s",
	   err, info->argv[0]);

  printf ("task loaded:");

  /* Set up the stack with arguments.  */
  build_args_and_stack(&boot_exec_info, info->argv, 0);

  for (av = info->argv; *av != 0; ++av)
    printf (" %s", *av);

  task_suspend (current_task());

  /* Tell the bootstrap thread running boot_script_exec_cmd
     that we are done looking at INFO.  */
  simple_lock (&info->lock);
  assert (!info->done);
  info->done = 1;
  thread_wakeup ((event_t) info);

  /*
   * Exit to user thread.
   */
  thread_bootstrap_return();
  /*NOTREACHED*/
}



void *
boot_script_malloc (unsigned int size)
{
  return (void *) kalloc (size);
}

void
boot_script_free (void *ptr, unsigned int size)
{
  kfree ((vm_offset_t)ptr, size);
}

int
boot_script_task_create (struct cmd *cmd)
{
  kern_return_t rc = task_create(TASK_NULL, FALSE, &cmd->task);
  if (rc)
    {
      printf("boot_script_task_create failed with %x\n", rc);
      return BOOT_SCRIPT_MACH_ERROR;
    }
  return 0;
}

int
boot_script_task_resume (struct cmd *cmd)
{
  kern_return_t rc = task_resume (cmd->task);
  if (rc)
    {
      printf("boot_script_task_resume failed with %x\n", rc);
      return BOOT_SCRIPT_MACH_ERROR;
    }
  printf ("\nstart %s: ", cmd->path);
  return 0;
}

int
boot_script_prompt_task_resume (struct cmd *cmd)
{
  char xx[5];

  printf ("Hit return to resume %s...", cmd->path);
  safe_gets (xx, sizeof xx);

  return boot_script_task_resume (cmd);
}

void
boot_script_free_task (task_t task, int aborting)
{
  if (aborting)
    task_terminate (task);
}

int
boot_script_insert_right (struct cmd *cmd, mach_port_t port, mach_port_t *name)
{
  *name = task_insert_send_right (cmd->task, (ipc_port_t)port);
  return 0;
}

int
boot_script_insert_task_port (struct cmd *cmd, task_t task, mach_port_t *name)
{
  *name = task_insert_send_right (cmd->task, task->itk_sself);
  return 0;
}
