/*
 *	Variable length array of strings
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"

/* Moved here from the headers: used only in this file */

/* aELEMENT adup(); */
#define adup(s) vsdup(s)

/* int acmp(); */
#define acmp(a,b) vscmp((a),(b))

/* extern aELEMENT ablank; */
#define ablank NULL

/* int aSIZ(aELEMENT *vary);
 * int aSiz(aELEMENT *vary);
 * Access size part of array.  This int indicates the number of elements which
 * can fit in the array before realloc needs to be called.  It does not include
 * the extra space needed for the terminator and the header.
 *
 * aSIZ returns 0 if you pass it 0.  aSiz does not do this checking,
 * but can be used as an lvalue.
 */
#define aSIZ(a) ((a) ? *((ptrdiff_t *)(a) - 2) : 0)

/* aELEMENT *vazap(aELEMENT *vary, int pos, int n);
 * Destroy n elements from an array beginning at pos.  Is ok if pos/n go
 * past end of array.  This does not change the aLEN() value of the array.
 * This does nothing and returns 0 if 'vary' is 0.  Note that this
 * function does not actually write to the array.  This does not stop if
 * a aterm is encountered.
 */
static aELEMENT *vazap(aELEMENT *vary, ptrdiff_t pos, ptrdiff_t n);

/* aELEMENT *vafill(aELEMENT *vary, int pos, aELEMENT el, int len);
 * Set 'len' element of 'vary' beginning at 'pos' to duplications of 'el'.
 * Ok, if pos/len are past end of array.  If 'vary' is 0, a new array is
 * created.
 *
 * This does not zap previous values.  If you need that to happen, call
 * vazap first.  It does move the terminator around properly though.
 */
static aELEMENT *vafill(aELEMENT *vary, ptrdiff_t pos, aELEMENT el, ptrdiff_t len);

/* Delete elements from an array */
static void vadel(aELEMENT *ary, ptrdiff_t ofset, ptrdiff_t len);

aELEMENT *vamk(ptrdiff_t len)
{
	ptrdiff_t *newa = (ptrdiff_t *) joe_malloc((1 + len) * SIZEOF(aELEMENT) + 2 * SIZEOF(ptrdiff_t));

	newa[0] = len;
	newa[1] = 0;
	((aELEMENT *)(newa + 2))[0] = aterm;
	return (aELEMENT *)(newa + 2);
}

void varm(aELEMENT *vary)
{
	if (vary) {
		vazap(vary, 0, aLen(vary));
		joe_free((ptrdiff_t *) vary - 2);
	}
}

ptrdiff_t alen(aELEMENT *ary)
{
	if (ary) {
		aELEMENT *beg = ary;
		while (acmp(*ary, aterm))
			++ary;
		return ary - beg;
	} else
		return 0;
}

aELEMENT *vaensure(aELEMENT *vary, ptrdiff_t len)
{
	if (!vary)
		vary = vamk(len);
	else if (len > aSiz(vary)) {
		len += (len >> 2);
		vary = (aELEMENT *)(2 + (ptrdiff_t *) joe_realloc((ptrdiff_t *) vary - 2, (len + 1) * SIZEOF(aELEMENT) + 2 * SIZEOF(ptrdiff_t)));

		aSiz(vary) = len;
	}
	return vary;
}

static aELEMENT *vazap(aELEMENT *vary, ptrdiff_t pos, ptrdiff_t n)
{
	if (vary) {
		ptrdiff_t x;

		if (pos < aLen(vary)) {
			if (pos + n <= aLen(vary)) {
				for (x = pos; x != pos + n; ++x)
					adel(vary[x]);
			} else {
				for (x = pos; x != aLen(vary); ++x)
					adel(vary[x]);
			}
		}
	}
	return vary;
}

aELEMENT *vatrunc(aELEMENT *vary, ptrdiff_t len)
{
	if (!vary || len > aLEN(vary))
		vary = vaensure(vary, len);
	if (len < aLen(vary)) {
		vary = vazap(vary, len, aLen(vary) - len);
		vary[len] = vary[aLen(vary)];
		aLen(vary) = len;
	} else if (len > aLen(vary)) {
		vary = vafill(vary, aLen(vary), ablank, len - aLen(vary));
	}
	return vary;
}

static aELEMENT *vafill(aELEMENT *vary, ptrdiff_t pos, aELEMENT el, ptrdiff_t len)
{
	ptrdiff_t olen = aLEN(vary), x;

	if (!vary || pos + len > aSIZ(vary))
		vary = vaensure(vary, pos + len);
	if (pos + len > olen) {
		vary[pos + len] = vary[olen];
		aLen(vary) = pos + len;
	}
	for (x = pos; x != pos + len; ++x)
		vary[x] = adup(el);
	if (pos > olen)
		vary = vafill(vary, pos, ablank, pos - olen);
	return vary;
}

aELEMENT *_vaset(aELEMENT *vary, ptrdiff_t pos, aELEMENT el)
{
	if (!vary || pos + 1 > aSIZ(vary))
		vary = vaensure(vary, pos + 1);
	if (pos > aLen(vary)) {
		vary = vafill(vary, aLen(vary), ablank, pos - aLen(vary));
		vary[pos + 1] = vary[pos];
		vary[pos] = el;
		aLen(vary) = pos + 1;
	} else if (pos == aLen(vary)) {
		vary[pos + 1] = vary[pos];
		vary[pos] = el;
		aLen(vary) = pos + 1;
	} else {
		adel(vary[pos]);
		vary[pos] = el;
	}
	return vary;
}

static int _acmp(aELEMENT *a, aELEMENT *b)
{
	return acmp(*a, *b);
}

aELEMENT *vasort(aELEMENT *ary, ptrdiff_t len)
{
	if (!ary || !len)
		return ary;
	jsort(ary, len, SIZEOF(aELEMENT), (int (*)(const void *, const void *))_acmp);
	return ary;
}

static void vadel(aELEMENT *ary, ptrdiff_t ofst, ptrdiff_t len)
{
	if (ary && ofst < aLen(ary)) {
		ptrdiff_t x;
		if (ofst + len > aLen(ary))
			len = aLen(ary) - ofst;
		for (x = ofst; x < ofst + len; ++x)
			adel(ary[x]);
		if (aLen(ary) - (ofst + len))
			mmove(ary + ofst, ary + ofst + len, (aLen(ary) - (ofst + len)) * SIZEOF(aELEMENT));
		aLen(ary) -= len;
		ary[aLen(ary)] = 0;
	}
}

void vauniq(aELEMENT *ary)
{
	if (ary) {
		ptrdiff_t x;
		ptrdiff_t len = aLen(ary);
		for (x = 0; x < len - 1; ++x) {
			ptrdiff_t y;
			for (y = x + 1;y < len; ++y)
				if (acmp(ary[x], ary[y]))
					break;
			vadel(ary, x + 1, y - (x + 1));
			len -= y - (x + 1);
		}
	}
}

aELEMENT *vawords(aELEMENT *a, const char *s, ptrdiff_t len, const char *sep, ptrdiff_t seplen)
{
	ptrdiff_t x;

	if (!a)
		a = vamk(10);
	else
		a = vatrunc(a, 0);
      loop:
	x = vsspan(s, len, sep, seplen);
	s += x;
	len -= x;
	if (len) {
		x = vsscan(s, len, sep, seplen);
		if (x != ~0) {
			a = vaadd(a, vsncpy(vsmk(x), 0, s, x));
			s += x;
			len -= x;
			if (len)
				goto loop;
		} else
			a = vaadd(a, vsncpy(vsmk(len), 0, s, len));
	}
	return a;
}
