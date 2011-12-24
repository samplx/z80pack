	Quickstart to run CP/M and MP/M on the Z80-CPU simulation

1. Change to directory ~/z80pack/cpmsim/srcsim
   make
   make clean
This compiles the CPU and hardware emulation needed to run CP/M and MP/M.

2. Change to directory ~/z80pack/cpmsim/srccpm2
   make
   make clean
This compiles support programs, see below.

3. Make backup copies of your distribution disks!
   cd ~/z80pack/cpmsim/disks/library
   cp *.dsk ../backups

4. Change to directory ~/z80pack/cpmsim
   cpm2 - run CP/M 2.2
   cpm3 - run CP/M 3.0
   mpm  - this boots CP/M 2, run command mpmldr to boot MP/M 2

Usage of the support programs:

format:	to create an empty disk image for the CP/M simulation.
	input: format <a | b | c | d | i | j | p>
	output: in directory disks files drivea.cpm, driveb.cpm,
		drivec.cpm, drived.cpm, drivei.cpm, drivej.cpm
		and drivep.cpm

bin2hex:converts binary files to Intel hex.

receive:This is a process spawned by cpmsim. It reads from the named
	pipe auxout and writes all input from the pipe to the file,
	which is given as first argument. cpmsim spawns this process
	with the output filename auxiliary.cpm. Inside the simulator
	this pipe is connected to I/O-port 5, which is assigned
	to the CP/M device PUN:. So everything you write from CP/M
	to device PUN: goes into the file auxiliary.cpm on the
	UNIX host.

send:	This process is to send a file from the UNIX host to the
	simulator. Type send <filename> &, and then run cpmsim.
	The process writes all data from file into the named pipe
	auxin, which is also connected to I/O-port 5, which is
	assigned to the CP/M device RDR:. You may use this to
	transfer a file from the UNIX host to the simulator.
	Under CP/M type pip file=RDR: to read the data send from
	the process on the UNIX host.

If you use PIP to transfer files between the UNIX host and the
simulator, you can only use ASCII files, because pip uses cntl-z
for EOF! To transfer a binary file from the UNIX host to the
simulator convert it to Intel hex format with bin2hex. This
can be converted back to a binary file under CP/M with the LOAD
command.
