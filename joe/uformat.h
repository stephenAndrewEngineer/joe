/*
 * 	User text formatting functions
 * 	Copyright
 *		(C) 1992 Joseph H. Allen
 * 
 *	This file is part of JOE (Joe's Own Editor)
 */

int ucenter(W *w, int k);
int ubop(W *w, int k);
int ueop(W *w, int k);
void wrapword(BW *bw, P *p, off_t indent, int french, int no_over, char *indents);
int uformat(W *w, int k);
int ufmtblk(W *w, int k);
