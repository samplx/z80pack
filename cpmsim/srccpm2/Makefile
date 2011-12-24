CFLAGS= -O -s

all: format putsys bin2hex send receive bios.bin boot.bin
	@echo "done"

format: format.c
	cc $(CFLAGS) -o format format.c
	cp format ..

putsys: putsys.c
	cc $(CFLAGS) -o putsys putsys.c

bin2hex: bin2hex.c
	cc $(CFLAGS) -o bin2hex bin2hex.c
	cp bin2hex ..

send: send.c
	cc $(CFLAGS) -o send send.c
	cp send ..

receive: receive.c
	cc $(CFLAGS) -o receive receive.c
	cp receive ..

bios.bin: bios.asm
	z80asm -vl -sn -x bios.asm

boot.bin: boot.asm
	z80asm -vl -sn boot.asm

clean:
	rm -f *.lis bios.bin boot.bin format putsys bin2hex receive send
