/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 1987-2006 by Udo Munk
 *
 * History:
 * 28-SEP-87 Development on TARGON/35 with AT&T Unix System V.3
 * 14-MAR-89 new option -l
 * 23-DEC-90 Ported to COHERENT 3.0
 * 06-OCT-06 modified to compile on modern POSIX OS's
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
