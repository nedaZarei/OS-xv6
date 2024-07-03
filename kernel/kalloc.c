// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
// defined by kernel.ld.

struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    struct run *freelist;
} kmem;

#define ID_IN_REFCOUNT_ARRAY(pa) (((pa) - KERNBASE) / PGSIZE)
static struct {
    struct spinlock lock;
    int ref_count[ID_IN_REFCOUNT_ARRAY(PHYSTOP)]; //array to store count of references to each page
} kref;

static void set_ref_count(uint64 pa, int count) {
    kref.ref_count[ID_IN_REFCOUNT_ARRAY(pa)] = count;
}

uint64 inc_ref_count(uint64 pa) {
    return ++kref.ref_count[ID_IN_REFCOUNT_ARRAY(pa)];
}

uint64 dec_ref_count(uint64 pa) {
    return --kref.ref_count[ID_IN_REFCOUNT_ARRAY(pa)];
}

static void init_ref_count() {
    for (int i = 0; i < ID_IN_REFCOUNT_ARRAY(PHYSTOP); ++i)
        kref.ref_count[i] = 1;
}
void kref_lock() {
    acquire(&kref.lock);
}

void kref_unlock() {
    release(&kref.lock);
}


void
kinit()
{
    initlock(&kmem.lock, "kmem");
    initlock(&kref.lock, "kref");
    init_ref_count();
    freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
    char *p;
    p = (char*)PGROUNDUP((uint64)pa_start);
    for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
        kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
    struct run *r;

    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    acquire(&kref.lock);
    if (dec_ref_count((uint64)pa) > 0) {
        release(&kref.lock);
        return;
    }
    release(&kref.lock);

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if(r)
        kmem.freelist = r->next;
    release(&kmem.lock);

    if(r){
        set_ref_count((uint64) r, 1);
        memset((char *) r, 7, PGSIZE); // fill with junk
    }
    return (void*)r;
}

long
totalram(void) {
    long total_ram;

    acquire(&kmem.lock);
    total_ram = PHYSTOP - KERNBASE;
    release(&kmem.lock);

    return total_ram;
}

long
freeram(void) {
    struct run *r;
    int free_ram = 0;

    acquire(&kmem.lock);
    r = kmem.freelist;
    while(r) {
        free_ram++;
        r = r->next;
    }
    release(&kmem.lock);

    return free_ram * 4096;
}
