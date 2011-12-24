/*
 * CP/M 2.2 Formats a simulated Disk Drive
 *
 * Copyright (C) 1988-2006 by Udo Munk
 *
 * History:
 * 29-APR-88 Development on TARGON/35 with AT&T Unix System V.3
 * 11-MAR-93 comments in english
 * 01-OCT-06 modified to compile on modern POSIX OS's
 * 18-NOV-06 added a second harddisk
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <fcntl.h>

#define TRACK   	77
#define SECTOR  	26
#define HDTRACK		255
#define HDSECTOR	128

/*
 *	This program is able to format the following disk formats:
 *
 *		drive A:	8" IBM SS,SD
 *		drive B:	8" IBM SS,SD
 *		drive C:	8" IBM SS,SD
 *		drive D:	8" IBM SS,SD
 *		drive I:	4MB harddisk
 *		drive J:	4MB harddisk
 */
int main(int argc, char *argv[])
{
	register int i;
	int fd;
	char drive;
	static unsigned char sector[128];
	static char fn[] = "disks/drive?.cpm";
	static char usage[] = "usage: format a | b | c | d | i | j";

	if (argc != 2) {
		puts(usage);
		exit(1);
	}
	i = *argv[1];
	if (argc != 2 ||
	    (i != 'a' && i != 'b' && i != 'c' && i != 'd' && i != 'i'
	     && i != 'j')) {
		puts(usage);
		exit(1);
	}
	fn[11] = drive = (char) i;
	memset((char *) sector, 0xe5, 128);
	if ((fd = creat(fn, 0644)) == -1) {
		perror("disk file");
		exit(1);
	}
	if (drive != 'i' && drive != 'j') {
		for (i = 0; i < TRACK * SECTOR; i++)
			write(fd, (char *) sector, 128);
	} else {
		for (i = 0; i < HDTRACK * HDSECTOR; i++)
			write(fd, (char *) sector, 128);
	}
	close(fd);
	return(0);
}
