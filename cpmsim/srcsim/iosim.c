/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 1987-2006 by Udo Munk
 *
 * This modul contains a complex I/O-simulation for running
 * CP/M 2, CP/M 3, MP/M...
 * Please note this this doesn't emulate any hardware which
 * ever existed, we've got all virtual circuits in here!
 *
 * History:
 * 28-SEP-87 Development on TARGON/35 with AT&T Unix System V.3
 * 19-MAY-89 Additions for CP/M 3.0 und MP/M
 * 23-DEC-90 Ported to COHERENT 3.0
 * 10-JUN-92 Some optimization done
 * 25-JUN-92 Flush output of stdout only at every OUT to port 0
 * 25-JUN-92 Comments in english and ported to COHERENT 4.0
 * 05-OCT-06 modified to compile on modern POSIX OS's
 */

/*
 *	This module contains the I/O handlers for a simulation
 *	of the hardware required for a CP/M system.
 *
 *	Used I/O ports:
 *
 *	 0 - console status
 *	 1 - console data
 *
 *	 2 - printer status
 *	 3 - printer data
 *
 *	 4 - auxilary status
 *	 5 - auxilary data
 *
 *	10 - FDC drive
 *	11 - FDC track
 *	12 - FDC sector
 *	13 - FDC command
 *	14 - FDC status
 *
 *	15 - DMA destination address low
 *	16 - DMA destination address high
 *
 *	20 - MMU initialization
 *	21 - MMU bank select
 *
 *	25 - clock command
 *	26 - clock data
 *	27 - 20ms timer causing INT, only usable in IM 1
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include "sim.h"
#include "simglb.h"

/*
 *	Structure to describe a emulated floppy disk drive:
 *		pointer to filename
 *		pointer to file descriptor
 *		number of tracks
 *		number of sectors
 */
struct dskdef {
	char *fn;
	int *fd;
	unsigned int tracks;
	unsigned int sectors;
};

static BYTE drive;	/* current drive A..P (0..15) */
static BYTE track;	/* current track (0..255) */
static BYTE sector;	/* current sektor (0..255) */
static BYTE status;	/* status of last I/O operation on FDC */
static BYTE dmadl;	/* current DMA adresse destination low */
static BYTE dmadh;	/* current DMA adresse destination high */
static BYTE clkcmd;	/* clock command */
static BYTE timer;	/* 20ms timer */
static int drivea;	/* fd for file "drivea.cpm" */
static int driveb;	/* fd for file "driveb.cpm" */
static int drivec;	/* fd for file "drivec.cpm" */
static int drived;	/* fd for file "drived.cpm" */
static int drivee;	/* fd for file "drivee.cpm" */
static int drivef;	/* fd for file "drivef.cpm" */
static int driveg;	/* fd for file "driveg.cpm" */
static int driveh;	/* fd for file "driveh.cpm" */
static int drivei;	/* fd for file "drivei.cpm" */
static int drivej;	/* fd for file "drivej.cpm" */
static int drivek;	/* fd for file "drivek.cpm" */
static int drivel;	/* fd for file "drivel.cpm" */
static int drivem;	/* fd for file "drivem.cpm" */
static int driven;	/* fd for file "driven.cpm" */
static int driveo;	/* fd for file "driveo.cpm" */
static int drivep;	/* fd for file "drivep.cpm" */
static int printer;	/* fd for file "printer.cpm" */
static int auxin;	/* fd for pipe "auxin" */
static int auxout;	/* fd for pipe "auxout" */
static int aux_in_eof;	/* status of pipe "auxin" (<>0 means EOF) */
static int pid_rec;	/* PID of the receiving process for auxiliary */
static char last_char;	/* buffer for 1 character (console status) */

static struct dskdef disks[16] = {
	{ "disks/drivea.cpm", &drivea, 77, 26 },
	{ "disks/driveb.cpm", &driveb, 77, 26 },
	{ "disks/drivec.cpm", &drivec, 77, 26 },
	{ "disks/drived.cpm", &drived, 77, 26 },
	{ "disks/drivee.cpm", &drivee, -1, -1 },
	{ "disks/drivef.cpm", &drivef, -1, -1 },
	{ "disks/driveg.cpm", &driveg, -1, -1 },
	{ "disks/driveh.cpm", &driveh, -1, -1 },
	{ "disks/drivei.cpm", &drivei, 255, 128 },
	{ "disks/drivej.cpm", &drivej, -1, -1 },
	{ "disks/drivek.cpm", &drivek, -1, -1 },
	{ "disks/drivel.cpm", &drivel, -1, -1 },
	{ "disks/drivem.cpm", &drivem, -1, -1 },
	{ "disks/driven.cpm", &driven, -1, -1 },
	{ "disks/driveo.cpm", &driveo, -1, -1 },
	{ "disks/drivep.cpm", &drivep, -1, -1 }
};

/*
 *      MMU:
 *      ===
 *
 *      +--------+
 * 16KB | common |
 *      +--------+
 *      +--------+  +--------+  ..........  +--------+
 *      |        |  |        |              |        |
 * 48KB |        |  |        |  ..........  |        |
 *      | bank 0 |  | bank 1 |              | bank n |
 *      +--------+  +--------+  ..........  +--------+
 */
#define MAXSEG 16		/* max. number of memory banks */
#define SEGSIZ 49152		/* size of one bank = 48KBytes */
static char *mmu[MAXSEG];	/* MMU with pointers to the banks */
static int selbnk;		/* current bank */
static int maxbnk;		/* number of initialized banks */

/*
 *	Forward declaration of the I/O handlers for all used ports
 */
static BYTE io_trap(void);
static BYTE cond_in(void), cond_out(BYTE), cons_in(void), cons_out(BYTE);
static BYTE prtd_in(void), prtd_out(BYTE), prts_in(void), prts_out(BYTE);
static BYTE auxd_in(void), auxd_out(BYTE), auxs_in(void), auxs_out(BYTE);
static BYTE fdcd_in(void), fdcd_out(BYTE);
static BYTE fdct_in(void), fdct_out(BYTE);
static BYTE fdcs_in(void), fdcs_out(BYTE);
static BYTE fdco_in(void), fdco_out(BYTE);
static BYTE fdcx_in(void), fdcx_out(BYTE);
static BYTE dmal_in(void), dmal_out(BYTE);
static BYTE dmah_in(void), dmah_out(BYTE);
static BYTE mmui_in(void), mmui_out(BYTE), mmus_in(void), mmus_out(BYTE);
static BYTE clkc_in(void), clkc_out(BYTE), clkd_in(void), clkd_out(BYTE);
static BYTE time_in(void), time_out(BYTE);
static void int_timer(int);

static int to_bcd(int), get_date(struct tm *);

/*
 *	This array contains two function pointer for every
 *	active port, one for input and one for output.
 */
static BYTE (*port[256][2]) () = {
	{ cons_in, cons_out },		/* port 0 */
	{ cond_in, cond_out },		/* port 1 */
	{ prts_in, prts_out },		/* port 2 */
	{ prtd_in, prtd_out },		/* port 3 */
	{ auxs_in, auxs_out },		/* port 4 */
	{ auxd_in, auxd_out },		/* port 5 */
	{ io_trap, io_trap  },		/* port 6 */
	{ io_trap, io_trap  },		/* port 7 */
	{ io_trap, io_trap  },		/* port 8 */
	{ io_trap, io_trap  },		/* port 9 */
	{ fdcd_in, fdcd_out },		/* port 10 */
	{ fdct_in, fdct_out },		/* port 11 */
	{ fdcs_in, fdcs_out },		/* port 12 */
	{ fdco_in, fdco_out },		/* port 13 */
	{ fdcx_in, fdcx_out },		/* port 14 */
	{ dmal_in, dmal_out },		/* port 15 */
	{ dmah_in, dmah_out },		/* port 16 */
	{ io_trap, io_trap  },		/* port 17 */
	{ io_trap, io_trap  },		/* port 18 */
	{ io_trap, io_trap  },		/* port 19 */
	{ mmui_in, mmui_out },		/* port 20 */
	{ mmus_in, mmus_out },		/* port 21 */
	{ io_trap, io_trap  },		/* port 22 */
	{ io_trap, io_trap  },		/* port 23 */
	{ io_trap, io_trap  },		/* port 24 */
	{ clkc_in, clkc_out },		/* port 25 */
	{ clkd_in, clkd_out },		/* port 26 */
	{ time_in, time_out }		/* port 27 */
};

/*
 *	This function initializes the I/O handlers:
 *	1. Initialize all unused ports with the I/O trap handler.
 *	2. Initialize the MMU with NULL pointers.
 *	3. Open the files which emulates the disk drives. The file
 *	   for drive A must be opened, or CP/M can't be booted.
 *	   Errors for opening one of the other 15 drives results
 *	   in a NULL pointer for fd in the dskdef structure,
 *	   so that this drive can't be used.
 *	4. Create and open the file "printer.cpm" for emulation
 *	   of a printer.
 *	5. Fork the process for receiving from the serial port.
 *	6. Open the named pipes "auxin" and "auxout" for simulation
 *	   of a serial port.
 */
void init_io(void)
{
	register int i;

	for (i = 28; i <= 255; i++) {
		port[i][0] = io_trap;
		port[i][1] = io_trap;
	}
	for (i = 0; i < MAXSEG; i++)
		mmu[i] = NULL;
	if ((*disks[0].fd = open(disks[0].fn, O_RDWR)) == -1) {
		perror("file disks/drivea.cpm");
		exit(1);
	}
	for (i = 1; i <= 15; i++)
		if ((*disks[i].fd = open(disks[i].fn, O_RDWR)) == -1)
			disks[i].fd = NULL;
	if ((printer = creat("printer.cpm", 0644)) == -1) {
		perror("file printer.cpm");
		exit(1);
	}
	pid_rec = fork();
	switch (pid_rec) {
	case -1:
		puts("can't fork");
		exit(1);
	case 0:
		execlp("./receive", "receive", "auxiliary.cpm", (char *) NULL);
		puts("can't exec receive process");
		exit(1);
	}
	if ((auxin = open("auxin", O_RDONLY | O_NDELAY)) == -1) {
		perror("pipe auxin");
		exit(1);
	}
	if ((auxout = open("auxout", O_WRONLY)) == -1) {
		perror("pipe auxout");
		exit(1);
	}
}

/*
 *	This function stops the I/O handlers:
 *
 *	1. The files emulating the disk drives are closed.
 *	2. The file "printer.com" emulating a printer is closed.
 *	3. The named pipes "auxin" and "auxout" are closed.
 *	4. The receiving process for the serial port is stopped.
 */
void exit_io(void)
{
	register int i;

	for (i = 0; i <= 15; i++)
		if (disks[i].fd != NULL)
			close(*disks[i].fd);
	close(printer);
	close(auxin);
	close(auxout);
	kill(pid_rec, SIGHUP);
}

/*
 *	This function is called for every IN opcode from the
 *	CPU emulation. It calls the right handler for the
 *	port, from which input is wanted.
 */
BYTE io_in(BYTE adr)
{
	return((*port[adr][0]) ());
}

/*
 *	This function is called for every OUT opcode from the
 *	CPU emulation. It calls the right handler for the port,
 *	to which output is wanted.
 */
BYTE io_out(BYTE adr, BYTE data)
{
	(*port[adr][1]) (data);
	return((BYTE) 0);
}

/*
 *	I/O trap handler
 */
static BYTE io_trap(void)
{
	if (i_flag) {
		cpu_error = IOTRAP;
		cpu_state = STOPPED;
	}
	return((BYTE) 0);
}

/*
 *	I/O handler for read console status:
 *	0xff : input available
 *	0x00 : no input available
 */
static BYTE cons_in(void)
{
	register int flags, readed;

	if (last_char)
		return((BYTE) 0xff);
	if (cntl_c)
		return((BYTE) 0xff);
	if (cntl_bs)
		return((BYTE) 0xff);
	else {
		flags = fcntl(0, F_GETFL, 0);
		fcntl(0, F_SETFL, flags | O_NDELAY);
		readed = read(0, &last_char, 1);
		fcntl(0, F_SETFL, flags);
		if (readed == 1)
			return((BYTE) 0xff);
	}
	return((BYTE) 0);
}

/*
 *	I/O handler for write console status:
 *	no reaction
 */
static BYTE cons_out(BYTE data)
{
	data = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read console data:
 *	read one character from the terminal without echo
 *	and character transformations
 */
static BYTE cond_in(void)
{
	char c;

	aborted:
	if (last_char) {
		c = last_char;
		last_char = '\0';
	} else if (cntl_c) {
		cntl_c--;
		c = 0x03;
	} else if (cntl_bs) {
		cntl_bs--;
		c = 0x1c;
	} else if (read(0, &c, 1) != 1) {
		goto aborted;
	}
	return((BYTE) c);
}

/*
 *	I/O handler for write console data:
 *	the output is written to the terminal
 */
static BYTE cond_out(BYTE data)
{
	while ((write(fileno(stdout), (char *) &data, 1)) != 1)
		;
	fflush(stdout);
	return((BYTE) 0);
}

/*
 *	I/O handler for read printer status:
 *	the printer is ready all the time
 */
static BYTE prts_in(void)
{
	return((BYTE) 0xff);
}

/*
 *	I/O handler for write printer status:
 *	no reaction
 */
static BYTE prts_out(BYTE data)
{
	data = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read printer data:
 *	always read a 0 from the printer
 */
static BYTE prtd_in(void)
{
	return((BYTE) 0);
}

/*
 *	I/O handler for write printer data:
 *	the output is written to file "printer.cpm"
 */
static BYTE prtd_out(BYTE data)
{
	if (data != '\r')
		while ((write(printer, (char *) &data, 1)) != 1)
			;
	return((BYTE) 0);
}

/*
 *	I/O handler for read aux status:
 *	return EOF status of the aux device
 */
static BYTE auxs_in(void)
{
	return((BYTE) aux_in_eof);
}

/*
 *	I/O handler for write aux status:
 *	change EOF status of the aux device
 */
static BYTE auxs_out(BYTE data)
{
	aux_in_eof = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read aux data:
 *	read next byte from pipe "auxin"
 */
static BYTE auxd_in(void)
{
	char c;

	if (read(auxin, &c, 1) == 1)
		return((BYTE) c);
	else {
		aux_in_eof = 0xff;
		return((BYTE) 0x1a);	/* CP/M EOF */
	}
}

/*
 *	I/O handler for write aux data:
 *	write output to pipe "auxout"
 */
static BYTE auxd_out(BYTE data)
{
	if (data != '\r')
		write(auxout, (char *) &data, 1);
	return((BYTE) 0);
}

/*
 *	I/O handler for read FDC drive:
 *	return the current drive
 */
static BYTE fdcd_in(void)
{
	return((BYTE) drive);
}

/*
 *	I/O handler for write FDC drive:
 *	set the current drive
 */
static BYTE fdcd_out(BYTE data)
{
	drive = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read FDC track:
 *	return the current track
 */
static BYTE fdct_in(void)
{
	return((BYTE) track);
}

/*
 *	I/O handler for write FDC track:
 *	set the current track
 */
static BYTE fdct_out(BYTE data)
{
	track = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read FDC sector
 *	return the current sector
 */
static BYTE fdcs_in(void)
{
	return((BYTE) sector);
}

/*
 *	I/O handler for write FDC sector:
 *	set the current sector
 */
static BYTE fdcs_out(BYTE data)
{
	sector = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read FDC command:
 *	always returns 0
 */
static BYTE fdco_in(void)
{
	return((BYTE) 0);
}

/*
 *	I/O handler for write FDC command:
 *	transfer one sector in the wanted direction,
 *	0 = read, 1 = write
 *
 *	The status byte of the FDC is set as follows:
 *	  0 - ok
 *	  1 - illegal drive
 *	  2 - illegal track
 *	  3 - illegal sector
 *	  4 - seek error
 *	  5 - read error
 *	  6 - write error
 *	  7 - illegal command to FDC
 */
static BYTE fdco_out(BYTE data)
{
	register long pos;
	if (disks[drive].fd == NULL) {
		status = 1;
		return((BYTE) 0);
	}
	if (track > disks[drive].tracks) {
		status = 2;
		return((BYTE) 0);
	}
	if (sector > disks[drive].sectors) {
		status = 3;
		return((BYTE) 0);
	}
	pos = (((long)track) * ((long)disks[drive].sectors) + sector - 1) << 7;
	if (lseek(*disks[drive].fd, pos, 0) == -1L) {
		status = 4;
		return((BYTE) 0);
	}
	switch (data) {
	case 0:			/* read */
		if (read(*disks[drive].fd, (char *) ram + (dmadh << 8) +
		    dmadl, 128) != 128)
			status = 5;
		else
			status = 0;
		break;
	case 1:			/* write */
		if (write(*disks[drive].fd, (char *) ram + (dmadh << 8) +
		    dmadl, 128) != 128)
			status = 6;
		else
			status = 0;
		break;
	default:		/* illegal command */
		status = 7;
		break;
	}
	return((BYTE) 0);
}

/*
 *	I/O handler for read FDC status:
 *	returns status of last FDC operation,
 *	0 = ok, else some error
 */
static BYTE fdcx_in(void)
{
	return((BYTE) status);
}

/*
 *	I/O handler for write FDC status:
 *	no reaction
 */
static BYTE fdcx_out(BYTE data)
{
	data = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read lower byte of DMA address:
 *	return lower byte of current DMA address
 */
static BYTE dmal_in(void)
{
	return((BYTE) dmadl);
}

/*
 *	I/O handler for write lower byte of DMA address:
 *	set lower byte of DMA address
 */
static BYTE dmal_out(BYTE data)
{
	dmadl = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read higher byte of DMA address:
 *	return higher byte of current DMA address
 */
static BYTE dmah_in(void)
{
	return((BYTE) dmadh);
}

/*
 *	I/O handler for write higher byte of DMA address:
 *	set higher byte of the DMA address
 */
static BYTE dmah_out(BYTE data)
{
	dmadh = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read MMU initialization:
 *	return number of initialized MMU banks
 */
static BYTE mmui_in(void)
{
	return((BYTE) maxbnk);
}

/*
 *	I/O handler for write MMU initialization:
 *	for the FIRST call the memory for the wanted number of banks
 *	is allocated and pointers to the memory is stored in the MMU array
 */
static BYTE mmui_out(BYTE data)
{
	register int i;

	if (mmu[0] != NULL)
		return((BYTE) 0);
	if (data > MAXSEG) {
		printf("Try to init %d banks, available %d banks\n",
		       data, MAXSEG);
		exit(1);
	}
	for (i = 0; i < data; i++) {
		if ((mmu[i] = malloc(SEGSIZ)) == NULL) {
			printf("can't allocate memory for bank %d\n", i+1);
			exit(1);
		}
	}
	maxbnk = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read MMU bank select:
 *	return current selected MMU bank
 */
static BYTE mmus_in(void)
{
	return((BYTE) selbnk);
}

/*
 *	I/O handler for write MMU bank select:
 *	if the current selected bank is not equal the wanted bank,
 *	the current bank is saved. Then the memory of the wanted
 *	bank is copied into the CPU address space and this bank is
 *	set to be the current one now.
 */
static BYTE mmus_out(BYTE data)
{
	if (data > maxbnk) {
		printf("Try to select unallocated bank %d\n", data);
		exit(1);
	}
	if (data == selbnk)
		return((BYTE) 0);
	memcpy(mmu[selbnk], (char *) ram, SEGSIZ);
	memcpy((char *) ram, mmu[data], SEGSIZ);
	selbnk = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read clock command:
 *	return last clock command
 */
static BYTE clkc_in(void)
{
	return(clkcmd);
}

/*
 *	I/O handler for write clock command:
 *	set the wanted clock command
 */
static BYTE clkc_out(BYTE data)
{
	clkcmd = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read clock data:
 *	dependent from the last clock command the following
 *	informations are given from the system clock:
 *		0 - seconds in BCD
 *		1 - minutes in BCD
 *		2 - hours in BCD
 *		3 - low byte number of days since 1.1.1978
 *		4 - high byte number of days since 1.1.1978
 *	for every other clock command a 0 is returned
 */
static BYTE clkd_in(void)
{
	register struct tm *t;
	register int val;
	time_t Time;

	time(&Time);
	t = localtime(&Time);
	switch(clkcmd) {
	case 0:			/* seconds in BCD */
		val = to_bcd(t->tm_sec);
		break;
	case 1:			/* minutes in BCD */
		val = to_bcd(t->tm_min);
		break;
	case 2:			/* hours in BCD */
		val = to_bcd(t->tm_hour);
		break;
	case 3:			/* low byte days */
		val = get_date(t) & 255;
		break;
	case 4:			/* high byte days */
		val = get_date(t) >> 8;
		break;
	default:
		val = 0;
		break;
	}
	return((BYTE) val);
}

/*
 *	I/O handler for write clock data:
 *	under UNIX the system clock only can be set by the
 *	super user, so we do nothing here
 */
static BYTE clkd_out(BYTE data)
{
	data = data;
	return((BYTE) 0);
}

/*
 *	Convert an integer to BCD
 */
static int to_bcd(int val)
{
	register int i = 0;

	while (val >= 10) {
		i += val / 10;
		i <<= 4;
		val %= 10;
	}
	i += val;
	return (i);
}

/*
 *	Calculate number of days since 1.1.1978
 *	The Y2K bug here is intentional, CP/M 3 has a Y2K bug fix
 */
static int get_date(struct tm *t)
{
	register int i;
	register int val = 0;

	for (i = 1978; i < 1900 + t->tm_year; i++) {
		val += 365;
		if (i % 4 == 0)
			val++;
	}
	val += t->tm_yday + 1;
	return(val);
}

/*
 *	I/O handler for write timer
 */
static BYTE time_out(BYTE data)
{
	static struct itimerval tim;
	static struct sigaction newact;

	if (data == 1) {
		timer = 1;
		newact.sa_handler = int_timer;
		sigaction(SIGALRM, &newact, NULL);
		tim.it_value.tv_sec = 0;
		tim.it_value.tv_usec = 20000;
		tim.it_interval.tv_sec = 0;
		tim.it_interval.tv_usec = 20000;
		setitimer(ITIMER_REAL, &tim, NULL);
	} else {
		timer = 0;
		newact.sa_handler = SIG_IGN;
		sigaction(SIGALRM, &newact, NULL);
		tim.it_value.tv_sec = 0;
		tim.it_value.tv_usec = 0;
		setitimer(ITIMER_REAL, &tim, NULL);
	}
	return((BYTE) 0);
}

/*
 *	I/O handler for read timer
 */
static BYTE time_in(void)
{
	return(timer);
}

/*
 *	timer interrupt causes maskerable CPU interrupt
 */
static void int_timer(int sig)
{
	int_type = INT_INT;
}
