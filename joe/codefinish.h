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

/* Code completion for the buffer being edited.
 *  buffer:   entire contents of the buffer, zero-terminated (it may also
 *            contain NUL bytes, so use size for its length)
 *  size:     length of buffer in bytes
 *  filename: name of the file being edited, or NULL if the buffer has none
 *  cursor:   position of the cursor in the buffer
 *
 * Returns text to insert at the cursor in a joe_malloc() block (freed by
 * the caller), or NULL to leave the buffer unchanged.
 */
char *codeFinish(const char *buffer, ptrdiff_t size, const char *filename, const struct codefinish_cursor *cursor);

/* codefinish command: run codeFinish() on the current buffer */
int ucodefinish(W *w, int k);
