/*
 * Z80SIM  -  a	Z80-CPU	simulator
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

/*
 *	This modul contains the 'main()' function of the simulator,
 *	where the options are checked and variables are initialized.
 *	After initialization of the UNIX interrupts ( int_on() )
 *	and initialization of the I/O simulation ( init_io() )
 *	the user interface ( mon() ) is called.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <memory.h>
#include "sim.h"
#include "simglb.h"

static void save_core(void), load_core(void);
extern void int_on(void), int_off(void), mon(void);
extern void init_io(void), exit_io(void);
extern int exatoi(char *);

int main(int argc, char *argv[])
{
	register char *s, *p;
	register char *pn = argv[0];

	while (--argc >	0 && (*++argv)[0] == '-')
		for (s = argv[0] + 1; *s != '\0'; s++)
			switch (*s) {
			case 's':	/* save core and CPU on exit */
				s_flag = 1;
				break;
			case 'l':	/* load core and CPU from file */
				l_flag = 1;
				break;
			case 'i':	/* trap I/O on unused ports */
				i_flag = 1;
				break;
			case 'm':	/* initialize Z80 memory */
				m_flag = exatoi(s+1);
				s += strlen(s+1);
				break;
			case 'f':	/* set emulation speed */
				f_flag = atoi(s+1);
				s += strlen(s+1);
				tmax = f_flag * 10000;
				break;
			case 'x':	/* get filename with Z80 executable */
				x_flag = 1;
				s++;
				p = xfn;
				while (*s)
					*p++ = *s++;
				*p = '\0';
				s--;
				break;
			case '?':
				goto usage;
			default:
				printf("illegal option %c\n", *s);
usage:				printf("usage:\t%s -s -l -i -mn -fn -xfilename\n", pn);
				puts("\ts = save core and cpu");
				puts("\tl = load core and cpu");
				puts("\ti = trap on I/O to unused ports");
				puts("\tm = init memory with n");
				puts("\tf = CPU frequenzy n in MHz");
				puts("\tx = load and execute filename");
				exit(1);
			}

	putchar('\n');
	puts("#######  #####    ###            #####    ###   #     #");
	puts("     #  #     #  #   #          #     #    #    ##   ##");
	puts("    #   #     # #     #         #          #    # # # #");
	puts("   #     #####  #     #  #####   #####     #    #  #  #");
	puts("  #     #     # #     #               #    #    #     #");
	puts(" #      #     #  #   #          #     #    #    #     #");
	puts("#######  #####    ###            #####    ###   #     #");
	printf("\nRelease %s, %s\n", RELEASE, COPYR);
#ifdef	USR_COM
	printf("%s Release %s, %s\n", USR_COM, USR_REL,	USR_CPR);
#endif
	putchar('\n');
	fflush(stdout);

	wrk_ram	= PC = STACK = ram;
	memset((char *)	ram, m_flag, 32768);
	memset((char *)	ram + 32768, m_flag, 32768);
	if (l_flag)
		load_core();
	int_on();
	init_io();
	mon();
	if (s_flag)
		save_core();
	exit_io();
	int_off();
	return(0);
}

/*
 *	This function saves the CPU and the memory into the file core.z80
 *
 */
static void save_core(void)
{
	int fd;

	if ((fd	= open("core.z80", O_WRONLY | O_CREAT, 0600)) == -1) {
		puts("can't open file core.z80");
		return;
	}
	write(fd, (char	*) &A, sizeof(A));
	write(fd, (char	*) &F, sizeof(F));
	write(fd, (char	*) &B, sizeof(B));
	write(fd, (char	*) &C, sizeof(C));
	write(fd, (char	*) &D, sizeof(D));
	write(fd, (char	*) &E, sizeof(E));
	write(fd, (char	*) &H, sizeof(H));
	write(fd, (char	*) &L, sizeof(L));
	write(fd, (char	*) &A_,	sizeof(A_));
	write(fd, (char	*) &F_,	sizeof(F_));
	write(fd, (char	*) &B_,	sizeof(B_));
	write(fd, (char	*) &C_,	sizeof(C_));
	write(fd, (char	*) &D_,	sizeof(D_));
	write(fd, (char	*) &E_,	sizeof(E_));
	write(fd, (char	*) &H_,	sizeof(H_));
	write(fd, (char	*) &L_,	sizeof(L_));
	write(fd, (char	*) &I, sizeof(I));
	write(fd, (char	*) &IFF, sizeof(IFF));
	write(fd, (char	*) &R, sizeof(R));
	write(fd, (char	*) &PC,	sizeof(PC));
	write(fd, (char	*) &STACK, sizeof(STACK));
	write(fd, (char	*) &IX,	sizeof(IX));
	write(fd, (char	*) &IY,	sizeof(IY));
	write(fd, (char	*) ram,	32768);
	write(fd, (char	*) ram + 32768,	32768);
	close(fd);
}

/*
 *	This function loads the CPU and memory from the file core.z80
 *
 */
static void load_core(void)
{
	int fd;

	if ((fd	= open("core.z80", O_RDONLY)) == -1) {
		puts("can't open file core.z80");
		return;
	}
	read(fd, (char *) &A, sizeof(A));
	read(fd, (char *) &F, sizeof(F));
	read(fd, (char *) &B, sizeof(B));
	read(fd, (char *) &C, sizeof(C));
	read(fd, (char *) &D, sizeof(D));
	read(fd, (char *) &E, sizeof(E));
	read(fd, (char *) &H, sizeof(H));
	read(fd, (char *) &L, sizeof(L));
	read(fd, (char *) &A_, sizeof(A_));
	read(fd, (char *) &F_, sizeof(F_));
	read(fd, (char *) &B_, sizeof(B_));
	read(fd, (char *) &C_, sizeof(C_));
	read(fd, (char *) &D_, sizeof(D_));
	read(fd, (char *) &E_, sizeof(E_));
	read(fd, (char *) &H_, sizeof(H_));
	read(fd, (char *) &L_, sizeof(L_));
	read(fd, (char *) &I, sizeof(I));
	read(fd, (char *) &IFF,	sizeof(IFF));
	read(fd, (char *) &R, sizeof(R));
	read(fd, (char *) &PC, sizeof(PC));
	read(fd, (char *) &STACK, sizeof(STACK));
	read(fd, (char *) &IX, sizeof(IX));
	read(fd, (char *) &IY, sizeof(IY));
	read(fd, (char *) ram, 32768);
	read(fd, (char *) ram +	32768, 32768);
	close(fd);
}
