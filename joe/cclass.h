/*
 *	Character classes and character maps
 *	Copyright
 *		(C) 2015 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */

/* An interval.  A character matches if it's in the range.
   An array of these can be used to define a character class. */

struct interval {
	int first;
	int last;
};

/* Test if character is in a sorted interval array using binary search.
   Returns -1 if not found, or index to matching struct interval */
ptrdiff_t interval_test(struct interval *array, ptrdiff_t size, int ch);

/* An interval list item.  This is one implementation of character -> pointer maps. */

struct interval_list {
	struct interval_list *next; /* Next item in list */
	struct interval interval; /* Range of characters */
	void *map;
};

/* Add a single interval to an interval list */
struct interval_list *interval_add(struct interval_list *interval_list, int first, int last, void *map);

/* Look up single character in an interval list, return what it's mapped to */
void *interval_lookup(struct interval_list *list, void *dflt, int ch);

/* Show an interval list */

/* A character classes and maps implemented as radix trees */

#define LEAFSIZE 16

#define SECONDSIZE 32

#define TOPSIZE 68

struct First {
	short entry[TOPSIZE];
};

struct Mid {
	short entry[SECONDSIZE]; /* THIRDSIZE too */
};

struct Leaf {
	void *entry[LEAFSIZE];
	int refcount; /* No. references */
};

struct Ileaf {
	int entry[LEAFSIZE];
	int refcount;
};

struct Level {
	int alloc; /* Allocation index */
	int size; /* Malloc size */
	union {
		struct Mid *b;
		struct Mid *c;
		struct Leaf *d;
		struct Ileaf *e;
	} table;
};

/* A radix tree bit-map for character classes */

struct Rset {
	struct First top;
	struct Level second;
	struct Mid mid;
	struct Level third;
};

/* A radix tree map: (Unicode character) -> (void *) or (Unicode character) -> (int) */

struct Rtree {
	struct First top;
	struct Level second;
	struct Mid mid;
	struct Level third;
	struct Level leaf;
};

/* Functions for character -> pointer maps */

void rtree_init(struct Rtree *r);
void rtree_clr(struct Rtree *r);
void *rtree_lookup(struct Rtree *r, int ch);
void rtree_opt(struct Rtree *r);
void rtree_set(struct Rtree *r, struct interval *array, ptrdiff_t len, void *map);
void rtree_build(struct Rtree *r, struct interval_list *l);

/* Functions for character -> int maps */

void rmap_init(struct Rtree *r);
int rmap_lookup(struct Rtree *r, int ch, int dflt);
void rmap_add(struct Rtree *r, int ch, int che, int map, int dflt);
void rmap_opt(struct Rtree *r);

/* A character class */

struct Cclass {
	ptrdiff_t size; /* Malloc size of intervals */
	ptrdiff_t len; /* Number of entries in intervals */
	struct interval *intervals; /* sorted, non-overlapping, non-adjacent */
	struct Rset rset[1]; /* Radix tree version of character class */
};

/* Initialize a character class */
void cclass_init(struct Cclass *cclass);

/* Add a range to a character class */
void cclass_add(struct Cclass *cclass, int first, int last);

/* Merge one character class into another */
void cclass_union(struct Cclass *cclass, struct Cclass *n);
void cclass_merge(struct Cclass *cclass, struct interval *intervals, int len);

/* Compute inverse of a character class */
void cclass_inv(struct Cclass *cclass);

/* Optimize a character class for fast lookup with cclass_lookup.  Generates a radix tree
   version of the character class. */
void cclass_opt(struct Cclass *cclass);

/* Return true if character is in the class */
int cclass_lookup(struct Cclass *cclass, int ch);

void cclass_show(struct Cclass *cclass);

/* Remap a character class */

struct Cclass *cclass_remap(struct Cclass *m, struct charmap *map);
