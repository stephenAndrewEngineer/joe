/* Byte code fragment construction functions */

typedef struct frag Frag;

struct frag {
	unsigned char *start;
	ptrdiff_t len;
	ptrdiff_t size;
	ptrdiff_t align;
};

/* Initialize a fragment: 'alignment' sets the natural alignment of
 * the fragment.  After any emit or fetch function is called, the
 * fragment is filled to the next multiple of 'alignment'.
 */

void iz_frag(Frag *, ptrdiff_t alignmnet);

void clr_frag(Frag *);

/* Generate byte offset you need to add to p so that
 * it is an exact multiple of size (which is a power of 2).
 */

#define align_o(p, size) (((size) - 1) & -(ptrdiff_t)(p))

/* Align frag to next multiple of n */

void align_frag(Frag *f, ptrdiff_t n);

/* Append data to a fragment: return byte offset to data.
 * These do two alignments: one before and one after the emit.  Before,
 * it fills the fragment until its size is a multiple of the size of
 * the emitted data.  After the emit, it fills the fragment until its
 * size is a multiple of the natural alignment specified in iz_frag.
 */
 

ptrdiff_t emiti(Frag *f, int n);
ptrdiff_t emitp(Frag *f, void *p);

ptrdiff_t emit_branch(Frag *f, ptrdiff_t target);
void fixup_branch(Frag *f, ptrdiff_t pos);
void frag_link(Frag *f, ptrdiff_t chain);

/* Fetch an datum from a fragment and advance the "PC" */

int fetchi(Frag *f, ptrdiff_t *pc);
void *fetchp(Frag *f, ptrdiff_t *pc);
void fin_code(Frag *f);
