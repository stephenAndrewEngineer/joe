/*
 *	Code completion
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"

void codeFinish(const char *buffer, ptrdiff_t size, const char *filename)
{
}

int ucodefinish(W *w, int k)
{
	BW *bw;
	P *p;
	ptrdiff_t size;
	char *buffer;
	WIND_BW(bw, w);

	size = (ptrdiff_t)(bw->b->eof->byte - bw->b->bof->byte);
	p = pdup(bw->b->bof, "ucodefinish");
	buffer = brs(p, size);
	prm(p);

	codeFinish(buffer, size, bw->b->name);

	joe_free(buffer);
	return 0;
}
