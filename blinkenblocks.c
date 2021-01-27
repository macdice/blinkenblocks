/*
 * Silly hack to visualise I/O concurrency, just for fun.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IOS 1024

enum mode {
	/* TODO: support some kind of dtrace io output too */
	MODE_BTRACE
};

/* Some kind of identifying tag for an I/O in progress. */
typedef struct io_id {
	char program[16];
	char tag[64];
} io_id;

typedef struct io_state {
	io_id ids[MAX_IOS];
    size_t size;
	bool unicode;
} io_state;

static void
display_submit(io_state *state, io_id *io, char rw, size_t blocks)
{
	/* Find a suitable empty column. */
	for (size_t i = 0; i < MAX_IOS; ++i) {
		if (state->ids[i].tag[0] == 0) {
			/* Found one. */
			state->ids[i] = *io;
			if (state->size <= i)
				state->size = i + 1;
			printf("%16s %c", io->program, rw);
			fputs(state->unicode ? "─" : "-", stdout);
			for (size_t j = 0; j < state->size; ++j) {
				if (j <  i) {
					if (state->ids[j].tag[0] == 0)
						fputs(state->unicode ? "─" : "-", stdout);
					else
						fputs(state->unicode ? "┼" : "+", stdout);
				} else if (j == i) {
					fputs(state->unicode ? "┐" : "+", stdout);
				} else {
					if (state->ids[j].tag[0] == 0)
						putchar(' ');
					else
						fputs(state->unicode ? "│" : "|", stdout);
				}
			}
			putchar('\n');
			return;
		}
	}
	/* If we got here, there are too many I/Os for us to track. */
	printf("too many IOs\n");
	exit(1);
}

static void
display_complete(io_state *state, io_id *io, char rw, size_t blocks)
{
	/* Find this tag.  Yeah, this should be a hash table. */
	for (size_t i = 0; i < state->size; ++i) {
		if (strcmp(state->ids[i].tag, io->tag) == 0) {
			/* Found it. */
			state->ids[i].tag[0] = 0;
			printf("%16s %s", state->ids[i].program,
				   state->unicode ? "◀" : "<");
			fputs(state->unicode ? "─" : "-", stdout);
			for (size_t j = 0; j < state->size; ++j) {
				if (j <  i) {
					if (state->ids[j].tag[0] == 0)
						fputs(state->unicode ? "─" : "-", stdout);
					else
						fputs(state->unicode ? "┼" : "+", stdout);
				} else if (j == i) {
					fputs(state->unicode ? "┘" : "+", stdout);
				} else {
					if (state->ids[j].tag[0] == 0)
						putchar(' ');
					else
						fputs(state->unicode ? "│" : "|", stdout);
				}
			}
			putchar('\n');
			while (state->size > 0 && state->ids[state->size - 1].tag[0] == 0)
				--state->size;
			return;
		}
	}
	/* Ignore the ones we can't find. (?) */
}

/* Parse a line output by btrace. */
static void
parse_btrace_line(io_state *state, char *line)
{
	size_t len = strlen(line);
	int field = 0;
	char rw = 0;
	char command = 0;
	size_t i = 0;
	size_t blocks = 1;
	io_id io;

	/* Yeah, this should be a regex. */
	while (i < len) {
		if (field == 5) {
			/* Action field */
			if (line[i] == 'D' || line[i] == 'C')
				command = line[i];
			else
				return;
		} else if (field == 6) {
			/* Flags field; look for R or W. */
			rw = 0;
			for (size_t j = i; line[j] != ' ' && line[j] != '\0'; ++j) {
				if (line[j] == 'R' || line[j] == 'W')
					rw = line[j];
			}
			/* Ignore non-RW for now. */
			if (rw == 0)
				return;
		} else if (field == 7) {
			size_t s = 0;
			size_t spaces = 0;
			size_t j = i;

			/* Read fields 7, optionally followed by + N into id.tag. */
			while (j < len && s < sizeof(io.tag) - 1) {
				if (line[j] == ' ') {
					++spaces;
					if (spaces == 1 && line[j + 1] != '+')
						break;
					if (spaces == 2)
						blocks = atoi(&line[j]);
					if (spaces == 3)
						break;
				}
				io.tag[s++] = line[j++];
			}
			io.tag[s] = 0;

		} else if ((field == 8 || field == 10) && line[i] == '[') {
			size_t j = i + 1;
			size_t s = 0;

			while (j < len && line[j] != ']' && s < sizeof(io.program) - 1)
				io.program[s++] = line[j++];
			io.program[s] = 0;

			if (command == 'D')
				display_submit(state, &io, rw, blocks);
			else if (command == 'C')
				display_complete(state,  &io, rw, blocks);
		}
		/* Step over field. */
		while (line[i] != ' ' && line[i] != '\0')
			++i;
		++field;
		/* Step over delimiter. */
		while (line[i] == ' ')
			++i;
	}
}

int
main(int argc, char *argv[])
{
	io_state state;
	char line[1024];
	enum mode mode = MODE_BTRACE;

	memset(&state, 0, sizeof(state));
	state.unicode = true;

	while (fgets(line, sizeof(line), stdin)) {
		if (mode == MODE_BTRACE) {
			parse_btrace_line(&state, line);
		}
	}
}
