/* Preprocessor information */

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

#define VER_MAJ 0
#define VER_MIN 1

#define IBL 0x100 // Default Input buffer length
#define CBL 0x100 // Command buffer length

#define COL_RD "\x1b[31m"
#define COL_GR "\x1b[32m"
#define COL_MG "\x1b[35m"

#define BG_RD "\x1b[41m"
#define BG_GR "\x1b[42m"
#define BG_MG "\x1b[45m"

#define FM_BOLD "\x1b[1m"
#define FM_THIN "\x1b[2m"
#define FM_ITAL "\x1b[3m"
#define FM_UNLN "\x1b[4m"
#define FM_BLN1 "\x1b[5m"
#define FM_BLN2 "\x1b[6m"
#define FM_INVR "\x1b[7m"
#define FM_DISA "\x1b[8m"
#define FM_STRK "\x1b[9m"

#define FATAL_ERR_MSG COL_RD FM_BLN1 FM_BOLD FM_INVR FM_UNLN "FATAL ERROR:" RESET " "
#define WARN_MSG COL_MG FM_BOLD "WARNING:" RESET " "

#define RESET "\x1b[0m"

/* Static information */

static char *help_fmt = FM_BOLD "Line Assembler version %u.%u" RESET "\nA line by line assembler wrapper for the Netwide Assembler on linux\n\n\t" FM_BOLD "Usage" RESET ":\n\t\tlasm [options] output_file(Default: stdout)\n\n\t" FM_BOLD "Options" RESET ":\n\t\t-a : ASCII output(equivalent to #ASCII command)\n\t\t-c : Specify command buffer length(Default: %lu)\n\t\t-d : Architecture = x86-32(Default: x86-64)\n\t\t-i : Specify alternate input stream/file(Default: stdin)\n\t\t-l : Specify input buffer length(Default: %lu)\n\t\t-m : Multiline mode(equivalent to #MULTILINE command. Terminate block with #ASSEMBLE command)\n\t\t-p : Specify path of nasm, inclusive of the binary(Default: \'nasm\', left to the environment)\n\t\t-P : Specify a new prompt for interactive mode(Default: \'%s\')\n\t\t-q : Architecture = x86-64(Default)\n\t\t-t : Specify alternate temporary file to communicate with nasm(Default: ./.lasmtemp)\n\t\t\t" FM_UNLN "Due to how nasm functions, the path of the tempfile with \".o\" appended will be reserved and a buffer of the length of the {tempfile name} +2 will be allocated dynamically" RESET "\n\t\t-w : Architecture = x86-16(Default: x86-64)\n\t\t-h : Print this help\n\n\t" FM_BOLD "INTERACTIVE COMMANDS" RESET ":\n\t\t#ASCII : ASCII output(equivalent to -a flag)\n\t\t#ASSEMBLE : Assemble current buffer(for multiline mode)\n\t\t#MULTILINE : Multiline mode(equivalent to -m flag. Terminate block with #ASSEMBLE command)\n\t\t#EXIT : Exit program\n\n\t\tNote: " FM_UNLN "Commands are case insensitive and the lowest string that can identify a command unambigously will suffice\n\t\t\t#asc, #ass, #m and #e are thus valid" RESET "\n";
static char *insuf_arg_fmt_s = WARN_MSG "%s not provided after %s flag, defaulting to %s\n";
static char *insuf_arg_fmt_lu = WARN_MSG "%s not provided after %s flag, defaulting to %lu\n";
static char *inv_int_fmt = WARN_MSG "Invalid integer input. Reason: " FM_UNLN "%s" RESET ". defaulting to %lu\n";
static char *malloc_fail_fmt = FATAL_ERR_MSG "Memory allocation faliure on " FM_UNLN "%s" RESET "\nAttempted allocation size: %lu bytes\n";

static char *inv_int_reasons[2] = {"Invalid characters encountered", "Input too big"};

static char *def_tempfile_n = ".lasmtemp";
static char *def_prompt = "+>> ";
static char *def_nasm_path = "nasm";

/* Structs */

struct assembler_state
{
	FILE *ofile;
	FILE *ifile;
	FILE *tempfile;
	char *ofile_n;
	char *ifile_n;
	char *tempfile_n;
	char *nasm_path;
	char *prompt;
	unsigned int mgmt_flags;
	unsigned int mode;
	unsigned int arch;
	char *ibuffer;
	char *cbuffer;
	char *altfile_n_buffer;
	size_t ib_len;
	size_t cb_len;
	size_t tempfile_n_len;
};

struct parse_return_val
{
	int error;
	size_t adv;
};

typedef struct parse_return_val prv;

struct parse_graph_node
{
	char match;
	prv (*match_func)(char *, char *, struct parse_graph_node *,  struct assembler_state *);
	size_t children_count;
	struct parse_graph_node *children;
};

typedef struct parse_graph_node pgn;

/* Program */

size_t strlen(char *str)
{
	char *itr = str;
	while(*(itr++));
	return itr-str;
}

int assemble(struct assembler_state *state) // Temporary while I figure out how to make more robust solutions portable between linux and windows, hopefully also eliminating the need of a tempfile with it.
{
	size_t command_len = snprintf(state->cbuffer, state->cb_len, "%s --before \'BITS %u\' %s -o %s", state->nasm_path, state->arch, state->tempfile_n, state->altfile_n_buffer);
	//DEBUG_DELIM fprintf(stdout, "Command: " FM_BOLD "%s" RESET "\n", state->cbuffer);
	int retval = system(state->cbuffer);
	if(retval)
	{
		fprintf(stderr, FATAL_ERR_MSG "NASM could not process the input. Check the path to the NASM binary and the input for correctness.\n");
		exit(0x8);
	}
	return 0;
}

int write_to_ofile(struct assembler_state *state)
{
	char fallback[2];
	char *base;
	size_t len;
	if(state->cb_len < 2)
	{
		base = fallback;
		len = 2;
	}
	else
	{
		base = state->cbuffer;
		len = state->cb_len & (~1);
	}

	if(state->mode & 0x1)
	{
		int read_char;
		char *itr = base;
		while((read_char = fgetc(state->tempfile)) != EOF)
		{
			itr[1] = read_char & 0xF;
			itr[1] += (itr[1] < 0xA)?0x30:0x37;
			itr[0] = (read_char & 0xF0)>>4;
			itr[0] += (itr[0] < 0xA)?0x30:0x37;
			itr += 2;
			if(itr-base >= len)
			{
				fwrite(base, itr-base, 1, state->ofile);
			}
		}
		fwrite(base, itr-base, 1, state->ofile);

		return 0;
	}
	size_t read_bytes;
	while((read_bytes = fread(base, 1, len, state->tempfile)))
	{
		fwrite(base, read_bytes, 1, state->ofile);
	}
	return 0;
}

size_t fill_ibuffer(char *ibuffer, size_t ib_len, FILE *ifile)
{
	char *itr = ibuffer;
	int read_char = 0;
	while(itr-ibuffer < ib_len - 1)
	{
		read_char = fgetc(ifile);
		switch(read_char)
		{
			case EOF:
			case '\n':
				*itr = 0;
				return itr - ibuffer;
			default:
				*itr = (char)read_char;
		}
		itr++;
	}
	*itr = 0;
	return itr - ibuffer;
}

void close_tempfile(struct assembler_state *state)
{

	tempfile_close_retry:
	if(fclose(state->tempfile))
	{
		fprintf(stderr, WARN_MSG "Failed to close temp file.\n Retry?(y/n): ");
		fill_ibuffer(state->ibuffer, 2, stdin);
		if(state->ibuffer[0] == 'y' | state->ibuffer[0] == 'Y')
		{
			goto tempfile_close_retry;
		}
		else
		{
			fprintf(stderr, FATAL_ERR_MSG "Could not close temp file. Check for obstructions and try again.\n");
			exit(0x2);
		}
	}
	state->tempfile = NULL;
	return;
}

void full_assemble(struct assembler_state *state)
{
	if(state->mgmt_flags & 0x4)
	{
		return;
	}
	close_tempfile(state);
	assemble(state);
	state->tempfile = fopen(state->altfile_n_buffer, "rb");
	if(!(state->tempfile))
	{
		fprintf(stderr, FATAL_ERR_MSG "Alt tempfile is not accessible\n");
		exit(0x2);
	}
	write_to_ofile(state);
	close_tempfile(state);
	return;
}

int size_t_parse_str(char *str, size_t *tar)
{
	char *fritr = str;
	char *bkitr = NULL;
	unsigned char radix = 10;
	size_t factor = 1;

	if(str[0] == '0')
	{
		fritr++;
		radix = 010;
		if(str[1] == 'x' || str[1] == 'X')
		{
			fritr++;
			radix = 0x10;
		}
	}

	bkitr = fritr;

	if(radix < 11)
	{
		while(*bkitr)
		{
			if(*bkitr - '0' > radix || *bkitr < '0')
			{
				return -1;
			}
			bkitr++;
		}
	}
	else if(radix < 37)
	{
		while(*bkitr)
		{
			if(*bkitr < '0')
			{
				return -1;
			}
			if(*bkitr - '0' > 9)
			{
				if(!(*bkitr & 0x40))
				{
					return -1;
				}
				if((*bkitr & 0x1F) > (radix - 10))
				{
					return -1;
				}
			}
			bkitr++;
		}
	}
	else
	{
		return -1;
	}

	size_t last = 0;
	*tar = 0;

	while(bkitr > fritr)
	{
		bkitr--;
		last = *tar;
		*tar += factor * ((*bkitr - '0' < 10)?*bkitr - '0':(*bkitr & 0x1F) + 9);
		if(last > *tar)
		{
			return -2; // Unsigned Overflow, input too big
		}
		factor *= radix;
	}
	return 0;
}

/* Command parse tree */

prv parse_command(char *iitr, char *citr, pgn *node, struct assembler_state *state)
{
	pgn *noditr = node->children;
	while(noditr - node->children < node->children_count)
	{
		if((noditr->match == *iitr && !(noditr->match & 0x40 && (noditr->match & 0x1F) < 27)) || ((noditr->match & 0x40 && (noditr->match & 0x1F) < 27) && (*iitr == noditr->match || *iitr == (noditr->match ^ 0x20))))
		{
			prv retval = noditr->match_func(iitr+1, citr, noditr, state);
			return (prv){retval.error, retval.adv + 1};
		}
		noditr++;
	}
	return (prv){-1, 0};
}

prv parse_command_ascii(char *iitr, char *citr, pgn *node, struct assembler_state *state) // Half assed, yeah I know.
{
	//DEBUG_DELIM fprintf(stdout, COL_GR "#ASCII command parsed\n" RESET);
	state->mode ^= 0x1;

	char *temp = iitr;
	while((*iitr >= '0' && *iitr < '0' + 10) || ((*iitr & 0x40) && ((*iitr & 0x1F) < 27)))
	{
		iitr++;
	}
	return (prv){0, iitr-temp};
}

prv parse_command_assemble(char *iitr, char *citr, pgn *node, struct assembler_state *state)
{
	//DEBUG_DELIM fprintf(stdout, COL_GR "#ASSEMBLE command parsed\n" RESET);
	full_assemble(state);

	char *temp = iitr;
	while((*iitr >= '0' && *iitr < '0' + 10) || ((*iitr & 0x40) && ((*iitr & 0x1F) < 27)))
	{
		iitr++;
	}
	return (prv){0, iitr-temp};
}

prv parse_command_exit(char *iitr, char *citr, pgn *node, struct assembler_state *state)
{
	//DEBUG_DELIM fprintf(stdout, COL_GR "#EXIT command parsed\n" RESET);
	if(state->mode & 0x2)
	{
		full_assemble(state);
	}
	exit(0);
	return (prv){42, 42};
}

prv parse_command_multiline(char *iitr, char *citr, pgn *node, struct assembler_state *state)
{
	//DEBUG_DELIM fprintf(stdout, COL_GR "#MULTILINE command parsed\n" RESET);
	state->mode ^= 0x2;

	char *temp = iitr;
	while((*iitr >= '0' && *iitr < '0' + 10) || ((*iitr & 0x40) && ((*iitr & 0x1F) < 27)))
	{
		iitr++;
	}
	return (prv){0, iitr-temp};
}

pgn *init_tree()
{
	#define CHLD_TOTAL 7
	pgn *stat_tree = malloc(CHLD_TOTAL * sizeof(pgn));
	if(!stat_tree)
	{
		fprintf(stderr, malloc_fail_fmt, "Command Tree", CHLD_TOTAL * sizeof(pgn));
		exit(0x4);
	}
	#define CHLD_R 3
	stat_tree[0] = (pgn){'#', &parse_command, CHLD_R, &stat_tree[1]};
	#define CHLD_R_0 1
	stat_tree[1 + 0] = (pgn){'A', &parse_command, CHLD_R_0, &stat_tree[1 + CHLD_R]};
	#define CHLD_R_0_0 2
	stat_tree[1 + CHLD_R + 0] = (pgn){'S', &parse_command, CHLD_R_0_0, &stat_tree[1 + CHLD_R + CHLD_R_0]};
	stat_tree[1 + CHLD_R + CHLD_R_0 + 0] = (pgn){'C', &parse_command_ascii, 0, NULL};
	stat_tree[1 + CHLD_R + CHLD_R_0 + 1] = (pgn){'S', &parse_command_assemble, 0, NULL};
	stat_tree[1 + 1] = (pgn){'E', &parse_command_exit, 0, NULL};
	stat_tree[1 + 2] = (pgn){'M', &parse_command_multiline, 0, NULL};
	return stat_tree;
}

int main(size_t nargs, char **args)
{
	pgn *stat_tree = init_tree();

	struct assembler_state state;
	state.ofile = stdout;
	state.ifile = stdin;
	state.tempfile = NULL;

	state.tempfile_n = def_tempfile_n;
	state.ofile_n = NULL;
	state.ifile_n = NULL;
	state.nasm_path = def_nasm_path;
	state.prompt = def_prompt;

	/*
		0x1: Alternate output specified
		0x2: Alternate input specified
		0x4: Assembled this pass
		0x8: Next argument read
	*/
	state.mgmt_flags = 0x0;
	/*
		0x1: ASCII mode
		0x2: Multiline mode
	*/
	state.mode = 0x0;
	state.arch = 0x40;

	state.ib_len = IBL;
	state.cb_len = CBL;

	state.ibuffer = NULL;
	state.cbuffer = NULL;

	size_t argcnt = nargs;
	argcnt--;

	while(argcnt)
	{
		if(state.mgmt_flags & 0x8)
		{
			state.mgmt_flags &= ~0x8;
			argcnt--;
			continue;
		}
		if(args[nargs-argcnt][0] == '-')	// Flag checking
		{
			size_t itr = 1;
			while(args[nargs-argcnt][itr])
			{
				switch(args[nargs-argcnt][itr])
				{
					case '-':	// Ignore further hyphens
						break;
					case 'a':	// Output ASCII characters instead of raw bytecode
						state.mode |= 0x1;
						break;

					case 'c':	// Change input buffer length
						if(state.mgmt_flags & 0x8)
						{
							goto next_arg_read;
						}
						if(argcnt > 1)
						{
							state.mgmt_flags |= 0x8;
							int retval = size_t_parse_str(args[1+nargs-argcnt], &(state.cb_len));
							if(retval)
							{
								fprintf(stderr, inv_int_fmt, inv_int_reasons[-1-retval], CBL);
								state.cb_len = CBL;
							}
							break;
						}
						fprintf(stderr, insuf_arg_fmt_lu, "Command buffer length", "-c", CBL);
						state.mgmt_flags &= (~0x8);
						state.cb_len = CBL;
						break;

					case 'd':
						state.arch = 32;
						break;

					case 'h':	// Print help
						fprintf(stdout, help_fmt, VER_MAJ, VER_MIN, CBL, IBL, def_prompt);
						exit(0);

					case 'i':	// Specify input file/stream
						if(state.mgmt_flags & 0x8)
						{
							goto next_arg_read;
						}
						if(argcnt > 1)
						{
							state.ifile_n = args[1+nargs-argcnt];
							state.mgmt_flags |= 0xA;
							break;
						}
						fprintf(stderr, insuf_arg_fmt_s, "Input file", "-i", "stdin");
						state.mgmt_flags &= (~0xA);
						state.ifile_n = NULL;
						break;

					case 'l':	// Change input buffer length
						if(state.mgmt_flags & 0x8)
						{
							goto next_arg_read;
						}
						if(argcnt > 1)
						{
							state.mgmt_flags |= 0x8;
							int retval = size_t_parse_str(args[1+nargs-argcnt], &(state.ib_len));
							if(retval)
							{
								fprintf(stderr, inv_int_fmt, inv_int_reasons[-1-retval], IBL);
								state.ib_len = IBL;
							}
							break;
						}
						fprintf(stderr, insuf_arg_fmt_lu, "Input buffer length", "-l", IBL);
						state.mgmt_flags &= (~0x8);
						state.ib_len = IBL;
						break;

					case 'm':	// multiline mode
						state.mode |= 0x2;
						break;

					case 'p':	// specify path to assembler
						if(state.mgmt_flags & 0x8)
						{
							goto next_arg_read;
						}
						if(argcnt > 1)
						{
							state.nasm_path = args[1+nargs-argcnt];
							state.mgmt_flags |= 0x8;
							break;
						}
						fprintf(stderr, insuf_arg_fmt_s, "Assembler path", "-p", def_nasm_path);
						state.mgmt_flags &= (~0x8);
						state.nasm_path = def_nasm_path;
						break;

					case 'P':	// specify prompt
						if(state.mgmt_flags & 0x8)
						{
							goto next_arg_read;
						}
						if(argcnt > 1)
						{
							state.prompt = args[1+nargs-argcnt];
							state.mgmt_flags |= 0x8;
							break;
						}
						fprintf(stderr, insuf_arg_fmt_s, "Assembler path", "-P", def_prompt);
						state.mgmt_flags &= (~0x8);
						state.prompt = def_prompt;
						break;

					case 'q':
						state.arch = 64;
						break;

					case 't':	// Specify temp file
						if(state.mgmt_flags & 0x8)
						{
							goto next_arg_read;
						}
						if(argcnt > 1)
						{
							state.tempfile_n = args[1+nargs-argcnt];
							state.mgmt_flags |= 0x8;
							break;
						}
						fprintf(stderr, insuf_arg_fmt_s, "Input buffer length", "-t", def_tempfile_n);
						state.mgmt_flags &= (~0x8);
						state.tempfile_n = def_tempfile_n;
						break;

					case 'w':
						state.arch = 16;
						break;

					default:
						fprintf(stderr, "Invalid flag supplied :%c\nUse lasm -h to print the help\n", args[nargs-argcnt][itr]);
						exit(0x1);
						next_arg_read:
							fprintf(stderr, WARN_MSG "Already have used a file specifier -c, -i, -l, -p, -P or -t in the current options chain. Ignoring...\n");

				}
				itr++;
			}
		}
		else
		{
			state.mgmt_flags |= 0x1;
			state.ofile_n = args[nargs-argcnt];
		}
		argcnt--;
	}

	if(state.mgmt_flags & 0x1)
	{
		state.ofile = fopen(state.ofile_n, "wb");
		if(!(state.ofile))
		{
			fprintf(stderr, FATAL_ERR_MSG "Specified output file is not accessible\n");
			exit(0x2);
		}
		//----------
		//DEBUG_DELIM fprintf(stdout, "Output file name: %s\n", state.ofile_n);
		//----------
	}

	if(state.mgmt_flags & 0x2)
	{
		state.ifile = fopen(state.ifile_n, "rb");
		if(!(state.ifile))
		{
			fprintf(stderr, FATAL_ERR_MSG "Specified input file could not be opened\n");
			exit(0x2);
		}
		//----------
		//DEBUG_DELIM fprintf(stdout, "Input file name: %s\n", state.ifile_n);
		//----------
	}

	state.tempfile_n_len = strlen(state.tempfile_n);

	state.altfile_n_buffer = malloc(state.tempfile_n_len + 3);
	if(!(state.altfile_n_buffer))
	{
		fprintf(stderr, malloc_fail_fmt, "Altfile Name Buffer", state.tempfile_n_len + 3);
		exit(0x4);
	}
	for(size_t i=0; i<state.tempfile_n_len; i++)
	{
		state.altfile_n_buffer[i] = state.tempfile_n[i];
	}
	state.altfile_n_buffer[state.tempfile_n_len - 1] = '.';
	state.altfile_n_buffer[state.tempfile_n_len] = 'o';
	state.altfile_n_buffer[state.tempfile_n_len + 1] = '\0';

	state.ibuffer = (char *)malloc(state.ib_len);
	if(!(state.ibuffer))
	{
		fprintf(stderr, malloc_fail_fmt, "Input Buffer", state.ib_len);
		exit(0x4);
	}

	state.cbuffer = (char *)malloc(state.cb_len);
	if(!(state.cbuffer))
	{
		fprintf(stderr, malloc_fail_fmt, "Command Buffer", state.cb_len);
		exit(0x4);
	}

	//----------
	//DEBUG_DELIM fprintf(stdout, "Temp file name: %s\n", state.tempfile_n);
	//DEBUG_DELIM fprintf(stdout, "mode: %x, mgmt_flags: %x, ib_len: %lu, cb_len: %lu, ibuffer: %p, cbuffer: %p, altfile_n_buffer: %s\n", state.mode, state.mgmt_flags, state.ib_len, state.cb_len, state.ibuffer, state.cbuffer, state.altfile_n_buffer);
	//----------

	while(1)
	{
		if(state.tempfile == NULL)
		{
			state.tempfile = fopen(state.tempfile_n, "wb");
			if(!(state.tempfile))
			{
				fprintf(stderr, FATAL_ERR_MSG "Tempfile is not accessible\n");
				exit(0x2);
			}
		}
		fprintf(stdout, "\n%s", state.prompt);
		fill_ibuffer(state.ibuffer, state.ib_len, state.ifile);

		char *iitr = state.ibuffer;
		char *citr = state.cbuffer;

		/*
		0x1 : Parsing lasm commands
		*/
		unsigned int parse_flags = 0x0;
		while(iitr - state.ibuffer < state.ib_len && *iitr)
		{
			if(!(parse_flags & 0x1))
			{
				if(*iitr == '#')
				{
					prv adv = parse_command(iitr+1, citr, stat_tree, &state);
					//DEBUG_DELIM fprintf(stdout, "mode: %x, mgmt_flags: %x, ib_len: %lu, cb_len: %lu, ibuffer: %p, cbuffer: %p\n", state.mode, state.mgmt_flags, state.ib_len, state.cb_len, state.ibuffer, state.cbuffer);
					if(adv.error == 1)
					{
						iitr++;
						fprintf(stdout, WARN_MSG "Unrecogonized command.\nSupported commands are #ASCII, #ASSEMBLE(only in multiline mode), #EXIT and #MULTILINE\n");
					}
					state.mgmt_flags |= 0x4; 
					iitr += adv.adv;
					goto processed_command;
				}
				*(citr++) = *(iitr++);
				if(citr-state.cbuffer == state.cb_len-1)
				{
					fwrite(state.cbuffer, citr-state.cbuffer, 1, state.tempfile);
					citr = state.cbuffer;
				}
			}
		}
		*(citr++) = '\n';	// Break different lines, only really useful in multiline mode

		processed_command:	// If we have just processed a command, we can skip appending a newline.
		fwrite(state.cbuffer, citr-state.cbuffer, 1, state.tempfile);

		if(!(state.mode & 0x2))
		{
			full_assemble(&state);
		}
		if(state.mgmt_flags & 0x4)
		{
			state.mgmt_flags &= (~0x4);
		}
	}

	exit(0);
}