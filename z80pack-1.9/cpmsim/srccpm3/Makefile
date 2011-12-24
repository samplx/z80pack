CFLAGS= -O -s

all: putsys boot.bin
	echo done

putsys: putsys.c
	cc $(CFLAGS) -o putsys putsys.c

boot.bin: boot.asm
	z80asm -vl -sn -fb boot.asm

clean:
	rm -f *.lis putsys boot.bin
