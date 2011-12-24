/*
 * Z80SIM  -  a	Z80-CPU	simulator
 *
 * Copyright (C) 1987-2006 by Udo Munk
 *
 * This modul of the simulator contains a simple terminal I/O
 * simulation as an example.
 *
 * History:
 * 28-SEP-87 Development on TARGON/35 with AT&T Unix System V.3
 * 11-JAN-89 Release 1.1
 * 08-FEB-89 Release 1.2
 * 13-MAR-89 Release 1.3
 * 09-FEB-90 Release 1.4 Ported to TARGON/31 M10/30
 * 20-DEC-90 Release 1.5 Ported to COHERENT 3.0
 * 10-JUN-92 Release 1.6 long casting problem solved with COHERENT 3.2
 *			 and some optimization
 * 25-JUN-92 Release 1.7 comments in english
 * 03-OCT-06 Release 1.8 changed to ANSI C for modern POSIX OS's
 */

/*
 *	Sample I/O-handler
 *
 *	Port 0 input:	reads the next byte from stdin
 *	Port 0 output:	writes the byte to stdout
 *
 *	All the other ports are connected to a I/O-trap handler,
 *	I/O to this ports stops the simulation with an I/O error.
 */

#include <stdio.h>
#include "sim.h"
#include "simglb.h"

/*
 *	Forward declarations of the I/O functions
 *	for all port addresses.
 */
static BYTE io_trap(void);
static BYTE p000_in(void);
static void p000_out(BYTE);

/*
 *	This two dimensional array contains function pointers
 *	for every I/O port (0 - 255), to do the needed I/O.
 *	The first entry is for input, the second for output.
 */
static BYTE (*port[256][2]) () = {
	{ p000_in, p000_out }		/* port	0 */
};

/*
 *	This function is to initiate the I/O devices.
 *	It will be called from the CPU simulation before
 *	any operation with the Z80 is possible.
 *
 *	In this sample I/O simulation we initialize all
 *	unused port with an error trap handler, so that
 *	simulation stops at I/O on the unused ports.
 *
 *	See the I/O simulation of CP/M for a more complex
 *	example.
 */
void init_io(void)
{
	register int i;

	for (i = 1; i <= 255; i++)
		port[i][0] = port[i][1]	= io_trap;
}

/*
 *	This function is to stop the I/O devices. It is
 *	called from the CPU simulation on exit.
 *
 *	Here is just nothing to do, see the I/O simulation
 *	of CP/M for a more complex example.
 */
void exit_io(void)
{
}

/*
 *	This is the main handler for all IN op-codes,
 *	called by the simulator. It calls the input
 *	function for port adr.
 */
BYTE io_in(BYTE adr)
{
	return((*port[adr][0]) ());
}

/*
 *	This is the main handler for all OUT op-codes,
 *	called by the simulator. It calls the output
 *	function for port adr.
 */
void io_out(BYTE adr, BYTE data)
{
	(*port[adr][1])	(data);
}

/*
 *	I/O trap funtion
 *	This function should be added into all unused
 *	entrys of the port array. It stops the emulation
 *	with an I/O error.
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
 *	I/O function port 0 read:
 *	Read next byte from stdin.
 */
static BYTE p000_in(void)
{
	return((BYTE) getchar());
}

/*
 *	I/O function port 0 write:
 *	Write byte to stdout and flush the output.
 */
static void p000_out(BYTE data)
{
	putchar((int) data);
	fflush(stdout);
}
