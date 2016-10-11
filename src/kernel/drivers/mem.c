/* Physical memory will be divided in frame MEM_FRAME_SIZE large, though you
 * can expect this value to be 4K always.
 *
 *  ------------------- 0xffffffff (4G)
 *  |    USER SPACE   |
 *  |-----------------| 0x00200000 (3M)
 *  |  KERNEL STACK   |
 *  |  KERNEL HEAP    |
 *  |-----------------| 0x00100000 (1M)
 *  |     UNUSED      |
 *  |-----------------| 0x00001000 + sizeof(KERNEL_TEXT)
 *  |   KERNEL TEXT   |
 *  |-----------------| 0x00001000 (4K)
 *  |    RESERVED     |
 *  ------------------- 0x00000000
 *
 * This means we need at least 3M of RAM to run and, for the time being, we
 * won't be able to run on systems with reserved regions in the [1M,3M] range.
 *
 * The reserved area contains some data prepared during the earliest stage of
 * this kernel, in particular it contains the GDT in use when the memory
 * initialization routines start. We'll only use five segments: two for the
 * kernel, two for user space, and one to hold a single TSS. Though only the
 * kernel segments were prepared, we know the GDT has enough space to hold
 * the five descriptors we need.
 *
 * TODO: Fill the remaining three GDT entries during initialization.
 * TODO: Should we provide a GDT-related API to the kernel?
 *
 * We must also set the kernel stack. However, we can't move the stack and
 * expect everything will work just fine, can we? Thus, this should be the
 * last step of memory initialization, but we can't do it here. The kernel
 * main routine will have to handle this properly somehow. However, we still
 * lack the ability to control what the stack size is, thus we can eventually
 * run into data corruption in kernel space :( if the heap and the stack
 * reach each other. I'm not sure about how to even detect it without virtual
 * memory.
 *
 * TODO: Think of a way to detect the clash between kernel's stack and heap.
 *
 * The interrupt table, since it's statically generated, will stay as part of
 * the kernel static data so we won't change it here. We could do it may the
 * need arise.
 *
 * We'll use a bitmap for physical allocation. In a 32-bit architecture we can
 * have up to 2^32 / 2^12 = 2^20 page frames, so we'll need 2^20 entries in our
 * bitmap, which results in a 2^20 / 2^3 = 2^17 = 128K long bitmap. However,
 * since only used/not-used is not enough, we'll need more bits to represent
 * a frame. Actually, I'm thinking of two bits, so this will give us a 256K
 * bitmap. Huge, I know, but it's the easiest method. Also, the actual size of
 * the bitmap depends on the actual size of installed memory and, what do 256K
 * do when compared to 4G? This bitmap will start at the very beginning of the
 * kernel's heap, which means we will only have (1M - 256K) in the worst case
 * scenario to hold both the kernel's heap and stack. Of course this seems
 * very restricted, but our kernel is not too deep right now. And, if we ever
 * run out of memory, we can think of a better policy to distribute memory.
 * But later, not now.
 */

#include <mem.h>
#include <string.h>
#include <fb.h>

void kalloc_init();

/*****************************************************************************
 * Physical allocator                                                        *
 *****************************************************************************/

static u64 mem_total_frames;      /* Keeps the actual number of pages in main
                                  * memory. */

/* During the initial, real-mode load of the kernel we used INT 0x12,
 * AX = 0xe820 to get a memory map, which we placed as a continuos list of
 * region descriptors which will be passed to the kernel as an argument. Thus,
 * during memory initialization, this is what we'll read. These are the
 * BIOS-related things we need. */
#define MEM_BIOS_MEM_MAP_REGION_AVAILABLE   0x00000001
#define MEM_BIOS_MEM_MAP_REGION_RESERVED    0x00000002
#define MEM_BIOS_MEM_MAP_ACPI_RECLAIM       0x00000003
#define MEM_BIOS_MEM_MAP_ACPI_NVS           0x00000004
/* The entries returned by INT 0x12 AX = 0xe820 are of this form. */
struct mem_bios_mmap_entry {
  u64 base;
  u64 size;
  u32 type;
}__attribute__((__packed__));

/* Our physical page frame allocator will use a bitmap. Each entry will be
 * two-bits wide and will represent a MEM_FRAME_SIZE-large page. */
#define MEM_BITMAP_ADDR                       0x00100000 /* 1M, i.e. exactly at
                                                          * the beginning of
                                                          * kernel's heap. */
#define MEM_BITMAP_BITS_PER_ENTRY             2 /* These two are closely */
#define MEM_BITMAP_ENTRIES_PER_BYTE           4 /* related. */

#define MEM_BITMAP_ENTRY_FREE                 0x00
#define MEM_BITMAP_ENTRY_USED                 0x01
#define MEM_BITMAP_ENTRY_RESERVED             0x02
#define MEM_BITMAP_ENTRY_RESERVED2            0x03

/* Bitmap handling routines */
void mem_bitmap_set_entry(u32 frame, u8 status) {
  u8 pack, *bm;
  bm = (u8*)MEM_BITMAP_ADDR;
  status = status & 0x03;
  pack = bm[frame / MEM_BITMAP_ENTRIES_PER_BYTE];
  pack = pack & ~( 0x03 << ((frame % MEM_BITMAP_ENTRIES_PER_BYTE) * 2) );
  pack = pack | ( status << ((frame % MEM_BITMAP_ENTRIES_PER_BYTE) * 2) );
  bm[frame / MEM_BITMAP_ENTRIES_PER_BYTE] = pack;
}

u8 mem_bitmap_get_entry(u32 frame) {
  u8 pack, *bm;
  bm = (u8*)MEM_BITMAP_ADDR;
  pack = bm[frame / MEM_BITMAP_ENTRIES_PER_BYTE];
  return pack >> ((frame % MEM_BITMAP_ENTRIES_PER_BYTE) * 2) & 0x03;
}

/* Intializes memory. It first reads the memory map obtained from the BIOS
 * and then creates a memory map with that info. It also intializes the bitmap
 * to keep track of all pages in the main memory. TODO: Fill the GDT. */
int mem_init(void *gdt_base /* __attribute__((unused)) */, void *mem_map) {
  struct mem_bios_mmap_entry *e;
  u64 max_addr;
  u64 first_frame, last_frame;

  /* Scan the memory map obtained from BIOS and the total number of frames. */
  for (max_addr = 0, e = (struct mem_bios_mmap_entry *)mem_map;
       e->size != 0 || e->base != 0 || e->type != 0;
       e++) {
    if (e->base + e->size > max_addr)
      max_addr = e->base + e->size;
  }
  mem_total_frames = max_addr / MEM_FRAME_SIZE;

  /* Verify we have enough space to hold the bitmap. */
  for (e = (struct mem_bios_mmap_entry*)mem_map;
       e->size != 0 || e->base != 0 || e->type != 0;
       e++) {
    if (e->type == MEM_BIOS_MEM_MAP_REGION_AVAILABLE) {
      if (e->base <= MEM_BITMAP_ADDR            &&
          e->base + e->size >= MEM_BITMAP_ADDR +
                               mem_total_frames * MEM_BITMAP_BITS_PER_ENTRY) {
        break;
      }
    }
  }
  if (e->size == 0 && e->base == 0 && e->type == 0) {
    return -1;
  }

  /* Set initial configuration. */
  /* Most memory will be free.  */
  memset((void *)MEM_BITMAP_ADDR,
         0,
         mem_total_frames * MEM_BITMAP_BITS_PER_ENTRY);
  /* Then, set the configuration obtained from the BIOS. Since we won't
   * handle ACPI at all, we won't reclaim the memory either. */
  for (e = (struct mem_bios_mmap_entry*)mem_map;
       e->size != 0 || e->base != 0 || e->type != 0;
       e++) {
    if (e->type == MEM_BIOS_MEM_MAP_REGION_AVAILABLE)
      continue;
    for (first_frame = e->base / MEM_FRAME_SIZE,
         last_frame = (e->base + e->size) / MEM_FRAME_SIZE;
         first_frame <= last_frame;
         first_frame++) {
      mem_bitmap_set_entry(first_frame, MEM_BITMAP_ENTRY_RESERVED);
      /* This is just paranoia, but since I've run into this before I prefer
       * to double check. */
      if (mem_bitmap_get_entry(first_frame) != MEM_BITMAP_ENTRY_RESERVED) {
        return -1;
      }
    }
  }
  /* Once done, let's reserve the memory we know we are using. However, since
   * we won't ever free it, let's mark it as reserved. */
  for (first_frame = 0,
       last_frame = (MEM_BITMAP_ADDR + mem_total_frames / MEM_BITMAP_ENTRIES_PER_BYTE) / MEM_FRAME_SIZE;
       first_frame <= last_frame;
       first_frame++) {
    mem_bitmap_set_entry(first_frame, MEM_BITMAP_ENTRY_RESERVED);
    /* And again, paranoia */
    if (mem_bitmap_get_entry(first_frame) != MEM_BITMAP_ENTRY_RESERVED) {
      return -1;
    }
  }

  /* Finally, let's intialize the logical allocator. */
  kalloc_init();

  return 0;
}

/* Tries to find an amount of count contigous free pages. Since we're not using
 * pagination at all we can't just find this space anywhere in the physical
 * memory, but we must know what are the limits we must respect. Frame 0 will
 * be always reserved, therefore we can use 0 to mark certain situations. */
void * mem_allocate_frames(u32 count, u32 first, u32 last) {
  /* This is the simplest, dummiest way to do this. */
  u32 f, free_f;

  if (last == 0 || last > mem_total_frames)
    last = mem_total_frames;

  for (free_f = 0, f = first; f < last; f++) {
    if (mem_bitmap_get_entry(f) == MEM_BITMAP_ENTRY_FREE)
      free_f++;
    else
      free_f = 0;
    if (free_f == count) {
      /* Good, we found a free spot :) */
      while (free_f > 0) {
        mem_bitmap_set_entry(f - free_f + 1, MEM_BITMAP_ENTRY_USED);
        free_f--;
      }
      /* Now, we return the address to the first frame. */
      return (void *)((f - count + 1) * MEM_FRAME_SIZE);
    }
  }
  return NULL;
}

/* Marks count frames from first_frame on as free. Of course, if any of the
 * frames in between are reserved we won't change them. Actually, if that
 * happens this call should be wrong. */
void mem_release_frames(void *addr, u32 count) {
  u32 f, last;

  f = (u32)addr / MEM_FRAME_SIZE;
  if (f >= mem_total_frames)
    return;

  last = f + count;
  if (last > mem_total_frames)
    last = mem_total_frames;

  for (; f < last; f++) {
    if (mem_bitmap_get_entry(f) == MEM_BITMAP_ENTRY_USED) {
      // fb_printf("fr: %d\n", f);
      mem_bitmap_set_entry(f, MEM_BITMAP_ENTRY_FREE);
    }
  }
}

void mem_inspect() {
  u8 v, w;
  u32 f, r_start;

  for (f = 0, w = 4 /* Invalid status */; f < mem_total_frames; f++) {
    v = mem_bitmap_get_entry(f);
    if (v != w) {
      if (w < 4)
        fb_printf("[%d,%d] = %d\n", r_start, f - 1, w);
      r_start = f;
      w = v;
    }
  }
}

/*****************************************************************************
 * Logical allocator                                                         *
 *****************************************************************************/

/* Our logical allocator will use a double linked list. Each entry will contain
 * the amount of memory reserved by the entry and its status. Both free and
 * used entries will be linked together in the same list. */
struct mem_entry {
  struct mem_entry *next;       /* Pointer to next entry */
  struct mem_entry *prev;       /* Pointer to prev entry */
  u32 size;                     /* Memory associated to this entry in
                                   sizeof(struct mem_entry) units. */
  u32 flags;                    /* Status of the entry. Actually, this is
                                   pretty wasteful but keeps alignment to
                                   8-bytes boundaries. */
}__attribute__((__packed__));

#define MEM_ALLOC_ENTRY_NULL                0x00000000
#define MEM_ALLOC_ENTRY_FREE                0x00000001
#define MEM_ALLOC_ENTRY_USED                0x00000002

static struct mem_entry mem_head;

/* Initializes the logical allocator. */
void kalloc_init() {
  mem_head.next = NULL;
  mem_head.prev = NULL;
  mem_head.size = 0;
  mem_head.flags = MEM_ALLOC_ENTRY_NULL;
}

/* Allocates memory in a malloc fashion for the kernel to use. */
void * kalloc(u32 bytes) {
  struct mem_entry *e, *n;
  u32 units, frames;

  units = bytes / sizeof(struct mem_entry);
  if (bytes % sizeof(struct mem_entry) > 0)
    units += 1;

  e = &mem_head;
  while (1) {
    if (e->flags == MEM_ALLOC_ENTRY_FREE) {
      /* The reason for checking if there's an exact match with units + 1 is
       * that otherwise we'll be creating a zero-size entry which we won't
       * be able to use if we don't. */
      if (e->size == units || e->size == units + 1) {
        e->flags = MEM_ALLOC_ENTRY_USED;
        return (void*)(e + 1);
      }
      /* We found a hole, so we can make it fit here. */
      if (e->size > units + 1) {
        n = e + units + 1;      /* New entry's location. */
        n->flags = MEM_ALLOC_ENTRY_FREE;
        n->size = e->size - units - 1;
        n->next = e->next;      /* Update the list. */
        if (n->next != NULL)
          n->next->prev = n;
        n->prev = e;
        e->next = n;
        e->size = units;
        e->flags = MEM_ALLOC_ENTRY_USED;
        return (void *)(e + 1);
      }
    }
    if (e->next != NULL) {
      e = e->next;
      continue;
    }
    /* Only if we reached the end of the list. */
    frames = ((units + 1) * sizeof(struct mem_entry)) / MEM_FRAME_SIZE;
    if ((units + 1) * sizeof(struct mem_entry) % MEM_FRAME_SIZE > 0)
      frames += 1;
    n = (struct mem_entry*)mem_allocate_frames(frames,
                                               MEM_KERNEL_FIRST_FRAME,
                                               MEM_USER_FIRST_FRAME);
    if (n == NULL) /* There's no free space :( */
      return NULL;
    e->next = n;
    n->prev = e;
    n->next = NULL;
    n->flags = MEM_ALLOC_ENTRY_FREE;
    n->size = (frames * MEM_FRAME_SIZE) / sizeof(struct mem_entry) - 1;
    e = e->next;
  }
}

void kfree(void * ptr) {
  struct mem_entry *e, *f;

  if (ptr == NULL)
    return; /* Just to avoid silly mistakes. */

  e = &mem_head;
  while (1) {
    if (e->next == NULL) {
      /* We reached the end of the list without finding anything to free.
       * Wrong pointer maybe? */
      return;
    }
    if (e->next + 1 == ptr) {
      /* Good, we found it. */
      f = e->next;
      if (f->flags == MEM_ALLOC_ENTRY_FREE)
        /* What? Double free? */
        return;
      /* Free! */
      f->flags = MEM_ALLOC_ENTRY_FREE;

      if (f->next != NULL &&
          f->next->flags == MEM_ALLOC_ENTRY_FREE) {
        /* If the entry after the one being freed is also free, join them. */
        f->size += f->next->size + 1;
        f->next = f->next->next;
        if (f->next != NULL)
          f->next->prev = f;
      }

      if (e->flags == MEM_ALLOC_ENTRY_FREE) {
        /* If e is also free, then join it with f */
        e->size += f->size + 1;
        e->next = f->next;
        if (e->next != NULL)
          e->next->prev = e;
      }
      return;
    }
    e = e->next;
  }
}

void mem_inspect_alloc() {
  struct mem_entry *e;
  e = &mem_head;
  while (e != NULL) {
    fb_printf("entry { flags: %d, size: %d, prev: %d, next: %d }\n",
              e->flags, e->size, e->prev, e->next);
    e = e->next;
  }
}
