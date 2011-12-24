# Makefile for Z80-CPU emulator unter UNIX System V

CFLAGS= -O -c -Wall
LFLAGS=	-s

#CFLAGS= -g -c -Wall
#LFLAGS=

OBJ =	sim0.o \
	sim1.o \
	sim2.o \
	sim3.o \
	sim4.o \
	sim5.o \
	sim6.o \
	sim7.o \
	simctl.o \
	disas.o	\
	simint.o \
	iosim.o	\
	simfun.o \
	simglb.o

z80sim : $(OBJ)
	cc $(OBJ) $(LFLAGS) -o z80sim

sim0.o : sim0.c	sim.h simglb.h
	cc $(CFLAGS) sim0.c

sim1.o : sim1.c	sim.h simglb.h
	cc $(CFLAGS) sim1.c

sim2.o : sim2.c	sim.h simglb.h
	cc $(CFLAGS) sim2.c

sim3.o : sim3.c	sim.h simglb.h
	cc $(CFLAGS) sim3.c

sim4.o : sim4.c	sim.h simglb.h
	cc $(CFLAGS) sim4.c

sim5.o : sim5.c	sim.h simglb.h
	cc $(CFLAGS) sim5.c

sim6.o : sim6.c	sim.h simglb.h
	cc $(CFLAGS) sim6.c

sim7.o : sim7.c	sim.h simglb.h
	cc $(CFLAGS) sim7.c

simctl.o : simctl.c sim.h simglb.h
	cc $(CFLAGS) simctl.c

disas.o	: disas.c
	cc $(CFLAGS) disas.c

simint.o : simint.c sim.h simglb.h
	cc $(CFLAGS) simint.c

iosim.o	: iosim.c sim.h	simglb.h
	cc $(CFLAGS) iosim.c

simfun.o : simfun.c sim.h
	cc $(CFLAGS) simfun.c

simglb.o : simglb.c sim.h
	cc $(CFLAGS) simglb.c

clean:
	rm -f *.o core z80sim
