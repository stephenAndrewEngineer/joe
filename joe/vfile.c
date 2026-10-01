/*
 *	Software virtual memory system
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"

/* Moved here from the headers: used only in this file */

static void vflsh(void);

static void vflshf(VFILE *vfile);

static VFILE vfiles = { {&vfiles, &vfiles} };	/* Known vfiles */
static VPAGE *freepages = NULL;	/* Linked list of free pages */
static VPAGE *htab[HTSIZE];	/* Hash table of page headers */
static long curvalloc = 0;	/* Amount of memory in use */
static long maxvalloc = ILIMIT;	/* Maximum allowed */
char *vbase;			/* Data first entry in vheader refers to */
VPAGE **vheaders = NULL;	/* Array of header addresses */
static ptrdiff_t vheadsz = 0;	/* No. entries allocated to vheaders */

static void vflsh(void)
{
	VPAGE *vp;
	VPAGE *vlowest;
	off_t addr;
	off_t last;
	VFILE *vfile;
	int x;

	for (vfile = vfiles.link.next; vfile != &vfiles; vfile = vfile->link.next) {
		last = -1;
	      loop:
		addr = MAXOFF;
		vlowest = NULL;
		for (x = 0; x != HTSIZE; x++)
			for (vp = htab[x]; vp; vp = vp->next)
				if (vp->addr < addr && vp->addr > last && vp->vfile == vfile && (vp->addr >= vfile->size || (vp->dirty && !vp->count))) {
					addr = vp->addr;
					vlowest = vp;
				}
		if (vlowest) {
			if (!vfile->name)
				vfile->name = mktmp(NULL);
			if (!vfile->fd)
				vfile->fd = open((vfile->name), O_RDWR);
			if (vfile->fd < 0)
				ttsig(-2);
			lseek(vfile->fd, addr, 0);
			if (addr + PGSIZE > vsize(vfile)) {
				if (joe_write(vfile->fd, vlowest->data, (int) (vsize(vfile) - addr)) < 0)
					ttsig(-2);
				vfile->size = vsize(vfile);
			} else {
				if (joe_write(vfile->fd, vlowest->data, PGSIZE) < 0)
					ttsig(-2);
				if (addr + PGSIZE > vfile->size)
					vfile->size = addr + PGSIZE;
			}
			vlowest->dirty = 0;
			last = addr;
			goto loop;
		}
	}
}

static void vflshf(VFILE *vfile)
{
	VPAGE *vp;
	VPAGE *vlowest;
	off_t addr;
	int x;

      loop:
	addr = MAXOFF;
	vlowest = NULL;
	for (x = 0; x != HTSIZE; x++)
		for (vp = htab[x]; vp; vp = vp->next)
			if (vp->addr < addr && vp->dirty && vp->vfile == vfile && !vp->count) {
				addr = vp->addr;
				vlowest = vp;
			}
	if (vlowest) {
		if (!vfile->name)
			vfile->name = mktmp(NULL);
		if (!vfile->fd) {
			vfile->fd = open((vfile->name), O_RDWR);
		}
		if (vfile->fd < 0)
			ttsig(-2);
		lseek(vfile->fd, addr, 0);
		if (addr + PGSIZE > vsize(vfile)) {
			if (joe_write(vfile->fd, vlowest->data, (int) (vsize(vfile) - addr)) < 0)
				ttsig(-2);
			vfile->size = vsize(vfile);
		} else {
			if (joe_write(vfile->fd, vlowest->data, PGSIZE) < 0)
				ttsig(-2);
			if (addr + PGSIZE > vfile->size)
				vfile->size = addr + PGSIZE;
		}
		vlowest->dirty = 0;
		goto loop;
	}
}

static char *mema(ptrdiff_t align, ptrdiff_t size)
{
	char *z = (char *)joe_malloc(align + size);

	return z + align - (ptrdiff_t)(physical(z) % (unsigned)align);
}

char *vlock(VFILE *vfile, off_t addr)
{
	VPAGE *vp, *pp;
	int x, y;
	off_t ofst = (addr & (PGSIZE - 1));

	addr -= ofst;

	for (vp = htab[((addr >> LPGSIZE) + (ptrdiff_t) vfile) & (HTSIZE - 1)]; vp; vp = vp->next)
		if (vp->vfile == vfile && vp->addr == addr) {
			++vp->count;
			return vp->data + ofst;
		}

	if (freepages) {
		vp = freepages;
		freepages = vp->next;
		goto gotit;
	}

	if (curvalloc + PGSIZE <= maxvalloc) {
		vp = (VPAGE *) joe_malloc(SIZEOF(VPAGE) * INC);
		if (vp) {
			vp->data = mema(PGSIZE, PGSIZE * INC);
			if (vp->data) {
				int q;

				curvalloc += PGSIZE * INC;
				if (!vheaders) {
					vheaders = (VPAGE **) joe_malloc((vheadsz = INC) * SIZEOF(VPAGE *));
					vbase = vp->data;
				} else if (physical(vp->data) < physical(vbase)) {
					VPAGE **t = vheaders;
					ptrdiff_t amnt = (ptrdiff_t)((physical(vbase) - physical(vp->data)) >> LPGSIZE);

					vheaders = (VPAGE **) joe_malloc((amnt + vheadsz) * SIZEOF(VPAGE *));
					mmove(vheaders + amnt, t, vheadsz * SIZEOF(VPAGE *));
					vheadsz += amnt;
					vbase = vp->data;
					joe_free(t);
				} else if (((physical(vp->data + PGSIZE * INC) - physical(vbase)) >> LPGSIZE) > (size_t)vheadsz) {
					vheaders = (VPAGE **)
					    joe_realloc(vheaders, (vheadsz = (ptrdiff_t)(((physical(vp->data + PGSIZE * INC) - physical(vbase)) >> LPGSIZE))) * SIZEOF(VPAGE *));
				}
				for (q = 1; q != INC; ++q) {
					vp[q].next = freepages;
					freepages = vp + q;
					vp[q].data = vp->data + q * PGSIZE;
					vheader(vp->data + q * PGSIZE) = vp + q;
				}
				vheader(vp->data) = vp;
				goto gotit;
			}
			joe_free(vp);
			vp = NULL;
		}
	}

	for (y = HTSIZE, x = (random() & (HTSIZE - 1)); y; x = ((x + 1) & (HTSIZE - 1)), --y)
		for (pp = (VPAGE *) (htab + x), vp = pp->next; vp; pp = vp, vp = vp->next)
			if (!vp->count && !vp->dirty) {
				pp->next = vp->next;
				goto gotit;
			}
	vflsh();
	for (y = HTSIZE, x = (random() & (HTSIZE - 1)); y; x = ((x + 1) & (HTSIZE - 1)), --y)
		for (pp = (VPAGE *) (htab + x), vp = pp->next; vp; pp = vp, vp = vp->next)
			if (!vp->count && !vp->dirty) {
				pp->next = vp->next;
				goto gotit;
			}
	if (-1 == joe_write(2, sz(joe_gettext(_("vfile: out of memory\n")))))
		exit(2);
	else
		exit(1);

      gotit:
	vp->addr = addr;
	vp->vfile = vfile;
	vp->dirty = 0;
	vp->count = 1;
	vp->next = htab[((addr >> LPGSIZE) + (ptrdiff_t)vfile) & (HTSIZE - 1)];
	htab[((addr >> LPGSIZE) + (ptrdiff_t)vfile) & (HTSIZE - 1)] = vp;

	if (addr < vfile->size) {
		if (!vfile->fd) {
			vfile->fd = open((vfile->name), O_RDWR);
		}
		if (vfile->fd < 0)
			ttsig(-2);
		lseek(vfile->fd, addr, 0);
		if (addr + PGSIZE > vfile->size) {
			if (joe_read(vfile->fd, vp->data, (int) (vfile->size - addr)) < 0)
				ttsig(-2);
			mset(vp->data + vfile->size - addr, 0, PGSIZE - (int) (vfile->size - addr));
		} else
			if (joe_read(vfile->fd, vp->data, PGSIZE) < 0)
				ttsig(-2);
	} else
		mset(vp->data, 0, PGSIZE);

	return vp->data + ofst;
}

VFILE *vtmp(void)
{
	VFILE *newf = (VFILE *) joe_malloc(SIZEOF(VFILE));

	newf->fd = 0;
	newf->name = NULL;
	newf->alloc = 0;
	newf->size = 0;
	newf->left = 0;
	newf->lv = 0;
	newf->vpage = NULL;
	newf->flags = 1;
	newf->vpage1 = NULL;
	newf->addr = -1;
	return enqueb_f(VFILE, link, &vfiles, newf);
}

void vclose(VFILE *vfile)
{
	VPAGE *vp, *pp;
	int x;

	if (vfile->vpage)
		vunlock(vfile->vpage);
	if (vfile->vpage1)
		vunlock(vfile->vpage1);
	if (vfile->name) {
		if (vfile->flags) {
		        if (vfile->fd) {
		                /* Unlink fails if file is open on some systems. */
		                close(vfile->fd);
		                vfile->fd = 0;
		        }
			unlink(vfile->name);
		} else
			vflshf(vfile);
		vsrm(vfile->name);
	}
	if (vfile->fd)
		close(vfile->fd);
	joe_free(deque_f(VFILE, link, vfile));
	for (x = 0; x != HTSIZE; x++)
		for (pp = (VPAGE *) (htab + x), vp = pp->next; vp;)
			if (vp->vfile == vfile) {
				pp->next = vp->next;
				vp->next = freepages;
				freepages = vp;
				vp = pp->next;
			} else {
				pp = vp;
				vp = vp->next;
			}
}

off_t my_valloc(VFILE *vfile, off_t size)
{
	off_t start = vsize(vfile);

	vfile->alloc = start + size;
	if (vfile->lv) {
		if (vheader(vfile->vpage)->addr + PGSIZE > vfile->alloc)
			vfile->lv = PGSIZE - (vfile->alloc - vheader(vfile->vpage)->addr);
		else
			vfile->lv = 0;
	}
	return start;
}

