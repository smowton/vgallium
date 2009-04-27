

#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <xen/gnttab.h>
#include <asm/hypervisor.h>
#include <xen/balloon.h>
#include <xen/evtchn.h>
#include <xen/driver_util.h>

#include <linux/types.h>
#include <xen/public/gntmem.h>

#define DBG(args...) printk(KERN_ALERT ); printk(args)

#define DRIVER_AUTHOR "Chris Smowton <cs448@cl.cam.ac.uk>"
#define DRIVER_DESC   "User-space page granting driver"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

struct granted_page {

  grant_ref_t grant;
  struct page* page;

};

/* Private data structure, which is stored in the file pointer for files
 * associated with this device.
 */
struct gntmem_state {
  
  /* Array of grant information. NULL when the device is uninitialised */
  struct granted_page* pages;
  int npages;

  /* Domain to whom pages will be granted */
  domid_t domain;

  /* Semaphore used to protect pages and npages. */
  struct semaphore sem;

  /* Refcount */
  int refs;

};

struct cleanup_list_entry {

  struct granted_page* pages;
  int npages;

  struct cleanup_list_entry* next;

};

/* Module lifecycle operations. */
static int __init gntmem_init(void);
static void __exit gntmem_exit(void);

module_init(gntmem_init);
module_exit(gntmem_exit);

/* File operations. */
static int gntmem_open(struct inode *inode, struct file *flip);
static int gntmem_release(struct inode *inode, struct file *flip);
static int gntmem_mmap(struct file *flip, struct vm_area_struct *vma);
static long gntmem_ioctl(struct file *flip,
			 unsigned int cmd, unsigned long arg);

static const struct file_operations gntmem_fops = {
	.owner = THIS_MODULE,
	.open = gntmem_open,
	.release = gntmem_release,
	.mmap = gntmem_mmap,
	.unlocked_ioctl = gntmem_ioctl
};

/* VM operations. */
static void gntmem_vma_close(struct vm_area_struct* vma);
static void gntmem_vma_open(struct vm_area_struct* vma);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
static int gntmem_handle_fault(struct vm_area_struct* vma, struct vm_fault* vmf);
#endif
/* Keep the nopage method even if fault is available: fault just defers to nopage */
static struct page* gntmem_handle_nopage(struct vm_area_struct*, unsigned long, int*);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void try_page_disposal(struct work_struct*);
#else
static void try_page_disposal(void*);
#endif

static struct vm_operations_struct gntmem_vmops = {
  .close = gntmem_vma_close,
  .open = gntmem_vma_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
  .fault = gntmem_handle_fault,
#else
  .nopage = gntmem_handle_nopage,
#endif
};

/* Global variables. */

/* The driver major number, for use when unregistering the driver. */
static int gntmem_major;

/* A list of sets of granted_pages which need cleaning up, to be periodically
   attempted by a workqueue task */
static struct cleanup_list_entry* cleanup_list_head = 0;

/* A semaphore to guard maniuplations of that list */
DECLARE_MUTEX(cleanup_list_sem);

/* A work item which can be queued to attempt cleanup */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
struct delayed_work cleanup_work;
#else
struct work_struct cleanup_work;
#endif

/* Is the work currently pending? */
int work_scheduled = 0;

// Delay 10 seconds between attempts at page cleanup
#define WORK_DELAY HZ * 10

#define GNTMEM_NAME "gntmem"

/* Helper functions. */
static void atomic_ref(struct gntmem_state* state) {

  int raise = 1;

  if(down_interruptible(&state->sem))
    raise = 0;

  // Ignore error -- better to have a correct reference count and potential
  // race than an incorrect count
  
  state->refs++;

  if(raise)
    up(&state->sem);

}

static void atomic_deref(struct gntmem_state* state) {

  int raise = 1;

  if(down_interruptible(&state->sem))
    raise = 0;

  // Ignore error -- better to have the refcount possibly wrong than certainly
  
  state->refs--;

  if(raise)
    up(&state->sem);

  if(!state->refs)
    kfree(state);

}

static int init_private_data(struct gntmem_state *priv) {

  priv->pages = 0;
  priv->npages = 0;
  priv->refs = 1;
  priv->domain = 0;

  init_MUTEX(&priv->sem);

  return 0;

}

/* Interface functions. */

/* Initialises the driver. Called when the module is loaded. */
static int __init gntmem_init(void)
{
	struct class *class;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
	struct device *device;
#else
	struct class_device *device;
#endif

	if (!is_running_on_xen()) {
		printk(KERN_ERR "You must be running Xen to use gntmem\n");
		return -ENODEV;
	}

	gntmem_major = register_chrdev(0, GNTMEM_NAME, &gntmem_fops);
	if (gntmem_major < 0)
	{
		printk(KERN_ERR "Could not register gntmem device\n");
		return -ENOMEM;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
	INIT_DELAYED_WORK(&cleanup_work, try_page_disposal);
#else
	INIT_WORK(&cleanup_work, try_page_disposal, 0);
#endif

	/* Note that if the sysfs code fails, we will still initialise the
	 * device, and output the major number so that the device can be
	 * created manually using mknod.
	 */
	if ((class = get_xen_class()) == NULL) {
		printk(KERN_ERR "Error setting up xen_class\n");
		printk(KERN_ERR "gntmem created with major number = %d\n", 
		       gntmem_major);
		return 0;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
	device = device_create(class, NULL, MKDEV(gntmem_major, 0), NULL, GNTMEM_NAME);
#else
	device = class_device_create(class, NULL, MKDEV(gntmem_major, 0),
				     NULL, GNTMEM_NAME);
#endif
	if (IS_ERR(device)) {
		printk(KERN_ERR "Error creating gntmem device in xen_class\n");
		printk(KERN_ERR "gntmem created with major number = %d\n",
		       gntmem_major);
		return 0;
	}

	DBG("Successfully initialised gntmem\n");

	return 0;
}

/* Cleans up and unregisters the driver. Called when the driver is unloaded.
 */
static void __exit gntmem_exit(void)
{
  struct class *class;
  DBG("Unloading gntmem\n");
  if ((class = get_xen_class()) != NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
    device_destroy(class, MKDEV(gntmem_major, 0));
#else
    class_device_destroy(class, MKDEV(gntmem_major, 0));
#endif
  }
  unregister_chrdev(gntmem_major, GNTMEM_NAME);
}

/* Called when the device is opened. */
static int gntmem_open(struct inode *inode, struct file *flip)
{
  struct gntmem_state *private_data;

  DBG("Opened gntmem\n");

  try_module_get(THIS_MODULE);

  /* Allocate space for the per-instance private data. */
  private_data = kmalloc(sizeof(*private_data), GFP_KERNEL);
  if (!private_data)
    goto nomem_out;
  
  init_private_data(private_data);
  
  flip->private_data = private_data;

  DBG("Open succeeded\n");

  return 0;

nomem_out:
  return -ENOMEM;
}

int try_ungrant_and_free_pages_now(struct granted_page* pages, int npages) {

  int can_free = 1;
  int i;

  DBG("Trying to ungrant-and-free block at %p, length %d\n", pages, npages);

  for(i = 0; i < npages; i++) {
    DBG("Trying page #%d: grant=%u, page=%p\n", i, pages[i].grant, pages[i].page);
    if(pages[i].grant >= 0) {
      if(gnttab_try_end_foreign_access(pages[i].grant, 0)) {
	DBG("Successfully ended foreign access\n");
	pages[i].grant = -1;
      }
      else {
	DBG("Failed to end foreign access\n");
	can_free = 0;
      }
    }
    if((pages[i].grant < 0) && pages[i].page) {
      DBG("Freeing page at %p\n", pages[i].page);
      put_page(pages[i].page);
      pages[i].page = 0;
    }
  }
  if(can_free) {
    DBG("All freeing succeeded; releasing the page-grant-list at %p\n", pages);
    kfree(pages);
    return 1;
  }
  else {
    DBG("At least one page seems to have been impossible to free right now\n");
    // Signal this needs to be tried again
    return 0;
  }

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void try_page_disposal(struct work_struct* ignored) {
#else
static void try_page_disposal(void* ignored) {
#endif

  // Try to dispose of all pending pages, and reschedule myself if the list
  // does not become empty.

  DBG("Entered workqueue cleanup task\n");

  int raise = 1;
  int reschedule = 0;

  int ret = down_interruptible(&cleanup_list_sem);

  DBG("Got the cleanup semaphore\n");

  if(ret == -1) {
    printk(KERN_INFO "Workqueue task interrupted; giving up for now and rescheduling\n");
    reschedule = 1;
    raise = 0;
  }
  else {
    struct cleanup_list_entry** current_entry = &cleanup_list_head;
    
    while(*current_entry) {
      struct cleanup_list_entry* totry = *current_entry;
      DBG("Trying cleanup list entry %p specifying grant-list %p, length %d\n",
	  totry, totry->pages, totry->npages);
      int success = try_ungrant_and_free_pages_now(totry->pages, totry->npages);
      if(success) {
	DBG("Cleanup succeeded: deleting list entry\n");
	*current_entry = totry->next;
	kfree(totry);
      }
      else {
	DBG("Cleanup failed: retaining list entry\n");
	current_entry = &(totry->next);
	reschedule = 1;
      }
    }
  }

  if(reschedule) {

    DBG("The workqueue task is rescheduled\n");

    // Re-schedule the same work
    schedule_delayed_work(&cleanup_work, WORK_DELAY);

  }
  else {

    DBG("No rescheduling required\n");

    work_scheduled = 0;
    module_put(THIS_MODULE);

  }

  if(raise)
    up(&cleanup_list_sem);

}

static int add_page_disposal_entry(struct granted_page* pages, int npages) {

  int ret;  

  DBG("Adding a page list for later disposal: %p, length %d\n", pages, npages);

  struct cleanup_list_entry* newentry = kmalloc(sizeof(struct cleanup_list_entry), GFP_KERNEL);
  if(!newentry) {
    printk(KERN_ERR "Couldn't allocate a new cleanup_list_entry\n");
    return 0;
  }

  newentry->pages = pages;
  newentry->npages = npages;

  DBG("Waiting on the cleanup semaphore...\n");

  ret = down_interruptible(&cleanup_list_sem);
  if(ret != 0) {
    printk(KERN_ERR "Interrupted getting cleanup list semaphore\n");
    return 0;
  }

  DBG("Got the cleanup semaphore\n");

  newentry->next = cleanup_list_head;
  cleanup_list_head = newentry;

  if(!work_scheduled) {
    DBG("Found work not currently scheduled\n");
    try_module_get(THIS_MODULE);
    // Prevent the module from unloading whilst the work is pending
    schedule_delayed_work(&cleanup_work, WORK_DELAY);
    work_scheduled = 1;
  }
  else {
    DBG("Found cleanup work already scheduled\n");
  }

  up(&cleanup_list_sem);

  return 1;

}

static void try_ungrant_and_free_pages(struct granted_page* pages, int npages) {

  DBG("Trying to ungrant and free page-list %p, length %d\n", pages, npages);

  if(!try_ungrant_and_free_pages_now(pages, npages)) {
    DBG("Failed to immediately free; adding to pending list\n");
    // Add to list of pages that need disposing of
    if(!add_page_disposal_entry(pages, npages)) {
      DBG("Failed to add to the list; giving up and leaking\n");
      // Couldn't add to the list; I give up. Just free the page vector
      // and warn the user as to what has happened.
      kfree(pages);
      printk("*** gntmap: Unable to add a set of pages to list of pages for disposal. Up to %d pages may be leaked\n", npages);
    }
    else {
      DBG("Successfully added to the list\n");
    }
  }
  else {
    DBG("Successfully freed synchronously\n");
  }
  
}
/* Called when the device is closed.
 */
static int gntmem_release(struct inode *inode, struct file *filp)
{
  DBG("Device closed with context %p\n", filp->private_data);
  if (filp->private_data) {
    struct gntmem_state* state = (struct gntmem_state*)filp->private_data;

    if(state->pages) {
      DBG("Found device had pages granted; trying to ungrant\n");
      try_ungrant_and_free_pages(state->pages, state->npages);
    }

    atomic_deref(state);
  }
  module_put(THIS_MODULE);
  return 0;
}

/* Called when an attempt is made to mmap() the device. The private data from
 * @flip contains the list of pages that can be mapped. The vm_pgoff 
 * field of @vma contains the index of the first page to map. 
 * Only mappings that are a multiple of PAGE_SIZE are handled.
 */
static int gntmem_mmap (struct file *filp, struct vm_area_struct *vma) 
{

  struct gntmem_state *private_data = (struct gntmem_state*)filp->private_data;

  DBG("mmap: device with context %p\n", private_data);

  if (unlikely(!private_data)) {
    printk(KERN_ERR "File's private data is NULL.\n");
    return -EINVAL;
  }

  if(unlikely((!private_data->pages) || (!private_data->npages))) {
    printk(KERN_ERR "gntmap mmaped before it had been initialised\n");
    return -EINVAL;
  }

  if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
    printk(KERN_ERR "Writable mappings must be shared.\n");
    return -EINVAL;
  }

  vma->vm_ops = &gntmem_vmops;
    
  vma->vm_private_data = private_data;

  /* This flag prevents Bad PTE errors when the memory is unmapped. */
  vma->vm_flags |= VM_RESERVED;

  gntmem_vma_open(vma);

  return 0;

}

static void gntmem_vma_open(struct vm_area_struct* vma) {

  struct gntmem_state* state = (struct gntmem_state*)vma->vm_private_data;

  DBG("VMA open: device with context %p\n", state);

  atomic_ref(state);

}

/* "Destructor" for a VM area.
 */
static void gntmem_vma_close(struct vm_area_struct *vma) {

  struct gntmem_state* state = (struct gntmem_state*)vma->vm_private_data;

  DBG("VMA close: device with context %p\n", state);

  atomic_deref(state);

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)

#define NOPAGE_SIGBUS ((struct page*)0xFFFFFFFF)

static int gntmem_handle_fault(struct vm_area_struct* vma, struct vm_fault* vmf) {

  struct page* answer = gntmem_handle_nopage(vma, (unsigned long)vmf->virtual_address, 0);

  if(answer == NOPAGE_SIGBUS)
    return VM_FAULT_OOM;
  else {
    vmf->page = answer;
    return 0;
  }

}

#endif

static struct page* gntmem_handle_nopage(struct vm_area_struct* vma, unsigned long address, int* type) {

  struct gntmem_state* state = (struct gntmem_state*)vma->vm_private_data;

  DBG("Page fault for address %lu, on device with context %p\n", address, state);

  // No need for locking here: after initialisation and until closure the pages
  // and npages fields are immutable, and they were checked by mmap.

  unsigned long addr_offset = address - vma->vm_start;
  unsigned long page_offset = addr_offset >> PAGE_SHIFT;
  unsigned long page_required = page_offset + vma->vm_pgoff;

  DBG("Giving out page %lu\n", page_required);

  if((!state) || (!state->pages)) {
    BUG();
    return /*VM_FAULT_OOM*/ NOPAGE_SIGBUS;
  }

  if(/*vmf->pgoff*/ page_required >= state->npages) {
    DBG("Page %lu is out of range\n", page_required);
    printk(KERN_INFO "gntmem returned SIGBUS: trying to access page %lu of a group of %d\n", /*vmf->pgoff*/ page_required, state->npages);
    return NOPAGE_SIGBUS;
  }

  if(type)
    *type = VM_FAULT_MINOR;

  DBG("Giving out page at %p\n", state->pages[page_required].page);

  get_page(state->pages[/*vmf->pgoff*/ page_required].page);
  /*vmf->page = state->pages[vmf->pgoff];
    return 0;*/
  return state->pages[page_required].page;

}

/* Called when an ioctl is made on the device.
 */
static long gntmem_ioctl(struct file *filp,
			 unsigned int cmd, unsigned long arg)
{
  
  int success = 1;
  int i;

  struct gntmem_state* state = (struct gntmem_state*)filp->private_data;

  DBG("ioctl for device %p: cmd %u, arg %lu\n", state, cmd, arg);

  switch (cmd) {
  case IOCTL_GNTMEM_SET_DOMAIN:
    {
      // arg is a domain number, of type domid_t
      if(state->pages) {
	printk("KERN_ERR gntmap: Grant domain must be set before section creation\n");
	return -EINVAL;
      }
      state->domain = (domid_t)arg;
      DBG("Set domain to %u\n", state->domain);
      return 0;
    }
  case IOCTL_GNTMEM_SET_SIZE:
    {
      int npages = arg;
      DBG("Set section size to %d pages\n", npages);

      struct granted_page* pages = kmalloc(npages * sizeof(struct granted_page), GFP_KERNEL);
      DBG("Allocated a page/grant-list at %p\n", pages);

      if(state->pages) {
	printk(KERN_ERR "gntmem initialisation is one-time only!\n");
	return -EINVAL;
      }

      if(!pages) {
	printk(KERN_ERR "Failed to allocate page-array (size %d)\n", npages);
	return -ENOMEM;
      }

      memset(pages, 0, npages * sizeof(struct granted_page));

      for(i = 0; i < npages; i++) {
	pages[i].page = alloc_page(GFP_KERNEL);
	DBG("Got page %p for page %d of this section\n", pages[i].page, i);
	if(!pages[i].page) {
	  success = 0;
	  break;
	}
	pages[i].grant = gnttab_grant_foreign_access(state->domain, pfn_to_mfn(page_to_pfn(pages[i].page)), 0);
	DBG("Got a grant (%u) for this page\n", pages[i].grant);
	if(pages[i].grant < 0) {
	  printk(KERN_ERR "gntmap: Failed to grant page\n");
	  success = 0;
	  break;
	}
      }

      if(success) {
	DBG("Page allocation and granting succeeded; locking to attach to device\n");
	int ret = down_interruptible(&state->sem);
	if(ret != 0) {
	  DBG("Interrupted getting lock; unmapping\n");
	  // Interrupted whilst getting semaphore
	  success = 0;
	}
	else {
	  if(!state->pages) {
	    DBG("Successfully attached\n");
	    state->pages = pages;
	    state->npages = npages;
	  }
	  else {
	    DBG("Failed to attach: state->pages was not zero\n");
	    success = 0;
	  }
	  up(&state->sem);
	}
      }

      if(!success) {
	DBG("Operation was unsuccessful: unmapping pages\n");
	try_ungrant_and_free_pages(pages, npages);
	return -ENOMEM;
      }

      return 0;

    }
  case IOCTL_GNTMEM_GET_GRANTS:
    {
      // arg is a buffer, which ought to be big enough to hold npages grants.
      // If not, that's the user's problem -- we'll just get a fault back from
      // copy_to_user.

      int failed_bytes = 0;

      struct granted_page* pages;
      int npages;
      grant_ref_t* user_grants;

      DBG("ioctl was a grant-request\n");

      if(!state->pages) {
	printk(KERN_ERR "gntmap: Tried to map an uninitialised device\n");
	return -EINVAL;
      }
      
      if(down_interruptible(&state->sem)) {
	DBG("Interrupted getting the state semaphore\n");
	return -EINTR;
      }

      pages = state->pages;
      npages = state->npages;

      up(&state->sem);

      user_grants = (grant_ref_t*)arg;

      int i;
      for(i = 0; i < npages; i++) {
	DBG("Passing out grant %u for page %p\n", pages[i].grant, pages[i].page);
	failed_bytes = copy_to_user(&(user_grants[i]), 
				    &(pages[i].grant),
				    sizeof(grant_ref_t));

	if(failed_bytes) {
	  DBG("Copy to userspace failed\n");
	  break;
	}
      }

      if(failed_bytes)
	return -EFAULT;
      else
	return 0;

    }
  default:
    return -EINVAL;
  }
}
