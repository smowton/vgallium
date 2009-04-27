
/******************************************************************************
 * gnttab.c
 *
 * Granting foreign access to our memory reservation.
 *
 * Copyright (c) 2005-2006, Christopher Clark
 * Copyright (c) 2004-2005, K A Fraser
 * Minor tinkerings by C Smowton, 2008
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/seqlock.h>
#include <xen/interface/xen.h>
#include <xen/gnttab.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/synch_bitops.h>
#include <asm/io.h>
#include <xen/interface/memory.h>
#include <xen/driver_util.h>
#include <asm/gnttab_dma.h>

#ifdef HAVE_XEN_PLATFORM_COMPAT_H
#include <xen/platform-compat.h>
#endif

/* External tools reserve first few grant table entries. */
#define NR_RESERVED_ENTRIES 8
#define GNTTAB_LIST_END 0xffffffff
#define ENTRIES_PER_GRANT_FRAME (PAGE_SIZE / sizeof(grant_entry_t))

static grant_ref_t **gnttab_list;
static unsigned int nr_grant_frames;
static unsigned int boot_max_nr_grant_frames;
static int gnttab_free_count;
static grant_ref_t gnttab_free_head;
static DEFINE_SPINLOCK(gnttab_list_lock);

static struct grant_entry *shared;

static struct gnttab_free_callback *gnttab_free_callback_list;

static int gnttab_expand(unsigned int req_entries);

#define RPP (PAGE_SIZE / sizeof(grant_ref_t))
#define gnttab_entry(entry) (gnttab_list[(entry) / RPP][(entry) % RPP])

#define nr_freelist_frames(grant_frames)				\
	(((grant_frames) * ENTRIES_PER_GRANT_FRAME + RPP - 1) / RPP)

static int get_free_entries(int count)
{
	unsigned long flags;
	int ref, rc;
	grant_ref_t head;

	spin_lock_irqsave(&gnttab_list_lock, flags);

	if ((gnttab_free_count < count) &&
	    ((rc = gnttab_expand(count - gnttab_free_count)) < 0)) {
		spin_unlock_irqrestore(&gnttab_list_lock, flags);
		return rc;
	}

	ref = head = gnttab_free_head;
	gnttab_free_count -= count;
	while (count-- > 1)
		head = gnttab_entry(head);
 	gnttab_free_head = gnttab_entry(head);
	gnttab_entry(head) = GNTTAB_LIST_END;

	spin_unlock_irqrestore(&gnttab_list_lock, flags);

	return ref;
}

#define get_free_entry() get_free_entries(1)

static void do_free_callbacks(void)
{
	struct gnttab_free_callback *callback, *next;

	callback = gnttab_free_callback_list;
	gnttab_free_callback_list = NULL;

	while (callback != NULL) {
		next = callback->next;
		if (gnttab_free_count >= callback->count) {
			callback->next = NULL;
			callback->queued = 0;
			callback->fn(callback->arg);
		} else {
			callback->next = gnttab_free_callback_list;
			gnttab_free_callback_list = callback;
		}
		callback = next;
	}
}

static inline void check_free_callbacks(void)
{
	if (unlikely(gnttab_free_callback_list))
		do_free_callbacks();
}

static void put_free_entry(grant_ref_t ref)
{
	unsigned long flags;
	spin_lock_irqsave(&gnttab_list_lock, flags);
	gnttab_entry(ref) = gnttab_free_head;
	gnttab_free_head = ref;
	gnttab_free_count++;
	check_free_callbacks();
	spin_unlock_irqrestore(&gnttab_list_lock, flags);
}

/*
 * Public grant-issuing interface functions
 */

int gnttab_grant_foreign_access(domid_t domid, unsigned long frame,
				int flags)
{
	int ref;

	if (unlikely((ref = get_free_entry()) < 0))
		return -ENOSPC;

	shared[ref].frame = frame;
	shared[ref].domid = domid;
	wmb();
	BUG_ON(flags & (GTF_accept_transfer | GTF_reading | GTF_writing));
	shared[ref].flags = GTF_permit_access | flags;

	return ref;
}
EXPORT_SYMBOL_GPL(gnttab_grant_foreign_access);

void gnttab_grant_foreign_access_ref(grant_ref_t ref, domid_t domid,
				     unsigned long frame, int flags)
{
	shared[ref].frame = frame;
	shared[ref].domid = domid;
	wmb();
	BUG_ON(flags & (GTF_accept_transfer | GTF_reading | GTF_writing));
	shared[ref].flags = GTF_permit_access | flags;
}
EXPORT_SYMBOL_GPL(gnttab_grant_foreign_access_ref);

int gnttab_query_foreign_access(grant_ref_t ref)
{
	u16 nflags;

	nflags = shared[ref].flags;

	return (nflags & (GTF_reading|GTF_writing));
}
EXPORT_SYMBOL_GPL(gnttab_query_foreign_access);

int gnttab_end_foreign_access_ref(grant_ref_t ref)
{
	u16 flags, nflags;

	nflags = shared[ref].flags;
	do {
		if ((flags = nflags) & (GTF_reading|GTF_writing)) {
			printk(KERN_DEBUG "WARNING: g.e. still in use!\n");
			return 0;
		}
	} while ((nflags = synch_cmpxchg_subword(&shared[ref].flags, flags, 0)) !=
		 flags);

	return 1;
}
EXPORT_SYMBOL_GPL(gnttab_end_foreign_access_ref);

int gnttab_try_end_foreign_access(grant_ref_t ref, unsigned long page)
{
  if (gnttab_end_foreign_access_ref(ref)) {
    put_free_entry(ref);
    if (page != 0)
      free_page(page);
    return 1;
  } else {
    return 0;
  }
}
EXPORT_SYMBOL_GPL(gnttab_try_end_foreign_access);

void gnttab_end_foreign_access(grant_ref_t ref, unsigned long page)
{
  // Broken -- leaks the ref and page whenever it can't immediately be freed.
  // Use gnttab_try_end_foreign_access instead and track pages which aren't
  // immediately freeable. Alternatively implement a workqueue/kthread here
  // to clean up things which couldn't be freed right away.

  gnttab_try_end_foreign_access(ref, page);
}
EXPORT_SYMBOL_GPL(gnttab_end_foreign_access);

int gnttab_grant_foreign_transfer(domid_t domid, unsigned long pfn)
{
	int ref;

	if (unlikely((ref = get_free_entry()) < 0))
		return -ENOSPC;
	gnttab_grant_foreign_transfer_ref(ref, domid, pfn);

	return ref;
}
EXPORT_SYMBOL_GPL(gnttab_grant_foreign_transfer);

void gnttab_grant_foreign_transfer_ref(grant_ref_t ref, domid_t domid,
				       unsigned long pfn)
{
	shared[ref].frame = pfn;
	shared[ref].domid = domid;
	wmb();
	shared[ref].flags = GTF_accept_transfer;
}
EXPORT_SYMBOL_GPL(gnttab_grant_foreign_transfer_ref);

unsigned long gnttab_end_foreign_transfer_ref(grant_ref_t ref)
{
	unsigned long frame;
	u16           flags;

	/*
	 * If a transfer is not even yet started, try to reclaim the grant
	 * reference and return failure (== 0).
	 */
	while (!((flags = shared[ref].flags) & GTF_transfer_committed)) {
		if (synch_cmpxchg_subword(&shared[ref].flags, flags, 0) == flags)
			return 0;
		cpu_relax();
	}

	/* If a transfer is in progress then wait until it is completed. */
	while (!(flags & GTF_transfer_completed)) {
		flags = shared[ref].flags;
		cpu_relax();
	}

	/* Read the frame number /after/ reading completion status. */
	rmb();
	frame = shared[ref].frame;
	BUG_ON(frame == 0);

	return frame;
}
EXPORT_SYMBOL_GPL(gnttab_end_foreign_transfer_ref);

unsigned long gnttab_end_foreign_transfer(grant_ref_t ref)
{
	unsigned long frame = gnttab_end_foreign_transfer_ref(ref);
	put_free_entry(ref);
	return frame;
}
EXPORT_SYMBOL_GPL(gnttab_end_foreign_transfer);

void gnttab_free_grant_reference(grant_ref_t ref)
{
	put_free_entry(ref);
}
EXPORT_SYMBOL_GPL(gnttab_free_grant_reference);

void gnttab_free_grant_references(grant_ref_t head)
{
	grant_ref_t ref;
	unsigned long flags;
	int count = 1;
	if (head == GNTTAB_LIST_END)
		return;
	spin_lock_irqsave(&gnttab_list_lock, flags);
	ref = head;
	while (gnttab_entry(ref) != GNTTAB_LIST_END) {
		ref = gnttab_entry(ref);
		count++;
	}
	gnttab_entry(ref) = gnttab_free_head;
	gnttab_free_head = head;
	gnttab_free_count += count;
	check_free_callbacks();
	spin_unlock_irqrestore(&gnttab_list_lock, flags);
}
EXPORT_SYMBOL_GPL(gnttab_free_grant_references);

int gnttab_alloc_grant_references(u16 count, grant_ref_t *head)
{
	int h = get_free_entries(count);

	if (h < 0)
		return -ENOSPC;

	*head = h;

	return 0;
}
EXPORT_SYMBOL_GPL(gnttab_alloc_grant_references);

int gnttab_empty_grant_references(const grant_ref_t *private_head)
{
	return (*private_head == GNTTAB_LIST_END);
}
EXPORT_SYMBOL_GPL(gnttab_empty_grant_references);

int gnttab_claim_grant_reference(grant_ref_t *private_head)
{
	grant_ref_t g = *private_head;
	if (unlikely(g == GNTTAB_LIST_END))
		return -ENOSPC;
	*private_head = gnttab_entry(g);
	return g;
}
EXPORT_SYMBOL_GPL(gnttab_claim_grant_reference);

void gnttab_release_grant_reference(grant_ref_t *private_head,
				    grant_ref_t release)
{
	gnttab_entry(release) = *private_head;
	*private_head = release;
}
EXPORT_SYMBOL_GPL(gnttab_release_grant_reference);

void gnttab_request_free_callback(struct gnttab_free_callback *callback,
				  void (*fn)(void *), void *arg, u16 count)
{
	unsigned long flags;
	spin_lock_irqsave(&gnttab_list_lock, flags);
	if (callback->queued)
		goto out;
	callback->fn = fn;
	callback->arg = arg;
	callback->count = count;
	callback->queued = 1;
	callback->next = gnttab_free_callback_list;
	gnttab_free_callback_list = callback;
	check_free_callbacks();
out:
	spin_unlock_irqrestore(&gnttab_list_lock, flags);
}
EXPORT_SYMBOL_GPL(gnttab_request_free_callback);

void gnttab_cancel_free_callback(struct gnttab_free_callback *callback)
{
	struct gnttab_free_callback **pcb;
	unsigned long flags;

	spin_lock_irqsave(&gnttab_list_lock, flags);
	for (pcb = &gnttab_free_callback_list; *pcb; pcb = &(*pcb)->next) {
		if (*pcb == callback) {
			*pcb = callback->next;
			callback->queued = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&gnttab_list_lock, flags);
}
EXPORT_SYMBOL_GPL(gnttab_cancel_free_callback);

static int grow_gnttab_list(unsigned int more_frames)
{
	unsigned int new_nr_grant_frames, extra_entries, i;
	unsigned int nr_glist_frames, new_nr_glist_frames;

	new_nr_grant_frames = nr_grant_frames + more_frames;
	extra_entries       = more_frames * ENTRIES_PER_GRANT_FRAME;

	nr_glist_frames = nr_freelist_frames(nr_grant_frames);
	new_nr_glist_frames = nr_freelist_frames(new_nr_grant_frames);
	for (i = nr_glist_frames; i < new_nr_glist_frames; i++) {
		gnttab_list[i] = (grant_ref_t *)__get_free_page(GFP_ATOMIC);
		if (!gnttab_list[i])
			goto grow_nomem;
	}

	for (i = ENTRIES_PER_GRANT_FRAME * nr_grant_frames;
	     i < ENTRIES_PER_GRANT_FRAME * new_nr_grant_frames - 1; i++)
		gnttab_entry(i) = i + 1;

	gnttab_entry(i) = gnttab_free_head;
	gnttab_free_head = ENTRIES_PER_GRANT_FRAME * nr_grant_frames;
	gnttab_free_count += extra_entries;

	nr_grant_frames = new_nr_grant_frames;

	check_free_callbacks();

	return 0;
	
grow_nomem:
	for ( ; i >= nr_glist_frames; i--)
		free_page((unsigned long) gnttab_list[i]);
	return -ENOMEM;
}

static unsigned int __max_nr_grant_frames(void)
{
	struct gnttab_query_size query;
	int rc;

	query.dom = DOMID_SELF;

	rc = HYPERVISOR_grant_table_op(GNTTABOP_query_size, &query, 1);
	if ((rc < 0) || (query.status != GNTST_okay))
		return 4; /* Legacy max supported number of frames */

	return query.max_nr_frames;
}

static inline unsigned int max_nr_grant_frames(void)
{
	unsigned int xen_max = __max_nr_grant_frames();

	if (xen_max > boot_max_nr_grant_frames)
		return boot_max_nr_grant_frames;
	return xen_max;
}

#ifdef CONFIG_XEN

static DEFINE_SEQLOCK(gnttab_dma_lock);

#ifdef CONFIG_X86
static int map_pte_fn(pte_t *pte, struct page *pmd_page,
		      unsigned long addr, void *data)
{
	unsigned long **frames = (unsigned long **)data;

	set_pte_at(&init_mm, addr, pte, pfn_pte_ma((*frames)[0], PAGE_KERNEL));
	(*frames)++;
	return 0;
}

static int unmap_pte_fn(pte_t *pte, struct page *pmd_page,
			unsigned long addr, void *data)
{

	set_pte_at(&init_mm, addr, pte, __pte(0));
	return 0;
}

void *arch_gnttab_alloc_shared(unsigned long *frames)
{
	struct vm_struct *area;
	area = alloc_vm_area(PAGE_SIZE * max_nr_grant_frames());
	BUG_ON(area == NULL);
	return area->addr;
}
#endif /* CONFIG_X86 */

static int gnttab_map(unsigned int start_idx, unsigned int end_idx)
{
	struct gnttab_setup_table setup;
	unsigned long *frames;
	unsigned int nr_gframes = end_idx + 1;
	int rc;

	frames = kmalloc(nr_gframes * sizeof(unsigned long), GFP_ATOMIC);
	if (!frames)
		return -ENOMEM;

	setup.dom        = DOMID_SELF;
	setup.nr_frames  = nr_gframes;
	set_xen_guest_handle(setup.frame_list, frames);

	rc = HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup, 1);
	if (rc == -ENOSYS) {
		kfree(frames);
		return -ENOSYS;
	}

	BUG_ON(rc || setup.status);

	if (shared == NULL)
		shared = arch_gnttab_alloc_shared(frames);

#ifdef CONFIG_X86
	rc = apply_to_page_range(&init_mm, (unsigned long)shared,
				 PAGE_SIZE * nr_gframes,
				 map_pte_fn, &frames);
	BUG_ON(rc);
	frames -= nr_gframes; /* adjust after map_pte_fn() */
#endif /* CONFIG_X86 */

	kfree(frames);

	return 0;
}

static void gnttab_page_free(struct page *page)
{
	ClearPageForeign(page);
	gnttab_reset_grant_page(page);
	put_page(page);
}

/*
 * Must not be called with IRQs off.  This should only be used on the
 * slow path.
 *
 * Copy a foreign granted page to local memory.
 */
int gnttab_copy_grant_page(grant_ref_t ref, struct page **pagep)
{
	struct gnttab_unmap_and_replace unmap;
	mmu_update_t mmu;
	struct page *page;
	struct page *new_page;
	void *new_addr;
	void *addr;
	paddr_t pfn;
	maddr_t mfn;
	maddr_t new_mfn;
	int err;

	page = *pagep;
	if (!get_page_unless_zero(page))
		return -ENOENT;

	err = -ENOMEM;
	new_page = alloc_page(GFP_ATOMIC | __GFP_NOWARN);
	if (!new_page)
		goto out;

	new_addr = page_address(new_page);
	addr = page_address(page);
	memcpy(new_addr, addr, PAGE_SIZE);

	pfn = page_to_pfn(page);
	mfn = pfn_to_mfn(pfn);
	new_mfn = virt_to_mfn(new_addr);

	write_seqlock(&gnttab_dma_lock);

	/* Make seq visible before checking page_mapped. */
	smp_mb();

	/* Has the page been DMA-mapped? */
	if (unlikely(page_mapped(page))) {
		write_sequnlock(&gnttab_dma_lock);
		put_page(new_page);
		err = -EBUSY;
		goto out;
	}

	if (!xen_feature(XENFEAT_auto_translated_physmap))
		set_phys_to_machine(pfn, new_mfn);

	gnttab_set_replace_op(&unmap, (unsigned long)addr,
			      (unsigned long)new_addr, ref);

	err = HYPERVISOR_grant_table_op(GNTTABOP_unmap_and_replace,
					&unmap, 1);
	BUG_ON(err);
	BUG_ON(unmap.status);

	write_sequnlock(&gnttab_dma_lock);

	if (!xen_feature(XENFEAT_auto_translated_physmap)) {
		set_phys_to_machine(page_to_pfn(new_page), INVALID_P2M_ENTRY);

		mmu.ptr = (new_mfn << PAGE_SHIFT) | MMU_MACHPHYS_UPDATE;
		mmu.val = pfn;
		err = HYPERVISOR_mmu_update(&mmu, 1, NULL, DOMID_SELF);
		BUG_ON(err);
	}

	new_page->mapping = page->mapping;
	new_page->index = page->index;
	set_bit(PG_foreign, &new_page->flags);
	*pagep = new_page;

	SetPageForeign(page, gnttab_page_free);
	page->mapping = NULL;

out:
	put_page(page);
	return err;
}
EXPORT_SYMBOL_GPL(gnttab_copy_grant_page);

void gnttab_reset_grant_page(struct page *page)
{
	init_page_count(page);
	reset_page_mapcount(page);
}
EXPORT_SYMBOL_GPL(gnttab_reset_grant_page);

/*
 * Keep track of foreign pages marked as PageForeign so that we don't
 * return them to the remote domain prematurely.
 *
 * PageForeign pages are pinned down by increasing their mapcount.
 *
 * All other pages are simply returned as is.
 */
void __gnttab_dma_map_page(struct page *page)
{
	unsigned int seq;

	if (!is_running_on_xen() || !PageForeign(page))
		return;

	do {
		seq = read_seqbegin(&gnttab_dma_lock);

		if (gnttab_dma_local_pfn(page))
			break;

		atomic_set(&page->_mapcount, 0);

		/* Make _mapcount visible before read_seqretry. */
		smp_mb();
	} while (unlikely(read_seqretry(&gnttab_dma_lock, seq)));
}

int gnttab_resume(void)
{
	if (max_nr_grant_frames() < nr_grant_frames)
		return -ENOSYS;
	return gnttab_map(0, nr_grant_frames - 1);
}

int gnttab_suspend(void)
{
#ifdef CONFIG_X86
	apply_to_page_range(&init_mm, (unsigned long)shared,
			    PAGE_SIZE * nr_grant_frames,
			    unmap_pte_fn, NULL);
#endif
	return 0;
}

#else /* !CONFIG_XEN */

#include <platform-pci.h>

static unsigned long resume_frames;

static int gnttab_map(unsigned int start_idx, unsigned int end_idx)
{
	struct xen_add_to_physmap xatp;
	unsigned int i = end_idx;

	/* Loop backwards, so that the first hypercall has the largest index,
	 * ensuring that the table will grow only once.
	 */
	do {
		xatp.domid = DOMID_SELF;
		xatp.idx = i;
		xatp.space = XENMAPSPACE_grant_table;
		xatp.gpfn = (resume_frames >> PAGE_SHIFT) + i;
		if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp))
			BUG();
	} while (i-- > start_idx);

	return 0;
}

int gnttab_resume(void)
{
	unsigned int max_nr_gframes, nr_gframes;

	nr_gframes = nr_grant_frames;
	max_nr_gframes = max_nr_grant_frames();
	if (max_nr_gframes < nr_gframes)
		return -ENOSYS;

	if (!resume_frames) {
		resume_frames = alloc_xen_mmio(PAGE_SIZE * max_nr_gframes);
		shared = ioremap(resume_frames, PAGE_SIZE * max_nr_gframes);
		if (shared == NULL) {
			printk("error to ioremap gnttab share frames\n");
			return -1;
		}
	}

	gnttab_map(0, nr_gframes - 1);

	return 0;
}

#endif /* !CONFIG_XEN */

static int gnttab_expand(unsigned int req_entries)
{
	int rc;
	unsigned int cur, extra;

	cur = nr_grant_frames;
	extra = ((req_entries + (ENTRIES_PER_GRANT_FRAME-1)) /
		 ENTRIES_PER_GRANT_FRAME);
	if (cur + extra > max_nr_grant_frames())
		return -ENOSPC;

	if ((rc = gnttab_map(cur, cur + extra - 1)) == 0)
		rc = grow_gnttab_list(extra);

	return rc;
}

int __devinit gnttab_init(void)
{
	int i;
	unsigned int max_nr_glist_frames, nr_glist_frames;
	unsigned int nr_init_grefs;

	if (!is_running_on_xen())
		return -ENODEV;

	nr_grant_frames = 1;
	boot_max_nr_grant_frames = __max_nr_grant_frames();

	/* Determine the maximum number of frames required for the
	 * grant reference free list on the current hypervisor.
	 */
	max_nr_glist_frames = nr_freelist_frames(boot_max_nr_grant_frames);

	gnttab_list = kmalloc(max_nr_glist_frames * sizeof(grant_ref_t *),
			      GFP_KERNEL);
	if (gnttab_list == NULL)
		return -ENOMEM;

	nr_glist_frames = nr_freelist_frames(nr_grant_frames);
	for (i = 0; i < nr_glist_frames; i++) {
		gnttab_list[i] = (grant_ref_t *)__get_free_page(GFP_KERNEL);
		if (gnttab_list[i] == NULL)
			goto ini_nomem;
	}

	if (gnttab_resume() < 0)
		return -ENODEV;

	nr_init_grefs = nr_grant_frames * ENTRIES_PER_GRANT_FRAME;

	for (i = NR_RESERVED_ENTRIES; i < nr_init_grefs - 1; i++)
		gnttab_entry(i) = i + 1;

	gnttab_entry(nr_init_grefs - 1) = GNTTAB_LIST_END;
	gnttab_free_count = nr_init_grefs - NR_RESERVED_ENTRIES;
	gnttab_free_head  = NR_RESERVED_ENTRIES;

	return 0;

 ini_nomem:
	for (i--; i >= 0; i--)
		free_page((unsigned long)gnttab_list[i]);
	kfree(gnttab_list);
	return -ENOMEM;
}

#ifdef CONFIG_XEN
core_initcall(gnttab_init);
#endif
