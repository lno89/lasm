LASM, Line Assembler
====================
<sup><sub>by lno89/flash-computer(same guy I just forgot the email I used to make the first one lol)

I really don't know what else to say.

It's just a wrapper over the Netwide Assembler for the x86 family of architectures to quickly assemble short bursts of code and make simple programs in machine language, raw instructions specified in assembly pnemonics, instead of all the luxury(and overhead) assemblers afford, much like the monitors of yore.

This is a substandard C code base with a monolithic source file and I intend to keep it this way(because I'm not going to touch it anymore lol, but also because I like hard to navigate but easy to move around monolithic source code). It probably has a few bugs here and there and I wanted to make a better, static parse tree but I really don't have enough need of it to actually improve anything.

I use this to create my tiny elf programs.

Usage:
```
lasm [options] output_file(Default: stdout)
Options:
	-a : ASCII output(equivalent to #ASCII command)
	-c : Specify command buffer length
	-d : Architecture = x86-32(Default: x86-64)
	-i : Specify alternate input stream/file(Default: stdin)
	-l : Specify input buffer length
	-m : Multiline mode(equivalent to #MULTILINE command. Terminate block with #ASSEMBLE command)
	-p : Specify path of nasm, inclusive of the binary(Default: 'nasm', left to the environment)
	-P : Specify a new prompt for interactive mode
	-q : Architecture = x86-64(Default)
	-t : Specify alternate temporary file to communicate with nasm(Default: ./.lasmtemp)
		Due to how nasm functions, the path of the tempfile with ".o" appended will be reserved and a buffer of the length of the {tempfile name} +2 will be allocated dynamically
	-w : Architecture = x86-16(Default: x86-64)
	-h : Print this help
```

This prompt looks way prettier in a terminal emulator by the way. It's a pretty cli program for something I hacked together on a weekend in general.

Also the correct Tab value is 4-width hard tab for high level language projects like this.
For assembly, it's 8-width hard tabs. Soft tabs suck.
