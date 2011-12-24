/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 1987-2007 by Udo Munk
 *
 * This modul contains a complex I/O-simulation for running
 * CP/M 2, CP/M 3, MP/M...
 * Please note this doesn't emulate any hardware which
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
 * 18-NOV-06 added a second harddisk
 * 08-DEC-06 modified MMU so that segment size can be configured
 * 10-DEC-06 started adding serial port for a passive TCP/IP socket
 * 14-DEC-06 started adding serial port for a client TCP/IP socket
 * 25-DEC-06 CPU speed option and 100 ticks interrupt
 * 19-FEB-07 improved networking
 * 22-JUL-07 added second FDC sector port for implementing large harddisks
 */

/*
 *	This module contains the I/O handlers for a simulation
 *	of the hardware required for a CP/M / MP/M system.
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
 *	12 - FDC sector (low)
 *	13 - FDC command
 *	14 - FDC status
 *
 *	15 - DMA destination address low
 *	16 - DMA destination address high
 *
 *	17 - FDC sector high
 *
 *	20 - MMU initialization
 *	21 - MMU bank select
 *	22 - MMU select segment size (in pages a 256 bytes)
 *
 *	25 - clock command
 *	26 - clock data
 *	27 - 10ms timer causing IM 1 INT
 *	28 - 10ms delay circuit
 *
 *	40 - passive socket #1 status
 *	41 - passive socket #1 data
 *	42 - passive socket #2 status
 *	43 - passive socket #2 data
 *	44 - passive socket #3 status
 *	45 - passive socket #3 data
 *	46 - passive socket #4 status
 *	47 - passive socket #4 data
 *
 *	50 - client socket #1 status
 *	51 - client socket #1 data
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <netdb.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include "sim.h"
#include "simglb.h"

/*
 *	Structure to describe an emulated disk drive:
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

static BYTE drive;		/* current drive A..P (0..15) */
static BYTE track;		/* current track (0..255) */
static int sector;		/* current sektor (0..65535) */
static BYTE status;		/* status of last I/O operation on FDC */
static BYTE dmadl;		/* current DMA address destination low */
static BYTE dmadh;		/* current DMA address destination high */
static BYTE clkcmd;		/* clock command */
static BYTE timer;		/* 10ms timer */
static int drivea;		/* fd for file "drivea.cpm" */
static int driveb;		/* fd for file "driveb.cpm" */
static int drivec;		/* fd for file "drivec.cpm" */
static int drived;		/* fd for file "drived.cpm" */
static int drivee;		/* fd for file "drivee.cpm" */
static int drivef;		/* fd for file "drivef.cpm" */
static int driveg;		/* fd for file "driveg.cpm" */
static int driveh;		/* fd for file "driveh.cpm" */
static int drivei;		/* fd for file "drivei.cpm" */
static int drivej;		/* fd for file "drivej.cpm" */
static int drivek;		/* fd for file "drivek.cpm" */
static int drivel;		/* fd for file "drivel.cpm" */
static int drivem;		/* fd for file "drivem.cpm" */
static int driven;		/* fd for file "driven.cpm" */
static int driveo;		/* fd for file "driveo.cpm" */
static int drivep;		/* fd for file "drivep.cpm" */
static int printer;		/* fd for file "printer.cpm" */
static int auxin;		/* fd for pipe "auxin" */
static int auxout;		/* fd for pipe "auxout" */
static int aux_in_eof;		/* status of pipe "auxin" (<>0 means EOF) */
static int pid_rec;		/* PID of the receiving process for auxiliary */
static char last_char;		/* buffer for 1 character (console status) */

#ifdef NETWORKING
static int ss[NUMSOC];		/* server socket descriptors */
static int ss_port[NUMSOC];	/* TCP/IP port for server sockets */
static int ss_telnet[NUMSOC];	/* telnet protocol flag for server sockets */
static int ssc[NUMSOC];		/* connected server socket descriptors */
static int cs;			/* client socket #1 descriptor */
static int cs_port;		/* TCP/IP port for cs */
static char cs_host[256];	/* hostname for cs */

#ifdef CNETDEBUG
static int cdirection = -1; /* protocol direction, 0 = send, 1 = receive */
#endif

#ifdef SNETDEBUG
static int sdirection = -1; /* protocol direction, 0 = send 1 = receive */
#endif

#endif

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
	{ "disks/drivej.cpm", &drivej, 255, 128 },
	{ "disks/drivek.cpm", &drivek, -1, -1 },
	{ "disks/drivel.cpm", &drivel, -1, -1 },
	{ "disks/drivem.cpm", &drivem, -1, -1 },
	{ "disks/driven.cpm", &driven, -1, -1 },
	{ "disks/driveo.cpm", &driveo, -1, -1 },
	{ "disks/drivep.cpm", &drivep, 256, 16384 }
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
 *
 * This is an example for 48KB segements as it was implemented originaly.
 * The segment size now can be configured via port 22.
 * If the segment size isn't configured the default is 48KB as it was
 * before, to maintain compatibility.
 */
#define MAXSEG 16		/* max. number of memory banks */
#define SEGSIZ 49152		/* default size of one bank = 48KBytes */
static char *mmu[MAXSEG];	/* MMU with pointers to the banks */
static int selbnk;		/* current bank */
static int maxbnk;		/* number of initialized banks */
static int segsize;		/* segment size of one bank, default 48KB */

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
static BYTE fdcsh_in(void), fdcsh_out(BYTE);
static BYTE fdco_in(void), fdco_out(BYTE);
static BYTE fdcx_in(void), fdcx_out(BYTE);
static BYTE dmal_in(void), dmal_out(BYTE);
static BYTE dmah_in(void), dmah_out(BYTE);
static BYTE mmui_in(void), mmui_out(BYTE), mmus_in(void), mmus_out(BYTE);
static BYTE mmuc_in(void), mmuc_out(BYTE);
static BYTE clkc_in(void), clkc_out(BYTE), clkd_in(void), clkd_out(BYTE);
static BYTE time_in(void), time_out(BYTE);
static BYTE delay_in(void), delay_out(BYTE);
static BYTE cond1_in(void), cond1_out(BYTE), cons1_in(void), cons1_out(BYTE);
static BYTE cond2_in(void), cond2_out(BYTE), cons2_in(void), cons2_out(BYTE);
static BYTE cond3_in(void), cond3_out(BYTE), cons3_in(void), cons3_out(BYTE);
static BYTE cond4_in(void), cond4_out(BYTE), cons4_in(void), cons4_out(BYTE);
static BYTE netd1_in(void), netd1_out(BYTE), nets1_in(void), nets1_out(BYTE);
static void int_timer(int);

static int to_bcd(int), get_date(struct tm *);

#ifdef NETWORKING
static void net_server_config(void), net_client_config(void);
static void init_server_socket(int);
static void int_io(int);
#endif

/*
 *	This array contains two function pointers for every
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
	{ fdcsh_in, fdcsh_out },	/* port 17 */
	{ io_trap, io_trap  },		/* port 18 */
	{ io_trap, io_trap  },		/* port 19 */
	{ mmui_in, mmui_out },		/* port 20 */
	{ mmus_in, mmus_out },		/* port 21 */
	{ mmuc_in, mmuc_out },		/* port 22 */
	{ io_trap, io_trap  },		/* port 23 */
	{ io_trap, io_trap  },		/* port 24 */
	{ clkc_in, clkc_out },		/* port 25 */
	{ clkd_in, clkd_out },		/* port 26 */
	{ time_in, time_out },		/* port 27 */
	{ delay_in, delay_out  },	/* port 28 */
	{ io_trap, io_trap  },		/* port 29 */
	{ io_trap, io_trap  },		/* port 30 */
	{ io_trap, io_trap  },		/* port 31 */
	{ io_trap, io_trap  },		/* port 32 */
	{ io_trap, io_trap  },		/* port 33 */
	{ io_trap, io_trap  },		/* port 34 */
	{ io_trap, io_trap  },		/* port 35 */
	{ io_trap, io_trap  },		/* port 36 */
	{ io_trap, io_trap  },		/* port 37 */
	{ io_trap, io_trap  },		/* port 38 */
	{ io_trap, io_trap  },		/* port 39 */
	{ cons1_in, cons1_out  },	/* port 40 */
	{ cond1_in, cond1_out  },	/* port 41 */
	{ cons2_in, cons2_out  },	/* port 42 */
	{ cond2_in, cond2_out  },	/* port 43 */
	{ cons3_in, cons3_out  },	/* port 44 */
	{ cond3_in, cond3_out  },	/* port 45 */
	{ cons4_in, cons4_out  },	/* port 46 */
	{ cond4_in, cond4_out  },	/* port 47 */
	{ io_trap, io_trap  },		/* port 48 */
	{ io_trap, io_trap  },		/* port 49 */
	{ nets1_in, nets1_out  },	/* port 50 */
	{ netd1_in, netd1_out  },	/* port 51 */
	{ io_trap, io_trap  },		/* port 52 */
	{ io_trap, io_trap  },		/* port 53 */
	{ io_trap, io_trap  }		/* port 54 */
};

/*
 *	This function initializes the I/O handlers:
 *	1. Initialize all unused ports with the I/O trap handler.
 *	2. Initialize the MMU with NULL pointers and defaults.
 *	3. Open the files which emulates the disk drives. The file
 *	   for drive A must be opened, or CP/M can't be booted.
 *	   Errors for opening one of the other 15 drives results
 *	   in a NULL pointer for fd in the dskdef structure,
 *	   so that this drive can't be used.
 *	4. Create and open the file "printer.cpm" for emulation
 *	   of a printer.
 *	5. Fork the process for receiving from the auxiliary serial port.
 *	6. Open the named pipes "auxin" and "auxout" for simulation
 *	   of the auxiliary serial port.
 *	7. Prepare TCP/IP sockets for serial port simulation
 */
void init_io(void)
{
	register int i;
#ifdef NETWORKING
	static struct sigaction newact;
#endif

	for (i = 55; i <= 255; i++) {
		port[i][0] = io_trap;
		port[i][1] = io_trap;
	}

	for (i = 0; i < MAXSEG; i++)
		mmu[i] = NULL;
	selbnk = 0;
	segsize = SEGSIZ;

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

#ifdef NETWORKING
	net_server_config();
	net_client_config();

	newact.sa_handler = int_io;
	sigaction(SIGIO, &newact, NULL);

	for (i = 0; i < NUMSOC; i++)
		init_server_socket(i);
#endif
}

#ifdef NETWORKING
/*
 * initialize a server socket
 */
static void init_server_socket(int n)
{
	struct sockaddr_in sin;
	int on = 1;
	int i;

	if (ss_port[n] == 0)
		return;
	if ((ss[n] = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("create server socket");
		exit(1);
	}
	if (setsockopt(ss[n], SOL_SOCKET, SO_REUSEADDR, (void *)&on,
	    sizeof(on)) == -1) {
		perror("server socket options");
		exit(1);
	}
	fcntl(ss[n], F_SETOWN, getpid());
	i = fcntl(ss[n], F_GETFL, 0);
	if (fcntl(ss[n], F_SETFL, i | FASYNC) == -1) {
		perror("fcntl FASYNC server socket");
		exit(1);
	}
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(ss_port[n]);
	if (bind(ss[n], (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		perror("bind server socket");
		exit(1);
	}
	if (listen(ss[n], 0) == -1) {
		perror("listen on server socket");
		exit(1);
	}
}

/*
 * Read and process network server configuration file
 */
static void net_server_config(void)
{
	register int i;
	FILE *fp;
	char buf[256];
	char *s;

	if ((fp = fopen("net_server.conf", "r")) != NULL) {
		printf("Server network configuration:\n");
		s = &buf[0];
		while (fgets(s, 256, fp) != NULL) {
			if (*s == '#')
				continue;
			i = atoi(s);
			if ((i < 1) || (i > 4)) {
				printf("console %d not supported\n", i);
				continue;
			}
			while((*s != ' ') && (*s != '\t'))
				s++;
			while((*s == ' ') || (*s == '\t'))
				s++;
			ss_telnet[i - 1] = atoi(s);
			while((*s != ' ') && (*s != '\t'))
				s++;
			while((*s == ' ') || (*s == '\t'))
				s++;
			ss_port[i - 1] = atoi(s);
			printf("console %d listening on port %d, telnet = %s\n",			       i, ss_port[i - 1],
			       ((ss_telnet[i - 1] > 0) ? "on" : "off"));
		}
		fclose(fp);
		printf("\n");
	}
}

/*
 * Read and process network client configuration file
 */
static void net_client_config(void)
{
	FILE *fp;
	char buf[256];
	char *s, *d;

	if ((fp = fopen("net_client.conf", "r")) != NULL) {
		printf("Client network configuration:\n");
		s = &buf[0];
		while (fgets(s, 256, fp) != NULL) {
			if (*s == '#')
				continue;
			while((*s != ' ') && (*s != '\t'))
				s++;
			while((*s == ' ') || (*s == '\t'))
				s++;
			d = &cs_host[0];
			while ((*s != ' ') && (*s != '\t'))
				*d++ = *s++;
			*d = '\0';
			while((*s == ' ') || (*s == '\t'))
				s++;
			cs_port = atoi(s);
			printf("Connecting to %s at port %d\n", cs_host,
			       cs_port);
		}
		fclose(fp);
		printf("\n");
	}
}
#endif

/*
 *	This function stops the I/O handlers:
 *
 *	1. The files emulating the disk drives are closed.
 *	2. The file "printer.com" emulating a printer is closed.
 *	3. The named pipes "auxin" and "auxout" are closed.
 *	4. All sockets are closed
 *	5. The receiving process for the aux serial port is stopped.
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
#ifdef NETWORKING
	for (i = 0; i < NUMSOC; i++)
		if (ss[i])
			close(ss[i]);
	for (i = 0; i < NUMSOC; i++)
		if (ssc[i])
			close(ssc[i]);
	if (cs)
		close(cs);
#endif
	kill(pid_rec, SIGHUP);
}

/*
 *	This function is called for every IN opcode from the
 *	CPU emulation. It calls the handler for the port,
 *	from which input is wanted.
 */
BYTE io_in(BYTE adr)
{
	return((*port[adr][0]) ());
}

/*
 *	This function is called for every OUT opcode from the
 *	CPU emulation. It calls the handler for the port,
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
 *	I/O handler for read console 0 status:
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
 *	I/O handler for read console 1 status:
 *	bit 0 = 1: input available
 *	bit 1 = 1: output writable
 */
static BYTE cons1_in(void)
{
	register BYTE status = 0;
#ifdef NETWORKING
	struct pollfd p[1];

	if (ssc[0] != 0) {
		p[0].fd = ssc[0];
		p[0].events = POLLIN | POLLOUT;
		p[0].revents = 0;
		poll(p, 1, 0);
		if (p[0].revents & POLLHUP) {
			close(ssc[0]);
			ssc[0] = 0;
			return(0);
		}
		if (p[0].revents & POLLIN)
			status |= 1;
		if (p[0].revents & POLLOUT)
			status |= 2;
	}
#endif
	return(status);
}

/*
 *	I/O handler for read console 2 status:
 *	bit 0 = 1: input available
 *	bit 1 = 1: output writable
 */
static BYTE cons2_in(void)
{
	register BYTE status = 0;
#ifdef NETWORKING
	struct pollfd p[1];

	if (ssc[1] != 0) {
		p[0].fd = ssc[1];
		p[0].events = POLLIN | POLLOUT;
		p[0].revents = 0;
		poll(p, 1, 0);
		if (p[0].revents & POLLHUP) {
			close(ssc[1]);
			ssc[1] = 0;
			return(0);
		}
		if (p[0].revents & POLLIN)
			status |= 1;
		if (p[0].revents & POLLOUT)
			status |= 2;
	}
#endif
	return(status);
}

/*
 *	I/O handler for read console 3 status:
 *	bit 0 = 1: input available
 *	bit 1 = 1: output writable
 */
static BYTE cons3_in(void)
{
	register BYTE status = 0;
#ifdef NETWORKING
	struct pollfd p[1];

	if (ssc[2] != 0) {
		p[0].fd = ssc[2];
		p[0].events = POLLIN | POLLOUT;
		p[0].revents = 0;
		poll(p, 1, 0);
		if (p[0].revents & POLLHUP) {
			close(ssc[2]);
			ssc[2] = 0;
			return(0);
		}
		if (p[0].revents & POLLIN)
			status |= 1;
		if (p[0].revents & POLLOUT)
			status |= 2;
	}
#endif
	return(status);
}

/*
 *	I/O handler for read console 4 status:
 *	bit 0 = 1: input available
 *	bit 1 = 1: output writable
 */
static BYTE cons4_in(void)
{
	register BYTE status = 0;
#ifdef NETWORKING
	struct pollfd p[1];

	if (ssc[3] != 0) {
		p[0].fd = ssc[3];
		p[0].events = POLLIN | POLLOUT;
		p[0].revents = 0;
		poll(p, 1, 0);
		if (p[0].revents & POLLHUP) {
			close(ssc[3]);
			ssc[3] = 0;
			return(0);
		}
		if (p[0].revents & POLLIN)
			status |= 1;
		if (p[0].revents & POLLOUT)
			status |= 2;
	}
#endif
	return(status);
}

/*
 *	I/O handler for read client socket 1 status:
 *	bit 0 = 1: input available
 *	bit 1 = 1: output writable
 */
static BYTE nets1_in(void)
{
	register BYTE status = 0;
#ifdef NETWORKING
	struct sockaddr_in sin;
	struct hostent *host;
	struct pollfd p[1];

	if ((cs == 0) && (cs_port != 0)) {
		host = gethostbyname(&cs_host[0]);
		if ((cs = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
			perror("create client socket");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
			return((BYTE) 0);
		}
		bzero((char *)&sin, sizeof(sin));
		bcopy(host->h_addr, (char *)&sin.sin_addr, host->h_length);
		sin.sin_family = AF_INET;
		sin.sin_port = htons(cs_port);
		if (connect(cs, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
			perror("connect client socket");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
			return((BYTE) 0);
		}
	}

	if (cs != 0) {
		p[0].fd = cs;
		p[0].events = POLLIN | POLLOUT;
		p[0].revents = 0;
		poll(p, 1, 0);
		if (p[0].revents & POLLHUP) {
			close(cs);
			cs = 0;
			return(0);
		}
		if (p[0].revents & POLLIN)
			status |= 1;
		if (p[0].revents & POLLOUT)
			status |= 2;
	}
#endif
	return(status);
}

/*
 *	I/O handler for write console 0 status:
 *	no reaction
 */
static BYTE cons_out(BYTE data)
{
	data = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for write console 1 status:
 *	no reaction
 */
static BYTE cons1_out(BYTE data)
{
	data = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for write console 2 status:
 *	no reaction
 */
static BYTE cons2_out(BYTE data)
{
	data = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for write console 3 status:
 *	no reaction
 */
static BYTE cons3_out(BYTE data)
{
	data = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for write console 4 status:
 *	no reaction
 */
static BYTE cons4_out(BYTE data)
{
	data = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for write client socket 1 status:
 *	no reaction
 */
static BYTE nets1_out(BYTE data)
{
	data = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read console 0 data:
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
 *	I/O handler for read console 1 data:
 */
static BYTE cond1_in(void)
{
	char c;
#ifdef NETWORKING
	char x;

	if (read(ssc[0], &c, 1) != 1) {
		if ((errno == EAGAIN) || (errno == EINTR)) {
			close(ssc[0]);
			ssc[0] = 0;
		} else {
			perror("read console 1");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
			return((BYTE) 0);
		}
	}
	if (ss_telnet[0] && (c == '\r'))
		read(ssc[0], &x, 1);
	if (ss_telnet[0] && ((BYTE) c == 0xff)) {
		read(ssc[0], &x, 1);
		read(ssc[0], &x, 1);
	}
#ifdef SNETDEBUG
	if (sdirection != 1) {
		printf("\n<- ");
		sdirection = 1;
	}
	printf("%02x ", (BYTE) c);
#endif
#else
	c = 0;
#endif
	return((BYTE) c);
}

/*
 *	I/O handler for read console 2 data:
 */
static BYTE cond2_in(void)
{
	char c;
#ifdef NETWORKING
	char x;

	if (read(ssc[1], &c, 1) != 1) {
		if ((errno == EAGAIN) || (errno == EINTR)) {
			close(ssc[1]);
			ssc[1] = 0;
		} else {
			perror("read console 2");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
			return((BYTE) 0);
		}
	}
	if (ss_telnet[1] && (c == '\r'))
		read(ssc[1], &x, 1);
	if (ss_telnet[1] && ((BYTE) c == 0xff)) {
		read(ssc[1], &x, 1);
		read(ssc[1], &x, 1);
	}
#ifdef SNETDEBUG
	if (sdirection != 1) {
		printf("\n<- ");
		sdirection = 1;
	}
	printf("%02x ", (BYTE) c);
#endif
#else
	c = 0;
#endif
	return((BYTE) c);
}

/*
 *	I/O handler for read console 3 data:
 */
static BYTE cond3_in(void)
{
	char c;
#ifdef NETWORKING
	char x;

	if (read(ssc[2], &c, 1) != 1) {
		if ((errno == EAGAIN) || (errno == EINTR)) {
			close(ssc[2]);
			ssc[2] = 0;
		} else {
			perror("read console 3");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
			return((BYTE) 0);
		}
	}
	if (ss_telnet[2] && (c == '\r'))
		read(ssc[2], &x, 1);
	if (ss_telnet[2] && ((BYTE) c == 0xff)) {
		read(ssc[2], &x, 1);
		read(ssc[2], &x, 1);
	}
#ifdef SNETDEBUG
	if (sdirection != 1) {
		printf("\n<- ");
		sdirection = 1;
	}
	printf("%02x ", (BYTE) c);
#endif
#else
	c = 0;
#endif
	return((BYTE) c);
}

/*
 *	I/O handler for read console 4 data:
 */
static BYTE cond4_in(void)
{
	char c;
#ifdef NETWORKING
	char x;

	if (read(ssc[3], &c, 1) != 1) {
		if ((errno == EAGAIN) || (errno == EINTR)) {
			close(ssc[3]);
			ssc[3] = 0;
		} else {
			perror("read console 4");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
			return((BYTE) 0);
		}
	}
	if (ss_telnet[3] && (c == '\r'))
		read(ssc[3], &x, 1);
	if (ss_telnet[3] && ((BYTE) c == 0xff)) {
		read(ssc[3], &x, 1);
		read(ssc[3], &x, 1);
	}
#ifdef SNETDEBUG
	if (sdirection != 1) {
		printf("\n<- ");
		sdirection = 1;
	}
	printf("%02x ", (BYTE) c);
#endif
#else
	c = 0;
#endif
	return((BYTE) c);
}

/*
 *	I/O handler for read client socket 1 data:
 */
static BYTE netd1_in(void)
{
	char c;

#ifdef NETWORKING
	if (read(cs, &c, 1) != 1) {
		perror("read client socket");
		cpu_error = IOERROR;
		cpu_state = STOPPED;
		return((BYTE) 0);
	}
#ifdef CNETDEBUG
	if (cdirection != 1) {
		printf("\n<- ");
		cdirection = 1;
	}
	printf("%02x ", (BYTE) c);
#endif
#else
	c = 0;
#endif
	return((BYTE) c);
}

/*
 *	I/O handler for write console 0 data:
 *	the output is written to the terminal
 */
static BYTE cond_out(BYTE data)
{
again:
	if (write(fileno(stdout), (char *) &data, 1) != 1) {
		if (errno == EINTR) {
			goto again;
		} else {
			perror("write console 0");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
		}
	}
	fflush(stdout);
	return((BYTE) 0);
}

/*
 *	I/O handler for write console 1 data:
 *	the output is written to the socket
 */
static BYTE cond1_out(BYTE data)
{
#ifdef NETWORKING
#ifdef SNETDEBUG
	if (sdirection != 0) {
		printf("\n-> ");
		sdirection = 0;
	}
	printf("%02x ", (BYTE) data);
#endif
again:
	if (write(ssc[0], (char *) &data, 1) != 1) {
		if (errno == EINTR) {
			goto again;
		} else {
			perror("write console 1");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
		}
	}
#endif
	return((BYTE) 0);
}

/*
 *	I/O handler for write console 2 data:
 *	the output is written to the socket
 */
static BYTE cond2_out(BYTE data)
{
#ifdef NETWORKING
#ifdef SNETDEBUG
	if (sdirection != 0) {
		printf("\n-> ");
		sdirection = 0;
	}
	printf("%02x ", (BYTE) data);
#endif
again:
	if (write(ssc[1], (char *) &data, 1) != 1) {
		if (errno == EINTR) {
			goto again;
		} else {
			perror("write console 2");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
		}
	}
#endif
	return((BYTE) 0);
}

/*
 *	I/O handler for write console 3 data:
 *	the output is written to the socket
 */
static BYTE cond3_out(BYTE data)
{
#ifdef NETWORKING
#ifdef SNETDEBUG
	if (sdirection != 0) {
		printf("\n-> ");
		sdirection = 0;
	}
	printf("%02x ", (BYTE) data);
#endif
again:
	if (write(ssc[2], (char *) &data, 1) != 1) {
		if (errno == EINTR) {
			goto again;
		} else {
			perror("write console 3");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
		}
	}
#endif
	return((BYTE) 0);
}

/*
 *	I/O handler for write console 4 data:
 *	the output is written to the socket
 */
static BYTE cond4_out(BYTE data)
{
#ifdef NETWORKING
#ifdef SNETDEBUG
	if (sdirection != 0) {
		printf("\n-> ");
		sdirection = 0;
	}
	printf("%02x ", (BYTE) data);
#endif
again:
	if (write(ssc[3], (char *) &data, 1) != 1) {
		if (errno == EINTR) {
			goto again;
		} else {
			perror("write console 4");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
		}
	}
#endif
	return((BYTE) 0);
}

/*
 *	I/O handler for write client socket 1 data:
 *	the output is written to the socket
 */
static BYTE netd1_out(BYTE data)
{
#ifdef NETWORKING
#ifdef CNETDEBUG
	if (cdirection != 0) {
		printf("\n-> ");
		cdirection = 0;
	}
	printf("%02x ", (BYTE) data);
#endif
again:
	if (write(cs, (char *) &data, 1) != 1) {
		if (errno == EINTR) {
			goto again;
		} else {
			perror("write client socket");
			cpu_error = IOERROR;
			cpu_state = STOPPED;
		}
	}
#endif
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
	if (data != '\r') {
again:
		if (write(printer, (char *) &data, 1) != 1) {
			if (errno == EINTR) {
				goto again;
			} else {
				perror("write printer");
				cpu_error = IOERROR;
				cpu_state = STOPPED;
			}
		}
	}
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
 *	I/O handler for read FDC sector low
 *	return low byte of the current sector
 */
static BYTE fdcs_in(void)
{
	return((BYTE) sector & 0xff);
}

/*
 *	I/O handler for write FDC sector low
 *	set low byte of the current sector
 */
static BYTE fdcs_out(BYTE data)
{
	sector = (sector & 0xff00) + data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read FDC sector high
 *	return high byte of the current sector
 */
static BYTE fdcsh_in(void)
{
	return((BYTE) sector >> 8);
}

/*
 *	I/O handler for write FDC sector high
 *	set high byte of the current sector
 */
static BYTE fdcsh_out(BYTE data)
{
	sector = (sector & 0xff) + (data << 8);
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
	register unsigned long pos;
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
		if ((mmu[i] = malloc(segsize)) == NULL) {
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
	//printf("SIM: memory select bank %d from %d\n", data, PC-ram);
	memcpy(mmu[selbnk], (char *) ram, segsize);
	memcpy((char *) ram, mmu[data], segsize);
	selbnk = data;
	return((BYTE) 0);
}

/*
 *	I/O handler for read MMU segment size configuration:
 *	returns size of the bank segments in pages a 256 bytes
 */
static BYTE mmuc_in(void)
{
	return((BYTE) (segsize >> 8));
}

/*
 *	I/O handler for write MMU segment size configuration:
 *	set the size of the bank segements in pages a 256 bytes
 *	must be done before any banks are allocated
 */
static BYTE mmuc_out(BYTE data)
{
	if (mmu[0] != NULL) {
		printf("Not possible to resize already allocated segments\n");
		exit(1);
	}
	segsize = data << 8;
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
 *	informations are returned from the system clock:
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
 *	CP/M 3 and MP/M 2 are Y2K bug fixed and can handle the date
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
 *	start or stop the 10ms interrupt timer
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
		tim.it_value.tv_usec = 10000;
		tim.it_interval.tv_sec = 0;
		tim.it_interval.tv_usec = 10000;
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
 *	return current status of 10ms interrupt timer,
 *	1 = enabled, 0 = disabled
 */
static BYTE time_in(void)
{
	return(timer);
}

/*
 *	I/O handler for write delay
 *	delay CPU for 10ms
 */
static BYTE delay_out(BYTE data)
{
	struct timespec timer;

	timer.tv_sec = 0;
	timer.tv_nsec = 10000000;
	nanosleep(&timer, NULL);

#ifdef CNETDEBUG
	printf(". ");
#endif

	return((BYTE) 0);
}

/*
 *	I/O handler for read delay
 *	returns 0
 */
static BYTE delay_in(void)
{
	return((BYTE) 0);
}

/*
 *	timer interrupt causes maskerable CPU interrupt
 */
static void int_timer(int sig)
{
	int_type = INT_INT;
}

#ifdef NETWORKING
/*
 *	SIGIO interrupt handler
 */
static void int_io(int sig)
{
	register int i;
	struct sockaddr_in fsin;
	struct pollfd p[NUMSOC];
	int alen;
	int go_away;
	char char_mode[3] = {255, 251, 3};
	char will_echo[3] = {255, 251, 1};

	for (i = 0; i < NUMSOC; i++) {
		p[i].fd = ss[i];
		p[i].events = POLLIN | POLLOUT;
		p[i].revents = 0;
	}

	poll(p, NUMSOC, 0);

	for (i = 0; i < NUMSOC; i++) {
		if ((ss[i] != 0) && (p[i].revents)) {

			alen = sizeof(fsin);

			if (ssc[i] != 0) {
				go_away = accept(ss[i],
						 (struct sockaddr *)&fsin,
						 &alen);
				close(go_away);
				return;
			}

			if ((ssc[i] = accept(ss[i], (struct sockaddr *)&fsin,
			    &alen)) == -1) {
				perror("accept server socket");
				ssc[i] = 0;
			}

			if (ss_telnet[i]) {
				write(ssc[i], &char_mode, 3);
				write(ssc[i], &will_echo, 3);
			}

		}
	}
}
#endif
