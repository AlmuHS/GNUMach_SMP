# Makefile for Mach 4 kernel directory
# Copyright 1996 Free Software Foundation, Inc.
# This file is part of GNU Mach.  Redistribution terms are not yet decided. 



# Set at entry:
# $(srcdir) $(systype) $(installed-clib)

sysdep = $(srcdir)/$(systype)

ifeq ($(MIG),)
MIG := mig
endif

ifeq ($(AWK),)
AWK := awk
endif

all:

# All the source in each directory.  Note that `bogus' contains no source,
# only header files.

# Generic code for various hardware drivers
chips-files = atm.c audio.c bt431.c bt455.c bt459.c build_font.c busses.c \
	cfb_hdw.c cfb_misc.c dc503.c dtop_handlers.c dtop_hdw.c dz_hdw.c \
	fb_hdw.c fb_misc.c fdc_82077_hdw.c frc.c ims332.c isdn_79c30_hdw.c \
	kernel_font.c lance.c lance_mapped.c lk201.c mc_clock.c mouse.c \
	nc.c nw_mk.c pm_hdw.c pm_misc.c scc_8530_hdw.c screen.c \
	screen_switch.c serial_console.c sfb_hdw.c sfb_misc.c spans.c \
	tca100.c tca100_if.c xcfb_hdb.c xcfb_misc.c

# Generic code for various SCSI unit types
scsi-files = disk_label.c mapped_scsi.c pc_scsi_label.c rz.c rz_audio.c \
	rz_cpu.c rz_disk.c rz_disk_bbr.c rz_host.c rz_tape.c scsi.c \
	scsi_alldevs.c scsi_comm.c scsi_cpu.c scsi_disk.c scsi_jukebox.c \
	scsi_optical.c scsi_printer.c scsi_rom.c scsi_scanner.c \
	scsi_tape.c scsi_worm.c

# Icky kernel debugger
ddb-files = $(addprefix db_,$(ddb-names))
ddb-names = access.c aout.c break.c command.c cond.c examine.c expr.c \
	ext_symtab.c input.c lex.c macro.c mp.c output.c print.c run.c \
	sym.c task_thread.c trap.c variables.c watch.c write_cmd.c

# Device support interfaces
device-files = blkio.c chario.c cirbuf.c cons.c dev_lookup.c dev_name.c \
	dev_pager.c device_init.c dk_label.c ds_routines.c net_io.c subrs.c

# IPC implementation
ipc-files = $(addprefix ipc_,$(ipc-names)) \
		mach_msg.c mach_port.c mach_rpc.c mach_debug.c fipc.c
ipc-names = entry.c hash.c init.c kmsg.c marequest.c mqueue.c \
	notify.c object.c port.c pset.c right.c space.c splay.c \
	table.c target.c thread.c 

# "kernel" implementation (tasks, threads, trivia, etc.)
kern-files = act.c ast.c bootstrap.c counters.c debug.c eventcount.c \
	exception.c host.c ipc_host.c ipc_kobject.c ipc_mig.c ipc_sched.c \
	ipc_tt.c kalloc.c lock.c lock_mon.c mach_clock.c mach_factor.c \
	machine.c pc_sample.c printf.c priority.c processor.c profile.c \
	queue.c sched_prim.c startup.c strings.c syscall_emulation.c \
	syscall_subr.c syscall_sw.c task.c thread.c thread_swap.c \
	time_stamp.c timer.c xpr.c zalloc.c elf-load.c

# Still more trivia
util-files = about_to_die.c cpu.c cpu_init.c die.c putchar.c puts.c

# Virtual memory implementation
vm-files = $(addprefix vm_,$(vm-names)) memory_object.c 
vm-names = debug.c external.c fault.c init.c kern.c map.c \
	object.c pageout.c resident.c user.c



# Object files that go into the kernel image.  (This will be augmented by the
# machine dependent Makefile fragment.)

# Basic kernel source for Mach
objfiles := $(subst .c,.o,$(ipc-files) $(kern-files) $(util-files) $(vm-files))
vpath %.c $(srcdir)/ipc $(srcdir)/kern $(srcdir)/util $(srcdir)/vm

# These device support files are always needed; the others are needed only
# if particular drivers want the routines.
# XXX functions in device/subrs.c should each be moved elsewhere
objfiles += cons.o dev_lookup.o dev_name.o dev_pager.o device_init.o \
	ds_routines.o subrs.o net_io.o blkio.o chario.o
vpath %.c $(srcdir)/device

# DDB support -- eventually to die.  Please.
objfiles += $(subst .c,.o,$(ddb-files))
vpath %.c $(srcdir)/ddb

# Version number
objfiles += version.o
vpath version.c $(srcdir)


# We steal routines from the C library and put them here.
objfiles += clib-routines.o

clib-routines = memcpy memset bcopy bzero htonl ntohl ntohs

clib-routines.o: $(installed-clib)
	$(LD) -o clib-routines.o -r $(addprefix -u ,$(clib-routines)) $(installed-clib)


# Automatically generated source

# User stubs
objfiles += memory_object_user_user.o memory_object_default_user.o \
	device_reply_user.o memory_object_reply_user.o

# Server stubs
objfiles += device_server.o device_pager_server.o mach_port_server.o \
	mach_server.o mach4_server.o mach_debug_server.o mach_host_server.o

# Where to find the relevant Mig source files
vpath %.cli $(srcdir)/vm $(srcdir)/device
vpath %.srv $(srcdir)/device $(srcdir)/ipc $(srcdir)/kern


# XXXX temporary
vm_fault.o: memory_object_user.h
vm_object.o: memory_object_default.h
ds_routines.o: device_reply.h




#
# Compilation flags
#

DEFINES += -DMACH -DCMU -DMACH_KERNEL -DKERNEL
INCLUDES += -I. -I$(srcdir) -I$(srcdir)/util -I$(srcdir)/bogus  \
	-I$(srcdir)/kern -I$(srcdir)/device \
	-I$(srcdir)/include -I$(srcdir)/include/mach/sa 

include $(sysdep)/Makefrag

CPPFLAGS += -nostdinc $(DEFINES) $(INCLUDES) 

MIGFLAGS += $(CPPFLAGS)

#
# Image
#
# (The newline in this command makes it much easier to read in make output.)
all: kernel
kernel: $(objfiles)
	$(LD) -o $@ $(LDFLAGS) \
		$(objfiles)
#
# How to do some things
# 

# Building foo.h from foo.sym:
%.symc: %.sym
	$(AWK) -f $(srcdir)/gensym.awk $< >$*.symc
%.symc.o: %.symc
	$(CC) -S $(CPPFLAGS) $(CFLAGS) $(CPPFLAGS-$@) -x c -o $@ $<
%.h: %.symc.o
	sed <$< -e 's/^[^*].*$$//' | \
		sed -e 's/^[*]/#define/' -e 's/mAgIc[^-0-9]*//' >$@

# Building from foo.cli
%.h %_user.c: %.cli
	$(MIG) $(MIGFLAGS) -header $*.h -user $*_user.c -server /dev/null $<

# Building from foo.srv
%_interface.h %_server.c: %.srv
	$(MIG) $(MIGFLAGS) -header $*_interface.h -server $*_server.c -user /dev/null $<


