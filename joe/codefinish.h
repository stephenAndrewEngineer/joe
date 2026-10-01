/*
 *	Code completion
 *
 *	This file is part of JOE (Joe's Own Editor)
 */

/* Where the cursor is when code completion is requested */
struct codefinish_cursor {
	off_t	byte;	/* Byte offset of the cursor from the start of the buffer */
	off_t	line;	/* Line number, starting at 1 (the "Row" in the status line) */
	off_t	col;	/* Screen column, starting at 1 (the "Col" in the status line);
			   tabs count as the columns they occupy */
	int	at_eof;	/* Non-zero if the cursor is at the end of the file */
};

/* What codeFinish() proposes: replace the bytes [start, end) of the buffer
 * with one of the options.
 */
struct codefinish_result {
	off_t	start;		/* First byte to replace */
	off_t	end;		/* One past the last byte to replace */
	char	**options;	/* Candidate texts: a va array of vs strings
				   (build with vaadd(options, vsncpy(NULL, 0, sz(text)))),
				   or NULL for no completion */
};

/* Code completion for the buffer being edited.
 *  buffer:   entire contents of the buffer, zero-terminated (it may also
 *            contain NUL bytes, so use size for its length)
 *  size:     length of buffer in bytes
 *  filename: name of the file being edited, or NULL if the buffer has none
 *  cursor:   position of the cursor in the buffer
 *  result:   filled in with the completion; options is NULL on entry
 *
 * With no options nothing happens (JOE beeps), with one option it is
 * inserted right away, and with several a menu lets the user pick one.
 */
void codeFinish(const char *buffer, ptrdiff_t size, const char *filename,
                const struct codefinish_cursor *cursor, struct codefinish_result *result);

/* codefinish command: run codeFinish() on the current buffer */
int ucodefinish(W *w, int k);
