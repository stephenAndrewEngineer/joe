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

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

/*
 * Helpers for looking at the line
 */

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

/* Characters that make up a (dotted) module name.  Bytes >= 0x80 are
 * allowed because Python identifiers may contain non-ASCII letters. */
static int is_name_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '_' || c == '.' || (c & 0x80);
}

static int is_blank(char c)
{
	return c == ' ' || c == '\t';
}

/* Is the text [s, end) a keyword followed by a space or a tab?  Returns a
 * pointer just past the keyword, or NULL if not.
 */
static const char *keyword(const char *s, const char *end, const char *word)
{
	ptrdiff_t len = zlen(word);
	if (starts_with(s, end, word) && end - s > len && is_blank(s[len]))
		return s + len;
	return NULL;
}

/*
 * Finding importable Python modules
 *
 * The directories searched are the ones Python's importer searches: its
 * sys.path, which includes $PYTHONPATH, the standard library and the
 * site-packages directories.  Python adds the directory of the script it
 * runs at the front of sys.path, so the edited file's directory is
 * searched first.
 *
 * Only directory listings are read; no file is opened.  A directory entry
 * is importable if it is
 *   - a file ending in one of Python's import suffixes (.py, .pyc and the
 *     extension module suffixes such as .cpython-311-x86_64-linux-gnu.so,
 *     .abi3.so and .so), and the part before the suffix is a valid
 *     identifier.  An extension built for another Python version, like
 *     foo.cpython-39-x86_64-linux-gnu.so, leaves "foo.cpython-39-..." before
 *     ".so", which is not an identifier, so it is skipped as Python would.
 *   - a directory whose name is a valid identifier: a package, or a
 *     namespace package if it has no __init__.py.
 * __init__ and __pycache__ are never offered.  Other names starting with
 * "_" (private modules, and built-ins like _io) are left out unless the
 * name being typed starts with "_" too: then only those names are offered.
 * Modules compiled into the interpreter, such as sys, have no file; their
 * names come from sys.builtin_module_names.  Zip files on sys.path are not
 * searched, since that would mean reading them.
 */

/* What Python told us, fetched once per session */
static struct {
	int	loaded;
	char	**dirs;		/* sys.path, with "" (the script's directory) left out */
	char	**builtins;	/* sys.builtin_module_names */
	char	**suffixes;	/* importlib.machinery.all_suffixes() */
} python;

/* Ask python3 for its search path, built-in modules and import suffixes.
 * Each output line is tagged: P = path entry, B = built-in, S = suffix.
 * stdin and stderr are redirected so python3 never touches the terminal.
 * Without python3, fall back to $PYTHONPATH and the usual suffixes.
 */
static void python_load(void)
{
	FILE *f;
	char line[4096];

	if (python.loaded)
		return;
	python.loaded = 1;

	f = joe_popen("python3 -c 'import sys, importlib.machinery as m; "
	              "print(\"\\n\".join([\"P\" + p for p in sys.path] + "
	              "[\"B\" + b for b in sys.builtin_module_names] + "
	              "[\"S\" + s for s in m.all_suffixes()]))' </dev/null 2>/dev/null", 0);
	if (f) {
		while (fgets(line, SIZEOF(line), f)) {
			ptrdiff_t len = zlen(line);
			if (len && line[len - 1] == '\n')
				line[--len] = 0;
			if (line[0] == 'P' && line[1])
				python.dirs = vaadd(python.dirs, vsncpy(NULL, 0, sz(line + 1)));
			else if (line[0] == 'B' && line[1])
				python.builtins = vaadd(python.builtins, vsncpy(NULL, 0, sz(line + 1)));
			else if (line[0] == 'S' && line[1])
				python.suffixes = vaadd(python.suffixes, vsncpy(NULL, 0, sz(line + 1)));
		}
		joe_pclose(f);
	}

	if (!python.suffixes) {
		/* No python3 (or it failed): search $PYTHONPATH only */
		const char *pythonpath = getenv("PYTHONPATH");
		if (pythonpath)
			python.dirs = vawords(NULL, sz(pythonpath), sc(":"));
		python.suffixes = vaadd(python.suffixes, vsncpy(NULL, 0, sc(".py")));
		python.suffixes = vaadd(python.suffixes, vsncpy(NULL, 0, sc(".pyc")));
		python.suffixes = vaadd(python.suffixes, vsncpy(NULL, 0, sc(".so")));
	}
}

/* Is [s, s + len) a Python identifier? */
static int is_identifier(const char *s, ptrdiff_t len)
{
	ptrdiff_t x;
	if (!len || (s[0] >= '0' && s[0] <= '9'))
		return 0;
	for (x = 0; x != len; ++x)
		if (s[x] == '.' || !is_name_char(s[x]))
			return 0;
	return 1;
}

/* Name of the module that the directory entry name provides, or a length
 * of 0 if it is not importable.  Returns the length of the module name.
 */
static ptrdiff_t module_name(const char *dir, const char *name)
{
	ptrdiff_t len = zlen(name);
	ptrdiff_t best = 0;
	ptrdiff_t x;
	struct stat st;
	char *path;
	int is_dir;

	/* Use the longest suffix that matches, so foo.abi3.so is "foo" and
	 * not "foo.abi3" */
	for (x = 0; python.suffixes[x]; ++x) {
		ptrdiff_t slen = sLEN(python.suffixes[x]);
		if (len > slen && slen > best && !zcmp(name + len - slen, python.suffixes[x]))
			best = slen;
	}
	if (best) {
		/* A package's __init__ is importable but never worth offering */
		if (len - best == 8 && !strncmp(name, "__init__", 8))
			return 0;
		return is_identifier(name, len - best) ? len - best : 0;
	}

	/* A package directory */
	if (!is_identifier(name, len) || !zcmp(name, "__pycache__"))
		return 0;
	path = vsncpy(NULL, 0, sz(dir));
	path = vsadd(path, '/');
	path = vsncpy(sv(path), sz(name));
	is_dir = !stat(path, &st) && S_ISDIR(st.st_mode);
	vsrm(path);
	return is_dir ? len : 0;
}

/* Should name be offered when leaf has been typed?  It must start with
 * leaf, and private names (starting with "_") are only offered when leaf
 * starts with "_" as well.
 */
static int wanted(const char *name, const char *leaf, ptrdiff_t leaf_len)
{
	if (name[0] == '_' && !(leaf_len && leaf[0] == '_'))
		return 0;
	return !strncmp(name, leaf, (size_t)leaf_len);
}

/* Add the importable names in directory dir that start with leaf to
 * options, each preceded by outprefix.
 */
static char **python_list(char **options, const char *dir, const char *leaf, ptrdiff_t leaf_len, const char *outprefix)
{
#ifdef HAVE_DIRENT_H
	DIR *d = opendir(dir);
	struct dirent *de;

	if (!d)
		return options;
	while ((de = readdir(d)) != NULL) {
		ptrdiff_t len;
		/* Skip hidden entries and, unless asked for, private names */
		if (de->d_name[0] == '.' || !wanted(de->d_name, leaf, leaf_len))
			continue;
		len = module_name(dir, de->d_name);
		if (len)
			options = vaadd(options, vsncpy(vsncpy(NULL, 0, sz(outprefix)), sLEN(outprefix), de->d_name, len));
	}
	closedir(d);
#endif
	return options;
}

/* Add the importable modules whose dotted name is pkg + "." + leaf* (or
 * just leaf* when pkg is empty) to options, each preceded by outprefix.
 * filename is the file being edited; its directory is searched first.
 */
static char **python_modules(char **options, const char *filename,
                             const char *pkg, ptrdiff_t pkg_len,
                             const char *leaf, ptrdiff_t leaf_len, const char *outprefix)
{
	char *subdir = vsncpy(NULL, 0, NULL, 0);	/* pkg as a relative path */
	char *script_dir;
	const char *slash;
	ptrdiff_t x;

	python_load();

	for (x = 0; x != pkg_len; ++x)
		subdir = vsadd(subdir, pkg[x] == '.' ? '/' : pkg[x]);

	/* The directory of the file being edited, standing in for the "" that
	 * python -c puts at the front of sys.path */
	slash = filename ? strrchr(filename, '/') : NULL;
	if (slash)
		script_dir = vsncpy(NULL, 0, filename, slash - filename + (slash == filename));
	else
		script_dir = vsncpy(NULL, 0, sc("."));

	for (x = -1; x == -1 || (python.dirs && python.dirs[x]); ++x) {
		const char *root = x == -1 ? script_dir : python.dirs[x];
		char *dir = vsncpy(NULL, 0, sz(root));
		if (pkg_len) {
			dir = vsadd(dir, '/');
			dir = vsncpy(sv(dir), sv(subdir));
		}
		options = python_list(options, dir, leaf, leaf_len, outprefix);
		vsrm(dir);
	}

	/* Modules built into the interpreter are top-level only */
	if (!pkg_len && python.builtins)
		for (x = 0; python.builtins[x]; ++x)
			if (wanted(python.builtins[x], leaf, leaf_len))
				options = vaadd(options, vsncpy(NULL, 0, sv(python.builtins[x])));

	vsrm(script_dir);
	vsrm(subdir);
	return options;
}

/*
 * The completion rules
 */

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
 *      the dotted module name at the cursor completes to the importable
 *      modules found as described above:
 *        import nu        ->  numpy, numbers, ...
 *        import os.pa     ->  submodules of os starting with "pa"
 *        from xml.d       ->  xml.dom
 *        from xml import  ->  submodules of xml (dom, etc, parsers, sax)
 *      Only names starting with what has been typed are offered.
 *      Relative imports (names starting with ".") are not completed.
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
	for (indent = bol; indent != eol && is_blank(*indent); ++indent)
		;
	if ((s = keyword(indent, eol, "import")) != NULL || (s = keyword(indent, eol, "from")) != NULL) {
		const char *word = here;	/* Start of the name being typed */
		const char *word_end = here;	/* End of it, if the cursor is inside it */
		const char *base = NULL;	/* "from <base> import ...": package to look in */
		ptrdiff_t base_len = 0;
		const char *dot;		/* Last '.' typed in word */
		char *pkg;			/* Package to look in: base + word up to its last '.' */
		char *outprefix;		/* What goes before each module name */

		while (word != bol && is_name_char(word[-1]))
			--word;
		while (word_end != eol && is_name_char(*word_end))
			++word_end;

		/* The cursor has to be past the keyword and the space after it */
		if (word <= s)
			return;

		/* "from <base> import <word>": complete submodules of base */
		if (s == indent + 4) {
			const char *t = s;
			while (t != eol && is_blank(*t))
				++t;
			base = t;
			while (t != eol && is_name_char(*t))
				++t;
			base_len = t - base;
			while (t != eol && is_blank(*t))
				++t;
			if (base_len && keyword(t, eol, "import") && word > t + 6)
				;	/* Cursor is in the import list */
			else
				base = NULL, base_len = 0;
		}

		/* Relative imports are not supported */
		if (*word == '.' || (base && *base == '.'))
			return;

		/* Split word into the package part and the name being completed */
		for (dot = here; dot != word && dot[-1] != '.'; --dot)
			;
		pkg = vsncpy(NULL, 0, base, base_len);
		if (base_len && dot != word)
			pkg = vsadd(pkg, '.');
		if (dot != word)
			pkg = vsncpy(sv(pkg), word, dot - 1 - word);
		outprefix = vsncpy(NULL, 0, word, dot - word);

		result->start = word - buffer;
		result->end = word_end - buffer;
		result->options = python_modules(result->options, filename, sv(pkg), dot, here - dot, outprefix);
		if (result->options) {
			vasort(result->options, aLEN(result->options));
			vauniq(result->options);
		}

		vsrm(outprefix);
		vsrm(pkg);
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
