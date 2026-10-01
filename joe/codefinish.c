/*
 *	Code completion
 *
 *	This file is part of JOE (Joe's Own Editor)
 *
 * The codefinish command (bound to Shift-Tab in the rc files) hands the
 * whole buffer being edited, its file name and the cursor position to
 * codeFinish().  codeFinish() answers with a range of the buffer to replace
 * and a list of candidate texts for it:
 *
 *   - no candidates:  nothing changes and JOE beeps
 *   - one candidate:  it replaces the range right away
 *   - several:        a menu (the same kind Esc Tab word completion uses)
 *                     opens below the window; Enter picks the highlighted
 *                     candidate, ^C closes the menu without changing
 *                     anything
 *
 * codeFinish() only sees plain C data, so the completion rules do not need
 * to know about JOE's buffer (B), pointer (P) or window (BW) structures;
 * ucodefinish() and the helpers below do that translation.
 */
#include "types.h"

/*
 * Completion rules
 */

/* Modules offered after "import" or "from" */
static const char *python_modules[] = { "numpy", "matplotlib", "os", "sys" };

/* Does the file name end in ext? */
static int has_extension(const char *filename, const char *ext)
{
	ptrdiff_t len, ext_len;
	if (!filename)
		return 0;
	len = zlen(filename);
	ext_len = zlen(ext);
	return len >= ext_len && !zcmp(filename + len - ext_len, ext);
}

/* Does the text [s, end) start with word? */
static int starts_with(const char *s, const char *end, const char *word)
{
	ptrdiff_t len = zlen(word);
	return end - s >= len && !strncmp(s, word, (size_t)len);
}

/* Characters that make up a module name */
static int is_name_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '_' || c == '.';
}

/* Is the text [s, end) a keyword followed by a space or a tab?  Returns a
 * pointer just past the keyword, or NULL if not.
 */
static const char *keyword(const char *s, const char *end, const char *word)
{
	ptrdiff_t len = zlen(word);
	if (starts_with(s, end, word) && end - s > len && (s[len] == ' ' || s[len] == '\t'))
		return s + len;
	return NULL;
}

/* Compute the completions for the text at the cursor.
 *
 * buffer is a private copy of the buffer, including any unsaved edits, so
 * it can be read freely but changes to it have no effect on the file.
 * cursor->byte indexes into it: buffer[cursor->byte] is the character
 * under the cursor and the text before the cursor is
 * buffer[0 .. cursor->byte - 1].
 *
 * Rules:
 *   1. In a .py file, a line starting with "#!/" completes to
 *      "#!/usr/bin/env python3" (the whole line is replaced).
 *   2. On a line starting with "import" or "from" (after any indentation),
 *      the module name at the cursor completes to one of numpy,
 *      matplotlib, os or sys.  Only the names starting with what has been
 *      typed so far are offered.
 */
void codeFinish(const char *buffer, ptrdiff_t size, const char *filename,
                const struct codefinish_cursor *cursor, struct codefinish_result *result)
{
	const char *here = buffer + cursor->byte;	/* The cursor */
	const char *bol = here;				/* Beginning of the cursor's line */
	const char *eol = here;				/* End of the cursor's line */
	const char *indent;				/* First non-blank on the line */
	const char *s;

	while (bol != buffer && bol[-1] != '\n')
		--bol;
	while (eol != buffer + size && *eol != '\n')
		++eol;

	/* Rule 1: Python shebang line */
	if (has_extension(filename, ".py") && starts_with(bol, eol, "#!/")) {
		result->start = bol - buffer;
		result->end = eol - buffer;
		result->options = vaadd(result->options, vsncpy(NULL, 0, sc("#!/usr/bin/env python3")));
		return;
	}

	/* Rule 2: module name after "import" or "from" */
	for (indent = bol; indent != eol && (*indent == ' ' || *indent == '\t'); ++indent)
		;
	if ((s = keyword(indent, eol, "import")) != NULL || (s = keyword(indent, eol, "from")) != NULL) {
		const char *word = here;	/* Start of the name being typed */
		const char *word_end = here;	/* End of it, if the cursor is inside it */
		ptrdiff_t x;

		while (word != bol && is_name_char(word[-1]))
			--word;
		while (word_end != eol && is_name_char(*word_end))
			++word_end;

		/* The cursor has to be past the keyword and the space after it */
		if (word <= s)
			return;

		result->start = word - buffer;
		result->end = word_end - buffer;
		for (x = 0; x != SIZEOF(python_modules) / SIZEOF(python_modules[0]); ++x)
			if (!strncmp(python_modules[x], word, (size_t)(here - word)))
				result->options = vaadd(result->options, vsncpy(NULL, 0, sz(python_modules[x])));
		return;
	}
}

/*
 * Applying a completion to the buffer
 */

/* Replace [start, end) of bw's buffer with text and put the cursor after it */
static void codefinish_replace(BW *bw, off_t start, off_t end, const char *text)
{
	ptrdiff_t len = zlen(text);
	P *from = pdup(bw->cursor, "codefinish_replace");
	P *to = pdup(bw->cursor, "codefinish_replace");

	pgoto(from, start);
	pgoto(to, end);
	bdel(from, to);
	binsm(from, text, len);
	prm(to);
	prm(from);

	/* Normal buffer routines are used, so this can be undone and the
	 * window is redrawn after the command */
	pgoto(bw->cursor, start + len);
	bw->cursor->xcol = piscol(bw->cursor);
}

/* The menu's object: what to replace once the user picks an option */
struct codefinish_menu {
	off_t	start;
	off_t	end;
	char	**options;	/* Also the menu's list */
};

/* The menu closed, either after a pick or because the user aborted it */
static int codefinish_menu_abort(W *w, ptrdiff_t x, void *obj)
{
	struct codefinish_menu *cm = (struct codefinish_menu *)obj;
	varm(cm->options);
	joe_free(cm);
	return -1;
}

/* The user picked option x from the menu */
static int codefinish_menu_pick(MENU *m, ptrdiff_t x, void *obj, int k)
{
	struct codefinish_menu *cm = (struct codefinish_menu *)obj;
	BW *bw = (BW *)m->parent->win->object;

	codefinish_replace(bw, cm->start, cm->end, cm->options[x]);
	/* Closing the menu window calls codefinish_menu_abort(), which frees cm */
	wabort(m->parent);
	return 0;
}

/* The codefinish command.  Gathers what codeFinish() needs from the
 * current window, calls it, and applies the result.
 */
int ucodefinish(W *w, int k)
{
	BW *bw;
	P *p;
	ptrdiff_t size;
	char *buffer;
	struct codefinish_cursor cursor;
	struct codefinish_result result;
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

	result.start = result.end = cursor.byte;
	result.options = NULL;

	codeFinish(buffer, size, bw->b->name, &cursor, &result);
	joe_free(buffer);

	if (!result.options || !aLEN(result.options)) {
		/* Nothing to complete */
		varm(result.options);
		ttputc(7);
		return -1;
	}

	if (aLEN(result.options) == 1) {
		codefinish_replace(bw, result.start, result.end, result.options[0]);
		varm(result.options);
		return 0;
	} else {
		struct codefinish_menu *cm = (struct codefinish_menu *)joe_malloc(SIZEOF(struct codefinish_menu));
		cm->start = result.start;
		cm->end = result.end;
		cm->options = result.options;
		if (!mkmenu(bw->parent, bw->parent, cm->options, codefinish_menu_pick, codefinish_menu_abort, NULL, 0, cm, NULL)) {
			varm(cm->options);
			joe_free(cm);
			return -1;
		}
		return 0;
	}
}
