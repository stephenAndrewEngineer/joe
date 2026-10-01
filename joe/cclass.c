/*
 *	Characters classes
 *	Copyright
 *		(C) 2015 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */

#include "types.h"

/* Moved here from the headers: used only in this file */

static struct interval_list *mkinterval(struct interval_list *next, int first, int last, void *map);

static void rminterval(struct interval_list *item);

#define LEAFMASK 0xF

#define LEAFSHIFT 0

#define THIRDSIZE 32

#define THIRDMASK 0x1f

#define THIRDSHIFT 4

#define SECONDMASK 0x1f

#define SECONDSHIFT 9

#define TOPMASK 0x7f

#define TOPSHIFT 14

static void rset_init(struct Rset *r);

static int rset_lookup(struct Rset *r, int ch);

static void rset_add(struct Rset *r, int ch, int che);

static void rset_opt(struct Rset *r);

static void rset_set(struct Rset *r, struct interval *array, ptrdiff_t size);

static void rtree_add(struct Rtree *r, int ch, int che, void *map);

/* Sort an array of struct intervals */

/* Test if character ch is in one of the struct intervals in array using binary search.
 * Returns index to interval, or -1 if none found. */

ptrdiff_t interval_test(struct interval *array, ptrdiff_t size, int ch)
{
	if (size) {
		ptrdiff_t min = 0;
		ptrdiff_t mid;
		ptrdiff_t max = size - 1;
		if (ch < array[min].first || ch > array[max].last)
			goto no_match;
		while (max >= min) {
			mid = (min + max) / 2;
			if (ch > array[mid].last)
				min = mid + 1;
			else if (ch < array[mid].first)
				max = mid - 1;
			else
				return mid;
		}
	}
	no_match:
	return -1;
}

static struct interval_list *mkinterval(struct interval_list *next, int first, int last, void *map)
{
	struct interval_list *interval_list = (struct interval_list *)joe_malloc(SIZEOF(struct interval_list));
	interval_list->next = next;
	interval_list->interval.first = first;
	interval_list->interval.last = last;
	interval_list->map = map;
	return interval_list;
}

static void rminterval(struct interval_list *interval_list)
{
	while (interval_list) {
		struct interval_list *n = interval_list->next;
		free(interval_list);
		interval_list = n;
	}
}

/* Add a single character range to an interval list */

struct interval_list *interval_add(struct interval_list *interval_list, int first, int last, void *map)
{
	struct interval_list *e, **p;
	for (p = &interval_list; *p;) {
		e = *p;
		if (first > e->interval.last + 1 || (first > e->interval.last && (map != e->map))) {
			/* e is below new range, skip it */
			p = &e->next;
		} else if (e->interval.first > last + 1 || (e->interval.first > last && (map != e->map))) {
			/* e is fully above new range, so insert new one here */
			break;
		} else if (e->map == map) {
			/* merge e into new */
			if (e->interval.first <= first) {
				if (e->interval.last >= last) { /* && e->interval.first <= first */
					/* Existing covers new: we're done */
					return interval_list;
				} else { /* e->interval.last < last && e->interval.first <= first */
					/* Enlarge new, delete existing */
					first = e->interval.first;
					*p = e->next;
					e->next = 0;
					rminterval(e);
				}
			} else { /* e->interval.first > first */
				if (e->interval.last <= last) { /* && e->interval.first > first */
					/* New fully covers existing, delete existing */
					*p = e->next;
					e->next = 0;
					rminterval(e);
				} else { /* e->interval.last > last && e->interval.first > first */
					/* Extend existing */
					e->interval.first = first;
					return interval_list;
				}
			}
		} else {
			/* replace e with new */
			if (e->interval.first < first) {
				if (e->interval.last <= last) { /* && e->interval.first < first */
					/* Top part of existing get cut-off by new */
					e->interval.last = first - 1;
				} else { /* e->interval.last > last && e->interval.first < first */
					int org = e->interval.last;
					void *orgmap = e->map;
					e->interval.last = first - 1;
					p = &e->next;
					*p = mkinterval(*p, first, last, map);
					p = &(*p)->next;
					*p = mkinterval(*p, last + 1, org, orgmap);
					return interval_list;
				}
			} else { /* e->interval.first >= first */
				if (e->interval.last <= last) { /* && e->interval.first >= first */
					/* Delete existing */
					*p = e->next;
					e->next = 0;
					rminterval(e);
				} else { /* e->interval.last > last && e->interval.first >= first */
					e->interval.first = last + 1;
					break;
				}
			}
		}
	}
	*p = mkinterval(*p, first, last, map);
	return interval_list;
}

/* Add a list of intervals (typically representing a character class) to an interval list */

void *interval_lookup(struct interval_list *list, void *dflt, int item)
{
	if (item < 0)
		item += 256;
	while (list) {
		if (item >= list->interval.first && item <= list->interval.last)
			return list->map;
		list = list->next;
	}
	return dflt;
}

/* Return true if character ch is in radix tree r */

static int rset_lookup(struct Rset *r, int ch)
{
	int a, b, c, d;

	if (ch < 0)
		ch += 256;

	a = (ch >> TOPSHIFT);
	b = (SECONDMASK & (ch >> SECONDSHIFT));
	c = (THIRDMASK & (ch >> THIRDSHIFT));
	d = (LEAFMASK & (ch >> LEAFSHIFT));

	if (a < 0 || a >= TOPSIZE)
		return 0;

	if (a || b) { /* Full lookup for character >= 512 */
		int idx = r->top.entry[a];
		if (idx != -1) {
			idx = r->second.table.b[idx].entry[b];
			if (idx != -1) {
				idx = r->third.table.c[idx].entry[c];
				return ((1 << d) & idx) != 0;
			}
		}
	} else { /* Quick lookup for character < 512 */
		int idx = r->mid.entry[c];
		return ((1 << d) & idx) != 0;
	}
	return 0;
}

/* Same as above, but can be be called before rset_opt() */

static void rset_init(struct Rset *r)
{
	int x;
	for (x = 0; x != TOPSIZE; ++x)
		r->top.entry[x] = -1;

	r->second.alloc = 0;
	r->second.size = 1;
	r->second.table.b = (struct Mid *)joe_malloc(r->second.size * SIZEOF(struct Mid));

	r->third.alloc = 0;
	r->third.size = 1;
	r->third.table.c = (struct Mid *)joe_malloc(r->third.size * SIZEOF(struct Mid));
}

static short rset_alloc(struct Level *l, int levelno)
{
	int x;
	if (l->alloc == l->size) {
		l->size *= 2;
		switch (levelno) {
			case 1: {
				l->table.b = (struct Mid *)joe_realloc(l->table.b, l->size * SIZEOF(struct Mid));
				break;
			} case 2: {
				l->table.c = (struct Mid *)joe_realloc(l->table.c, l->size * SIZEOF(struct Mid));
				break;
			}
		}
	}
	switch (levelno) {
		case 1: {
			for (x = 0; x != SECONDSIZE; ++x)
				l->table.b[l->alloc].entry[x] = -1;
			break;
		} case 2: {
			for (x = 0; x != THIRDSIZE; ++x)
				l->table.c[l->alloc].entry[x] = 0;
			break;
		}
	}
	if (l->alloc == 32768) {
		fprintf(stderr,"rset_alloc overflow\r\n");
		exit(-1);
	}
	return (short)l->alloc++;
}

static void rset_add(struct Rset *r, int ch, int che)
{
	int a = (TOPMASK & (ch >> TOPSHIFT));
	int b = (SECONDMASK & (ch >> SECONDSHIFT));
	int c = (THIRDMASK & (ch >> THIRDSHIFT));
	int d = (LEAFMASK & (ch >> LEAFSHIFT));

	short ib;
	short ic;

	while (ch <= che) {

		if (a >= TOPSIZE)
			return;

		ib = r->top.entry[a];
		if (ib == -1) {
			r->top.entry[a] = ib = rset_alloc(&r->second, 1);
		}

		while (ch <= che) {
			ic = r->second.table.b[ib].entry[b];
			if (ic == -1) {
				r->second.table.b[ib].entry[b] = ic = rset_alloc(&r->third, 2);
			}
			while (ch <= che) {
				r->third.table.c[ic].entry[c] |= (short)(1 << d);
				++ch;
				if (++d == LEAFSIZE) {
					d = 0;
					if (++c == THIRDSIZE) {
						c = 0;
						break;
					}
				}
			}
			if (++b == SECONDSIZE) {
				b = 0;
				break;
			}
		}
		++a;
	}
}

/* Optimize radix tree: setup mid */

static void rset_opt(struct Rset *r)
{
	int x;
	int idx;
	/* Set up mid table */
	for (x = 0; x != THIRDSIZE; ++x)
		r->mid.entry[x] = 0;
	idx = r->top.entry[0];
	if (idx != -1) {
		idx = r->second.table.b[idx].entry[0];
		if (idx != -1) {
			mcpy(r->mid.entry, r->third.table.c[idx].entry, SIZEOF(r->mid.entry));
		}
	}
}

static void rset_set(struct Rset *r, struct interval *array, ptrdiff_t size)
{
	ptrdiff_t y;
	for (y = 0; y != size; ++y) {
		rset_add(r, array[y].first, array[y].last);
	}
}

/* Radix tree maps */

void *rtree_lookup(struct Rtree *r, int ch)
{
	int a, b, c, d;
	if (ch < 0)
		ch += 256;
	a = (ch >> TOPSHIFT);
	b = (SECONDMASK & (ch >> SECONDSHIFT));
	c = (THIRDMASK & (ch >> THIRDSHIFT));
	d = (LEAFMASK & (ch >> LEAFSHIFT));
	if (a >= TOPSIZE)
		return NULL;
	if (a || b) { /* Full lookup for character >= 512 */
		int idx = r->top.entry[a];
		if (idx != -1) {
			idx = r->second.table.b[idx].entry[b];
			if (idx != -1) {
				idx = r->third.table.c[idx].entry[c];
				if (idx != -1)
					return r->leaf.table.d[idx].entry[d];
			}
		}
	} else { /* Quick lookup for character < 512 */
		int idx = r->mid.entry[c];
		if (idx != -1)
			return r->leaf.table.d[idx].entry[d];
	}
	return NULL;
}

void rtree_init(struct Rtree *r)
{
	int x;
	for (x = 0; x != TOPSIZE; ++x)
		r->top.entry[x] = -1;

	r->second.alloc = 0;
	r->second.size = 1;
	r->second.table.b = (struct Mid *)joe_malloc(r->second.size * SIZEOF(struct Mid));

	r->third.alloc = 0;
	r->third.size = 1;
	r->third.table.c = (struct Mid *)joe_malloc(r->third.size * SIZEOF(struct Mid));

	r->leaf.alloc = 0;
	r->leaf.size = 1;
	r->leaf.table.d = (struct Leaf *)joe_malloc(r->leaf.size * SIZEOF(struct Leaf));
}

void rtree_clr(struct Rtree *r)
{
	joe_free(r->second.table.b);
	joe_free(r->third.table.c);
	joe_free(r->leaf.table.d);
}

static short rtree_alloc(struct Level *l, int levelno)
{
	int x;
	if (l->alloc == l->size) {
		l->size *= 2;
		switch (levelno) {
			case 1: {
				l->table.b = (struct Mid *)joe_realloc(l->table.b, l->size * SIZEOF(struct Mid));
				break;
			} case 2: {
				l->table.c = (struct Mid *)joe_realloc(l->table.c, l->size * SIZEOF(struct Mid));
				break;
			} case 3: {
				l->table.d = (struct Leaf *)joe_realloc(l->table.d, l->size * SIZEOF(struct Leaf));
				break;
			}
		}
	}
	switch (levelno) {
		case 1: {
			for (x = 0; x != SECONDSIZE; ++x)
				l->table.b[l->alloc].entry[x] = -1;
			break;
		} case 2: {
			for (x = 0; x != THIRDSIZE; ++x)
				l->table.c[l->alloc].entry[x] = -1;
			break;
		} case 3: {
			for (x = 0; x != LEAFSIZE; ++x)
				l->table.d[l->alloc].entry[x] = NULL;
			l->table.d[l->alloc].refcount = 1;
			break;
		}
	}
	if (l->alloc == 32768) {
		fprintf(stderr,"rtree_alloc overflow\r\n");
		exit(-1);
	}
	return (short)l->alloc++;
}

static void rtree_add(struct Rtree *r, int ch, int che, void *map)
{
	int a = (TOPMASK & (ch >> TOPSHIFT));
	int b = (SECONDMASK & (ch >> SECONDSHIFT));
	int c = (THIRDMASK & (ch >> THIRDSHIFT));
	int d = (LEAFMASK & (ch >> LEAFSHIFT));

	short ib;
	short ic;
	short id;

	/* printf("%p rtree_add %x..%x [%d %d %d %d] -> %p\n",r, ch, che, a, b, c, d, map); */

	while (ch <= che) {

		if (a >= TOPSIZE)
			return;

		ib = r->top.entry[a];
		if (ib == -1) {
			r->top.entry[a] = ib = rtree_alloc(&r->second, 1);
		}

		while (ch <= che) {
			ic = r->second.table.b[ib].entry[b];
			if (ic == -1) {
				r->second.table.b[ib].entry[b] = ic = rtree_alloc(&r->third, 2);
			}

			while (ch <= che) {
				id = r->third.table.c[ic].entry[c];
				if (id == -1) {
					r->third.table.c[ic].entry[c] = id = rtree_alloc(&r->leaf, 3);
				}

				while (ch <= che) {
					struct Leaf *l = r->leaf.table.d + id;
					
					/* Copy on write */
					if (l->refcount != 1) {
						struct Leaf *org = l;
						r->third.table.c[ic].entry[c] = id = rtree_alloc(&r->leaf, 3);
						l = r->leaf.table.d + id;
						mcpy(l, org, SIZEOF(struct Leaf));
						--org->refcount;
						l->refcount = 1;
					}

					/* Write */
					l->entry[d] = map;

					/* Try to de-duplicate with previous entry */
					if (d == LEAFSIZE - 1 && id && id == r->leaf.alloc - 1 && !memcmp(l->entry, r->leaf.table.d[id - 1].entry, LEAFSIZE * SIZEOF(void *))) {
						--r->leaf.alloc;
						--id;
						l = r->leaf.table.d + id;
						++l->refcount;
						r->third.table.c[ic].entry[c] = id;
					}

					++ch;
					if (++d == LEAFSIZE) {
						d = 0;
						break;
					}
				}
				if (++c == THIRDSIZE) {
					c = 0;
					break;
				}
			}
			if (++b == SECONDSIZE) {
				b = 0;
				break;
			}
		}
		++a;
	}
}

/* Optimize radix tree: de-duplicate leaf nodes and setup mid */

struct rhentry {
	struct rhentry *next;
	struct Leaf *leaf;
	int idx;
};

static ptrdiff_t rhhash(struct Leaf *l)
{
	ptrdiff_t hval = 0;
	int x;
	for (x = 0; x != LEAFSIZE; ++x)
		hval = (hval << 4) + (hval >> 28) + (ptrdiff_t)l->entry[x];
	return hval;
		
}

void rtree_opt(struct Rtree *r)
{
	ptrdiff_t idx;
	int x;
	int rhsize;
	struct rhentry **rhtable;
	int *equiv;
	short *repl;
	int dupcount;
	int newalloc;
	struct Leaf *l;

	/* De-duplicate leaf nodes (it's not worth bothering with interior nodes) */

	equiv = (int *)joe_malloc(SIZEOF(int) * r->leaf.alloc);
	repl = (short *)joe_malloc(SIZEOF(short) * r->leaf.alloc);

	/* Create hash table index of all the leaf nodes */
	dupcount = 0;
	rhsize = 1024;
	rhtable = (struct rhentry **)joe_calloc(SIZEOF(struct rhentry *), rhsize);
	for (x = 0; x != r->leaf.alloc; ++x) {
		struct rhentry *rh;
		idx = ((rhsize - 1) & rhhash(r->leaf.table.d + x));
		/* Already exists? */
		for (rh = rhtable[idx]; rh; rh = rh->next)
			if (!memcmp(rh->leaf->entry, r->leaf.table.d[x].entry, LEAFSIZE * SIZEOF(void *)))
				break;
		if (rh) {
			equiv[x] = rh->idx;
			++dupcount;
		} else {
			equiv[x] = -1;
			rh = (struct rhentry *)joe_malloc(SIZEOF(struct rhentry));
			rh->next = rhtable[idx];
			rh->idx = x;
			rh->leaf = r->leaf.table.d + x;
			rhtable[idx] = rh;
		}
	}
	/* Free hash table */
	for (x = 0; x != 1024; ++x) {
		struct rhentry *rh, *nh;
		for (rh = rhtable[x]; rh; rh = nh) {
			nh = rh->next;
			joe_free(rh);
		}
	}
	joe_free(rhtable);

	/* Create new leaf table */
	newalloc = r->leaf.alloc - dupcount;
	l = (struct Leaf *)joe_malloc(SIZEOF(struct Leaf) * newalloc);

	/* Copy entries */
	idx = 0;
	for (x = 0; x != r->leaf.alloc; ++x)
		if (equiv[x] == -1) {
			mcpy(l + idx, r->leaf.table.d + x, sizeof(struct Leaf));
			l[idx].refcount = 1;
			if (idx > 32767) {
				fprintf(stderr, "cclass too complex\r\n");
				ttsig(-1);
			}
			repl[x] = (short)idx;
			++idx;
		}
	for (x = 0; x != r->leaf.alloc; ++x)
		if (equiv[x] != -1) {
			repl[x] = repl[equiv[x]];
			++l[repl[equiv[x]]].refcount;
		}

	/* Install new table */
	joe_free(r->leaf.table.d);
	r->leaf.table.d = l;
	r->leaf.alloc = newalloc;
	r->leaf.size = newalloc;

	/* Remap third level */
	for (x = 0; x != r->third.alloc; ++x)
		for (idx = 0; idx != THIRDSIZE; ++idx)
			if (r->third.table.c[x].entry[idx] != -1)
				r->third.table.c[x].entry[idx] = repl[r->third.table.c[x].entry[idx]];

	joe_free(equiv);
	joe_free(repl);

	/* Set up mid table */
	for (x = 0; x != THIRDSIZE; ++x)
		r->mid.entry[x] = -1;
	idx = r->top.entry[0];
	if (idx != -1) {
		idx = r->second.table.b[idx].entry[0];
		if (idx != -1) {
			mcpy(r->mid.entry, r->third.table.c[idx].entry, SIZEOF(r->mid.entry));
		}
	}
}

void rtree_set(struct Rtree *r, struct interval *array, ptrdiff_t len, void *map)
{
	ptrdiff_t y;
	for (y = 0; y != len; ++y) {
		rtree_add(r, array[y].first, array[y].last, map);
	}
}

void rtree_build(struct Rtree *r, struct interval_list *l)
{
	while (l) {
		// printf("%p build %x %x %p\n",r, l->interval.first, l->interval.last, l->map);
		rtree_add(r, l->interval.first, l->interval.last, l->map);
		l = l->next;
	}
}

/* Radix tree maps */

int rmap_lookup(struct Rtree *r, int ch, int dflt)
{
	int a, b, c, d;
	if (ch < 0)
		ch += 256;
	a = (ch >> TOPSHIFT);
	b = (SECONDMASK & (ch >> SECONDSHIFT));
	c = (THIRDMASK & (ch >> THIRDSHIFT));
	d = (LEAFMASK & (ch >> LEAFSHIFT));
	if (a >= TOPSIZE)
		return dflt;
	if (a || b) { /* Full lookup for character >= 512 */
		int idx = r->top.entry[a];
		if (idx != -1) {
			idx = r->second.table.b[idx].entry[b];
			if (idx != -1) {
				idx = r->third.table.c[idx].entry[c];
				if (idx != -1)
					return r->leaf.table.e[idx].entry[d];
			}
		}
	} else { /* Quick lookup for character < 512 */
		int idx = r->mid.entry[c];
		if (idx != -1)
			return r->leaf.table.e[idx].entry[d];
	}
	return dflt;
}

void rmap_init(struct Rtree *r)
{
	int x;
	for (x = 0; x != TOPSIZE; ++x)
		r->top.entry[x] = -1;

	r->second.alloc = 0;
	r->second.size = 1;
	r->second.table.b = (struct Mid *)joe_malloc(r->second.size * SIZEOF(struct Mid));

	r->third.alloc = 0;
	r->third.size = 1;
	r->third.table.c = (struct Mid *)joe_malloc(r->third.size * SIZEOF(struct Mid));

	r->leaf.alloc = 0;
	r->leaf.size = 1;
	r->leaf.table.e = (struct Ileaf *)joe_malloc(r->leaf.size * SIZEOF(struct Ileaf));
}

static short rmap_alloc(struct Level *l, int levelno, int dflt)
{
	int x;
	if (l->alloc == l->size) {
		l->size *= 2;
		switch (levelno) {
			case 1: {
				l->table.b = (struct Mid *)joe_realloc(l->table.b, l->size * SIZEOF(struct Mid));
				break;
			} case 2: {
				l->table.c = (struct Mid *)joe_realloc(l->table.c, l->size * SIZEOF(struct Mid));
				break;
			} case 3: {
				l->table.e = (struct Ileaf *)joe_realloc(l->table.e, l->size * SIZEOF(struct Ileaf));
				break;
			}
		}
	}
	switch (levelno) {
		case 1: {
			for (x = 0; x != SECONDSIZE; ++x)
				l->table.b[l->alloc].entry[x] = -1;
			break;
		} case 2: {
			for (x = 0; x != THIRDSIZE; ++x)
				l->table.c[l->alloc].entry[x] = -1;
			break;
		} case 3: {
			for (x = 0; x != LEAFSIZE; ++x)
				l->table.e[l->alloc].entry[x] = dflt;
			l->table.e[l->alloc].refcount = 1;
			break;
		}
	}
	if (l->alloc == 32768) {
		fprintf(stderr,"rmap_alloc overflow\r\n");
		ttsig(-1);
	}
	return (short)l->alloc++;
}

void rmap_add(struct Rtree *r, int ch, int che, int map, int dflt)
{
	int a = (TOPMASK & (ch >> TOPSHIFT));
	int b = (SECONDMASK & (ch >> SECONDSHIFT));
	int c = (THIRDMASK & (ch >> THIRDSHIFT));
	int d = (LEAFMASK & (ch >> LEAFSHIFT));

	short ib;
	short ic;
	short id;

	while (ch <= che) {

		if (a >= TOPSIZE)
			return;

		ib = r->top.entry[a];
		if (ib == -1) {
			r->top.entry[a] = ib = rmap_alloc(&r->second, 1, dflt);
		}

		while (ch <= che) {
			ic = r->second.table.b[ib].entry[b];
			if (ic == -1) {
				r->second.table.b[ib].entry[b] = ic = rmap_alloc(&r->third, 2, dflt);
			}

			while (ch <= che) {
				id = r->third.table.c[ic].entry[c];
				if (id == -1) {
					r->third.table.c[ic].entry[c] = id = rmap_alloc(&r->leaf, 3, dflt);
				}

				while (ch <= che) {
					struct Ileaf *l = r->leaf.table.e + id;

					/* Copy on write */
					if (l->refcount != 1) {
						struct Ileaf *org = l;
						r->third.table.c[ic].entry[c] = id = rmap_alloc(&r->leaf, 3, dflt);
						l = r->leaf.table.e + id;
						mcpy(l, org, SIZEOF(struct Ileaf));
						--org->refcount;
						l->refcount = 1;
					}

					/* Write */
					l->entry[d] = map;

					/* Try to de-duplicate with previous entry */
					if (d == LEAFSIZE - 1 && id && id == r->leaf.alloc - 1 && !memcmp(l->entry, r->leaf.table.e[id - 1].entry, LEAFSIZE * SIZEOF(int))) {
						--r->leaf.alloc;
						--id;
						l = r->leaf.table.e + id;
						++l->refcount;
						r->third.table.c[ic].entry[c] = id;
					}

					++ch;
					if (++d == LEAFSIZE) {
						d = 0;
						break;
					}
				}
				if (++c == THIRDSIZE) {
					c = 0;
					break;
				}
			}
			if (++b == SECONDSIZE) {
				b = 0;
				break;
			}
		}
		++a;
	}
}

/* Optimize radix tree: de-duplicate leaf nodes and setup mid */

struct rmaphentry {
	struct rmaphentry *next;
	struct Ileaf *leaf;
	int idx;
};

static ptrdiff_t rmaphash(struct Ileaf *l)
{
	ptrdiff_t hval = 0;
	int x;
	for (x = 0; x != LEAFSIZE; ++x)
		hval = (hval << 4) + (hval >> 28) + l->entry[x];
	return hval;
		
}

void rmap_opt(struct Rtree *r)
{
	ptrdiff_t idx;
	int x;
	int rhsize;
	struct rmaphentry **rhtable;
	int *equiv;
	short *repl;
	int dupcount;
	int newalloc;
	struct Ileaf *l;

	/* De-duplicate leaf nodes (it's not worth bothering with interior nodes) */

	equiv = (int *)joe_malloc(SIZEOF(int) * r->leaf.alloc);
	repl = (short *)joe_malloc(SIZEOF(short) * r->leaf.alloc);

	/* Create hash table index of all the leaf nodes */
	dupcount = 0;
	rhsize = 1024;
	rhtable = (struct rmaphentry **)joe_calloc(SIZEOF(struct rmaphentry *), rhsize);
	for (x = 0; x != r->leaf.alloc; ++x) {
		struct rmaphentry *rh;
		idx = ((rhsize - 1) & rmaphash(r->leaf.table.e + x));
		/* Already exists? */
		for (rh = rhtable[idx]; rh; rh = rh->next)
			if (!memcmp(rh->leaf, r->leaf.table.e + x, SIZEOF(struct Ileaf)))
				break;
		if (rh) {
			equiv[x] = rh->idx;
			++dupcount;
		} else {
			equiv[x] = -1;
			rh = (struct rmaphentry *)joe_malloc(SIZEOF(struct rmaphentry));
			rh->next = rhtable[idx];
			rh->idx = x;
			rh->leaf = r->leaf.table.e + x;
			rhtable[idx] = rh;
		}
	}
	/* Free hash table */
	for (x = 0; x != 1024; ++x) {
		struct rmaphentry *rh, *nh;
		for (rh = rhtable[x]; rh; rh = nh) {
			nh = rh->next;
			joe_free(rh);
		}
	}
	joe_free(rhtable);

	/* Create new leaf table */
	newalloc = r->leaf.alloc - dupcount;
	l = (struct Ileaf *)joe_malloc(SIZEOF(struct Ileaf) * newalloc);

	/* Copy entries */
	idx = 0;
	for (x = 0; x != r->leaf.alloc; ++x)
		if (equiv[x] == -1) {
			mcpy(l + idx, r->leaf.table.e + x, sizeof(struct Ileaf));
			l[idx].refcount = 1;
			if (idx > 32767) {
				fprintf(stderr, "Oops, cclass too complex\n");
				ttsig(-1);
			}
			repl[x] = (short)idx;
			++idx;
		}
	for (x = 0; x != r->leaf.alloc; ++x)
		if (equiv[x] != -1) {
			repl[x] = repl[equiv[x]];
			++l[repl[equiv[x]]].refcount;
		}

	/* Install new table */
	joe_free(r->leaf.table.e);
	r->leaf.table.e = l;
	r->leaf.alloc = newalloc;
	r->leaf.size = newalloc;

	/* Remap third level */
	for (x = 0; x != r->third.alloc; ++x)
		for (idx = 0; idx != THIRDSIZE; ++idx)
			if (r->third.table.c[x].entry[idx] != -1)
				r->third.table.c[x].entry[idx] = repl[r->third.table.c[x].entry[idx]];

	joe_free(equiv);
	joe_free(repl);

	/* Set up mid table */
	for (x = 0; x != THIRDSIZE; ++x)
		r->mid.entry[x] = -1;
	idx = r->top.entry[0];
	if (idx != -1) {
		idx = r->second.table.b[idx].entry[0];
		if (idx != -1) {
			mcpy(r->mid.entry, r->third.table.c[idx].entry, SIZEOF(r->mid.entry));
		}
	}
}

/* Character classes */

void cclass_init(struct Cclass *m)
{
	m->size = 0;
	m->len = 0;
	m->intervals = 0;
}

/* Delete range from character class at position x
 * x is in range 0 to (m->len - 1), inclusive.
 */
static void cclass_del(struct Cclass *m, int x)
{
	mmove(m->intervals + x, m->intervals + x + 1, SIZEOF(struct interval) * (m->len - (x + 1)));
	--m->len;
}

/* We are about to insert: expand array if necessary */
static void cclass_grow(struct Cclass *m)
{
	if (m->len == m->size) {
		if (!m->size) {
			m->size = 1;
			m->intervals = (struct interval *)joe_malloc(SIZEOF(struct interval) * (m->size));
		} else {
			m->size *= 2;
			m->intervals = (struct interval *)joe_realloc(m->intervals, SIZEOF(struct interval) * (m->size));
		}
	}
}

/* Insert a range into a character class at position x
 * x is in range 0 to m->len inclusive.
 */
static void cclass_ins(struct Cclass *m, int x, int first, int last)
{
	cclass_grow(m);
	mmove(m->intervals + x + 1, m->intervals + x, SIZEOF(struct interval) * (m->len - x));
	++m->len;
	m->intervals[x].first = first;
	m->intervals[x].last = last;
}

/* Add a single range [first,last] into class m.  The resulting m->intervals will
 * be sorted and consist of non-overlapping, non-adjacent ranges.
 */

void cclass_add(struct Cclass *m, int first, int last)
{
	int x;

	/* If it's invalid, ignore */
	if (last < first || first < 0)
		return;

	for (x = 0; x != m->len; ++x) {
		if (first > m->intervals[x].last + 1) {
			/* intervals[x] is below new range, skip it. */
		} else if (m->intervals[x].first > last + 1) {
			/* intervals[x] is fully above new range, so insert new one here */
			break;
		} else if (m->intervals[x].first <= first) {
			if (m->intervals[x].last >= last) { /* && m->intervals[x].first <= first */
				/* Existing covers new: we're done. */
				return;
			} else { /* m->intervals[x].last < last && m->intervals[x].first <= first */
				/* Enlarge new, delete existing */
				first = m->intervals[x].first;
				cclass_del(m, x);
				--x;
			}
		} else { /* m->intervals[x].first > first */
			if (m->intervals[x].last <= last) { /* && m->intervals[x].first <= first */
				/* New fully covers existing, delete existing */
				cclass_del(m, x);
				--x;
			} else { /* m->intervals[x].last > last && m->intervals[x].first > first */
				/* Extend existing */
				m->intervals[x].first = first;
				return;
			}
		}
	}
	cclass_ins(m, x, first, last);
}

/* Merge class n into class m */

void cclass_union(struct Cclass *m, struct Cclass *n)
{
	int x;
	if (n)
		for (x = 0; x != n->len; ++x)
			cclass_add(m, n->intervals[x].first, n->intervals[x].last);
}

void cclass_merge(struct Cclass *m, struct interval *array, int len)
{
	int x;
	for (x = 0; x != len; ++x)
		cclass_add(m, array[x].first, array[x].last);
}

/* Subtract a single range [first,last] from class m.  The resulting m->array will
 * be sorted and consist of non-overlapping, non-adjacent ranges.
 */

/* Remove any parts of m which also appear in n */

/* Compute inverse of class m */

#define UNICODE_LAST 0x10FFFF

void cclass_inv(struct Cclass *m)
{
//	printf("\r\nBefore:\n");
//	cclass_show(m);
	if (m->len && !m->intervals[0].first) {
		/* Starts at 0 */
		int x;
		int last = m->intervals[0].last;
		for (x = 0; x != m->len - 1; ++x) {
			m->intervals[x].first = last + 1;
			m->intervals[x].last = m->intervals[x + 1].first - 1;
			last = m->intervals[x + 1].last;
		}
		if (last == UNICODE_LAST) {
			--m->len; /* Delete last one */
		} else {
			m->intervals[x].first = last + 1;
			m->intervals[x].last = UNICODE_LAST;
		}
	} else {
		/* Does not start at 0 */
		int x;
		int first = 0;
		for (x = 0; x != m->len; ++x) {
			int new_first = m->intervals[x].last + 1;
			m->intervals[x].last = m->intervals[x].first - 1;
			m->intervals[x].first = first;
			first = new_first;
		}
		if (first != UNICODE_LAST) {
			cclass_grow(m);
			m->intervals[x].first = first;
			m->intervals[x].last = UNICODE_LAST;
			++m->len;
		}
	}
//	printf("\r\nAfter:\n");
//	cclass_show(m);
//	sleep(1);
}

void cclass_opt(struct Cclass *m)
{
	rset_init(m->rset);
	rset_set(m->rset, m->intervals, m->len);
	rset_opt(m->rset);
}

int cclass_lookup(struct Cclass *m, int ch)
{
	return rset_lookup(m->rset, ch);
}

void cclass_show(struct Cclass *m)
{
	int x;
	int first = 0;
	for (x = 0; x != m->len; ++x) {
		if (!first)
			first = 1;
		else
			logmessage_0(" ");
		logmessage_2("[%x..%x]", (unsigned)m->intervals[x].first, (unsigned)m->intervals[x].last);
	}
	logmessage_0("\n");
}

/* Here is a list of struct Cclasses which have been remapped to some local character set */

struct Cclass_list {
	struct Cclass_list *next;
	struct Cclass *m;
	struct charmap *map;
	struct Cclass n[1];
} *cclass_list;

struct Cclass *cclass_remap(struct Cclass *m, struct charmap *map)
{
	if (!map)
		return 0;
	if (!map->type) {
		struct Cclass_list *l;
		ptrdiff_t x;
		int low, high;
		for (l = cclass_list; l; l = l->next) {
			if (l->m == m && l->map == map)
				return l->n;
		}
		l = (struct Cclass_list *)joe_malloc(SIZEOF(struct Cclass_list));
		l->next = cclass_list;
		cclass_list = l;
		l->m = m;
		l->map = map;
		cclass_init(l->n);
		low = -2;
		high = -2;
		for (x  = 0; x != m->len; ++x) {
			int a;
			for (a = m->intervals[x].first; a <= m->intervals[x].last; ++a) {
				int b = from_uni(map, a);
				if (b != -1) {
					if (b == high + 1) {
						high = b;
					} else {
						cclass_add(l->n, low, high);
						low = high = b;
					}
				}
			}
		}
		if (low != -2) {
			cclass_add(l->n, low, high);
		}
		return l->n;
	} else {
		return m;
	}
}
