#import util.c

/* mem.h - generic obstack library */

pub typedef void clearf_t(void *);

pub typedef {
  size_t osize;
  void **stack, **base;
  size_t size;
  clearf_t *clearf;
} mmanager_t;


void mm_fill_stack (mmanager_t * mm) {
  uint8_t *p = calloc!((mm->size - (size_t)(mm->stack - mm->base)), mm->osize);
  while (mm->stack < mm->base + mm->size) {
      mm->clearf (p);
      *(mm->stack) = (void *) p;

      mm->stack++;
      p += mm->osize;
  }
  mm->stack--;
}

void mm_resize_stack (mmanager_t * mm)
{
  mm->size *= 2;
  size_t count = mm->stack - mm->base;
  mm->base = util.xrealloc (mm->base, mm->size * sizeof (void *));
  mm->stack = mm->base + count;
}

/* Creates a new memory manager. */
pub mmanager_t *mm_create(size_t osize, clearf_t *clear_func) {
  mmanager_t *mm = calloc!(1, sizeof (mmanager_t));
  mm->osize = osize;
  mm->clearf = clear_func;
  mm->size = 1024;
  mm->base = calloc!(mm->size, sizeof (void *));
  mm->stack = mm->base;
  mm_fill_stack (mm);
  return mm;
}

/* Free a memory manager */
pub void mm_destroy (mmanager_t * mm) {
  free (mm->stack);
  free (mm);
}

pub void *mm_alloc(mmanager_t * mm) {
  if (mm->stack == mm->base) {
    mm_fill_stack (mm);
  }
  void *p = *(mm->stack);
  mm->stack--;
  return p;
}

pub void mm_free (mmanager_t * mm, void *o) {
  mm->stack++;
  if (mm->stack == mm->base + mm->size) {
    mm_resize_stack (mm);
  }
  *(mm->stack) = o;
  mm->clearf (o);
}
