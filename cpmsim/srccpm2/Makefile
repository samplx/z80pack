CC = gcc

CFLAGS= -O -s -Wall

all: format putsys bin2hex send receive bios.bin boot.bin
	@echo "done"

format: format.c
	$(CC) $(CFLAGS) -o format format.c
	cp format ..

putsys: putsys.c
	$(CC) $(CFLAGS) -o putsys putsys.c

bin2hex: bin2hex.c
	$(CC) $(CFLAGS) -o bin2hex bin2hex.c
	cp bin2hex ..

send: send.c
	$(CC) $(CFLAGS) -o send send.c
	cp send ..

receive: receive.c
	$(CC) $(CFLAGS) -o receive receive.c
	cp receive ..

bios.bin: bios.asm
	z80asm -vl -sn -x bios.asm

boot.bin: boot.asm
	z80asm -vl -sn boot.asm

clean:
	rm -f *.lis bios.bin boot.bin format putsys bin2hex receive send
