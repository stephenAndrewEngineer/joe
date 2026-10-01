/*
 *	Code completion
 *
 *	This file is part of JOE (Joe's Own Editor)
 *
 * The codefinish command (bound to Shift-Tab in the rc files) hands the
 * whole buffer being edited, its file name and the cursor position to
 * codeFinish().  Whatever text codeFinish() returns is inserted at the
 * cursor, and the cursor is left after it.
 *
 * codeFinish() is where the completion logic goes.  It only sees plain C
 * data, so it does not need to know about JOE's buffer (B), pointer (P)
 * or window (BW) structures; ucodefinish() below does that translation.
 */
#include "types.h"

/* Compute a completion for the text at the cursor.
 *
 * buffer is a private copy of the buffer, including any unsaved edits, so
 * it can be read freely but changes to it have no effect on the file.
 * cursor->byte indexes into it: buffer[cursor->byte] is the character
 * under the cursor and the text before the cursor is
 * buffer[0 .. cursor->byte - 1].
 *
 * The returned string must come from joe_malloc() (zdup() and friends do
 * this); ucodefinish() inserts it and then frees it.  Return NULL to
 * insert nothing.
 */
char *codeFinish(const char *buffer, ptrdiff_t size, const char *filename, const struct codefinish_cursor *cursor)
{
	/* Example: ignore the context and always insert the same text */
	return zdup("I am completing code");
}

/* The codefinish command.  Gathers what codeFinish() needs from the
 * current window, calls it, and inserts the result at the cursor.
 */
int ucodefinish(W *w, int k)
{
	BW *bw;
	P *p;
	ptrdiff_t size;
	char *buffer;
	char *text;
	struct codefinish_cursor cursor;
	WIND_BW(bw, w);

	/* Copy the entire buffer into a zero-terminated string */
	size = (ptrdiff_t)(bw->b->eof->byte - bw->b->bof->byte);
	p = pdup(bw->b->bof, "ucodefinish");
	buffer = brs(p, size);
	prm(p);

	/* JOE counts lines and columns from 0; report them from 1 like the
	 * status line does */
	cursor.byte = bw->cursor->byte;
	cursor.line = bw->cursor->line + 1;
	cursor.col = bw->cursor->col + 1;
	cursor.at_eof = piseof(bw->cursor);

	text = codeFinish(buffer, size, bw->b->name, &cursor);
	joe_free(buffer);

	if (text) {
		ptrdiff_t len = zlen(text);

		/* Insert at the cursor and move the cursor past the new text.
		 * This goes through the normal buffer routines, so it can be
		 * undone and the window is redrawn after the command. */
		binsm(bw->cursor, text, len);
		pfwrd(bw->cursor, len);
		bw->cursor->xcol = piscol(bw->cursor);
		joe_free(text);
	}

	return 0;
}
