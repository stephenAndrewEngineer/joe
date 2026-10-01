/*
 *	Variable length arrays of strings
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */

/* Functions and global variable you have to define.  Replace these with
 * macros or defines here if they are not to be actual functions 
 */

typedef char *aELEMENT;

/* aELEMENT adel(); */
#define adel(s) vsrm(s)
/* extern aELEMENT aterm; */
#define aterm NULL

/************************/
/* Creation/Destruction */
/************************/

/* aELEMENT *vamk(int len);
 * Create a variable length array.  Space for 'len' elements is preallocated.
 */
aELEMENT *vamk(ptrdiff_t len);

/* void varm(aELEMENT *vary);
 * Free an array and everything which is in it.  Does nothing if 'vary' is
 * 0.
 */
void varm(aELEMENT *vary);

/********************/
/* Space management */
/********************/

#define aSiz(a) (*((ptrdiff_t *)(a) - 2))

/* int aLEN(aELEMENT *vary);
 * int aLen(aELEMENT *vary);
 * Access length part of array.  This int indicates the number of elements
 * currently in the array (not including the terminator).  This should be
 * used primarily for reading the size of the array.  It can be used for
 * setting the size of the array, but it must be used with care since it
 * does not eliminate elements (if the size decreases) or make sure there's
 * enough room (if the size increases).  See vensure and vtrunc.
 *
 * aLEN return a length of zero if 'vary' is 0.
 * aLen doesn't do this checking, but can be used as an lvalue
 */
#define aLEN(a) ((a) ? *((ptrdiff_t *)(a) - 1) : 0)
#define aLen(a) (*((ptrdiff_t *)(a) - 1))

/* int alen(aELEMENT *ary);
 * Compute length of char or variable length array by searching for termination
 * element.  Returns 0 if 'vary' is 0.
 */
ptrdiff_t alen(aELEMENT *ary);

/* aELEMENT *vaensure(aELEMENT *vary, int len);
 * Make sure there's enough space in the array for 'len' elements.  Whenever
 * vaensure reallocs the array, it allocates 25% more than the necessary
 * minimum space in anticipation of future expansion.  If 'vary' is 0,
 * it creates a new array.
 */
aELEMENT *vaensure(aELEMENT *vary, ptrdiff_t len);

/* aELEMENT *vatrunc(aELEMENT *vary, int len);
 * Truncate array to indicated size.  This zaps or expands with blank elements
 * and sets the LEN() of the array.  A new array is created if 'vary' is 0.
 */
aELEMENT *vatrunc(aELEMENT *vary, ptrdiff_t len);

/************************************/
/* Function which write to an array */
/************************************/

#ifdef junk
/* aELEMENT *vancpy(aELEMENT *vary, int pos, aELEMENT *array, int len);
 * Copy 'len' elements from 'array' onto 'vary' beginning at position 'pos'.
 * 'array' can be a normal char array since the length is passed separately.  The
 * elements are copied, not duplicated.  A new array is created if 'vary' is
 * 0.  This does not zap previous elements.
 */
aELEMENT *vancpy(aELEMENT *vary, ptrdiff_t pos, aELEMENT *array, ptrdiff_t len);
#endif

/* aELEMENT *vaset(aELEMENT *vary, int pos, aELEMENT element);
 * Set an element in an array.  Any value of 'pos' is valid.  A new array
 * is created if 'vary' is 0.  The previous contents of the position is
 * deleted.    This does not duplicate 'element'.  If you need 'element'
 * duplicated, call: vaset(vary,pos,adup(element));
 */
aELEMENT *_vaset(aELEMENT *vary, ptrdiff_t pos, aELEMENT el);

#define vaset(v,p,el)  \
 (!(v) || (p) > aLen(v) || ((p) == aLen(v) && (p) == aSiz(v)) ?  \
  _vaset((v),(p),(el)) \
 : \
  ((p) == aLen(v) ? \
   ((v)[(p)+1] = (v)[p], (v)[p] = (el), aLen(v) = (p)+1, (v)) \
  : \
   (adel((v)[p]), (v)[p] = (el), (v)) \
  ) \
 )

/* aELEMENT *vaadd(aELEMENT *vary, aELEMENT element);
 * Concatenate a single element to the end of 'vary'.  A new array is created
 * if 'vary' is 0.  This does not duplicate element: call
 * vaadd(vary,adup(element));  If you need it duplicated.
 */
#define vaadd(v,el) \
 (!(v) || aLen(v) == aSiz(v) ? \
  _vaset((v), aLEN(v), (el)) \
 : \
  ((v)[aLen(v) + 1] = (v)[aLen(v)], (v)[aLen(v)] = (el), \
   aLen(v) = aLen(v) + 1, (v)) \
 )

/**************************************/
/* Functions which read from an array */
/**************************************/

/* These macros are used to generate the address/size pairs which get
 * passed to the functions of the previous section.
 */

/* { aELEMENT *, int } av(aELEMENT *array);
 * Return array,size pair.  Uses aLEN to get size.
 */
#define av(a) (a), aLEN(a)

/* { aELEMENT *, int } az(aELEMENT *array);
 * Return array,size pair.  Uses alen to get size.
 */
#define az(a) (a), alen(a)

/* { aELEMENT *, int } ac(aELEMENT *array);
 * Return array,size pair.  Uses 'sizeof' to get size.
 */
#define ac(a) (a), (sizeof(a) / sizeof(aELEMENT))

/* { aELEMENT *, int } arest(aELEMENT *vary, int pos);
 * Return array,size pair of rest of array beginning at pos.  If
 * pos is past end of array, gives size of 0.
 */
#define arest(a, p) ((a) + (p)), (((p) > aLEN(a)) ? 0 : aLen(a) - (p))

/* { aELEMENT *, int } apart(aELEMENT *vary, int pos, int len);
 * Return array,size pair of 'len' elements of array beginning with pos.  If
 * pos is past end of array, gives size of 0.  If pos+len is past end of array,
 * returns number of elements to end of array.
 */
#define apart(a, p, l) \
 ((a) + (p)), ((p) >= aLEN(a) ? 0 : ((p) + (l) > aLen(a) ? aLen(a) - (p) : (l)))

/* aELEMENT vaget(aELEMENT *vary, int pos);
 * Get an element from an array.  Any value of pos is valid; if it's past the
 * end of the array or if 'vary' is 0, the terminator is returned.  This
 * does not make a duplicate of the returned element.  If you want that, pass
 * the return value of this to adup.
 */
#define vaget(a, p) ((p) >= aLEN(a) ? aterm : (a)[p])

/*************************/
/* Searching and Sorting */
/*************************/

/* aELEMENT *vasort(aELEMENT *ary, int len)
 * Sort the elements of an array (char or variable length) using qsort().
 */
aELEMENT *vasort(aELEMENT *ary, ptrdiff_t len);

/* Eliminate duplicates from a sorted array */
void vauniq(aELEMENT *ary);

/* aELEMENT *vawords(aELEMENT *a, char *s, int len, char *sep, int seplen);
 * Generate list of strings out of words in 's' separated with the characters
 * in 'sep'.  The characters in 'sep' must be sorted.
 */
aELEMENT *vawords(aELEMENT *a, const char *s, ptrdiff_t len, const char *sep, ptrdiff_t seplen);
