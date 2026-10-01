/*
 *	Code completion
 *
 *	This file is part of JOE (Joe's Own Editor)
 */

/* Code completion for the buffer being edited.
 *  buffer:   entire contents of the buffer, zero-terminated (it may also
 *            contain NUL bytes, so use size for its length)
 *  size:     length of buffer in bytes
 *  filename: name of the file being edited, or NULL if the buffer has none
 */
void codeFinish(const char *buffer, ptrdiff_t size, const char *filename);

/* codefinish command: run codeFinish() on the current buffer */
int ucodefinish(W *w, int k);
