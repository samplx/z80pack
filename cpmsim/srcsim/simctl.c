/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 1987-2007 by Udo Munk
 *
 * History:
 * 28-SEP-87 Development on TARGON/35 with AT&T Unix System V.3
 * 11-JAN-89 Release 1.1
 * 08-FEB-89 Release 1.2
 * 13-MAR-89 Release 1.3
 * 09-FEB-90 Release 1.4  Ported to TARGON/31 M10/30
 * 20-DEC-90 Release 1.5  Ported to COHERENT 3.0
 * 10-JUN-92 Release 1.6  long casting problem solved with COHERENT 3.2
 *			  and some optimization
 * 25-JUN-92 Release 1.7  comments in english and ported to COHERENT 4.0
 * 02-OCT-06 Release 1.8  modified to compile on modern POSIX OS's
 * 18-NOV-06 Release 1.9  modified to work with CP/M sources
 * 08-DEC-06 Release 1.10 modified MMU for working with CP/NET
 * 17-DEC-06 Release 1.11 TCP/IP sockets for CP/NET
 * 25-DEC-06 Release 1.12 CPU speed option
 * 19-FEB-07 Release 1.13 various improvements
 * 06-OCT-07 Release 1.14 bug fixes and improvements
 */

#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include "sim.h"
#include "simglb.h"

extern void cpu(void);

struct termios old_term, new_term;

/*
 *	This function gets the CP/M boot sector from first track/sector
 *	of disk drive A (file drivea.cpm) into memory started at 0.
 *	Then the Z80 CPU emulation is started and the system should boot.
 */
void mon(void)
{
	register int fd;

	if (!l_flag) {
		if ((fd = open("disks/drivea.cpm", O_RDONLY)) == -1) {
			perror("file disks/drivea.cpm");
			return;
		}
		if (read(fd, (char *) ram, 128) != 128) {
			perror("file disks/drivea.cpm");
			return;
		}
		close(fd);
	}

	tcgetattr(0, &old_term);
	new_term = old_term;
	new_term.c_lflag &= ~(ICANON | ECHO);
	new_term.c_iflag &= ~(IXON | IXANY | IXOFF);
	new_term.c_iflag &= ~(IGNCR | ICRNL | INLCR);
	new_term.c_cc[VMIN] = 1;
#ifndef CNTL_Z
	new_term.c_cc[VSUSP] = 0;
#endif
	tcsetattr(0, TCSADRAIN, &new_term);

	cpu_state = CONTIN_RUN;
	cpu_error = NONE;
	cpu();

	tcsetattr(0, TCSADRAIN, &old_term);

	switch (cpu_error) {
	case NONE:
		break;
	case OPHALT:
		printf("\nHALT Op-Code reached at %04x\n",
		       (unsigned int)(PC - ram - 1));
		break;
	case IOTRAP:
		printf("\nI/O Trap at %04x\n", (unsigned int)(PC - ram));
		break;
	case IOERROR:
		printf("\nFatal I/O Error at %04x\n", (unsigned int)(PC - ram));
		break;
	case OPTRAP1:
		printf("\nOp-code trap at %04x %02x\n",
		       (unsigned int)(PC - 1 - ram), *(PC - 1));
		break;
	case OPTRAP2:
		printf("\nOp-code trap at %04x %02x %02x\n",
		       (unsigned int)(PC - 2 - ram), *(PC - 2), *(PC - 1));
		break;
	case OPTRAP4:
		printf("\nOp-code trap at %04x %02x %02x %02x %02x\n",
		       (unsigned int)(PC - 4 - ram), *(PC - 4), *(PC - 3),
		       *(PC - 2), *(PC - 1));
		break;
	case USERINT:
		puts("\nUser Interrupt");
		break;
	default:
		printf("\nUnknown error %d\n", cpu_error);
		break;
	}
}
