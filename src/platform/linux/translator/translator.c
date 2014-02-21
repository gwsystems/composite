#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/uaccess.h>

#include <linux/ioctl.h>
#include "translator_ioctl.h"
#include "../../../kernel/include/shared/cos_types.h"

#define MODULE_NAME "cos <-> Linux translator"
#define FILE_NAME "translator"
#define TRANS_PAGE_ORDER 0
#define TRANS_MAX_MAPPING (1024*1024)
#define MAX_NCHANNELS 10
//#define printl(str,args...) printk(str, ## args)
#define printl(str,args...)

struct trans_channel {
	char *mem; 		/* kernel address */
	unsigned long size;
	void *acap;
	int cpuid;
	int direction;

	wait_queue_head_t e;
	int levent, cevent; 		/* is data available? */

	int channel;
	int lrefcnt;
	struct list_head l;
};

struct trans_channel *channels[MAX_NCHANNELS];

static void
trans_channel_init(struct trans_channel *c)
{
	memset(c, 0, sizeof(struct trans_channel));
	INIT_LIST_HEAD(&c->l);
	init_waitqueue_head(&c->e);
	c->channel = -1;
	c->lrefcnt = 1;
	return;
}

static void 
trans_mapping_deref(struct trans_channel *c)
{
	c->lrefcnt--;
	if (c->lrefcnt) return;

	free_pages((unsigned long)c->mem, TRANS_PAGE_ORDER);
	wake_up_interruptible_all(&c->e);
	if (c->channel != -1) {
		BUG_ON(c->channel > MAX_NCHANNELS);
		channels[c->channel] = NULL;
	}
	kfree(c);
}

static void
trans_vm_close(struct vm_area_struct *vma)
{
	printl("trans_munmap\n");
	return;
}

struct vm_operations_struct trans_vmops = {
	.close = trans_vm_close
};

#define page_to_virt(p) (pfn_to_kaddr(page_to_pfn(p)))

static int
trans_open(struct inode *i, struct file *f)
{
	struct trans_channel *c;

	printl("trans_open\n");
	c = kmalloc(sizeof(struct trans_channel), GFP_KERNEL);
	if (!c) return -ENOMEM;
	trans_channel_init(c);

	c->mem = NULL;
	f->private_data = c;

	return 0;
}

static int
trans_close(struct inode *i, struct file *f)
{
	struct trans_channel *c = f->private_data;
	BUG_ON(!c);

	printl("trans_close\n");
	trans_mapping_deref(c);
	return 0;
}

static int
trans_mmap(struct file *f, struct vm_area_struct *vma)
{
	struct trans_channel *c = f->private_data;
	struct page *pg = NULL, *p;
	unsigned long addr, sz, pages;
	int i;
	BUG_ON(!c);

	printl("trans_mmap, sz %d\n", vma->vm_end - vma->vm_start);

	if (vma->vm_end - vma->vm_start > TRANS_MAX_MAPPING) return -EINVAL;
	if (c->mem) return -EINVAL;

	if (vma->vm_end - vma->vm_start > 4096) return -EINVAL;
	p = alloc_pages(GFP_KERNEL, TRANS_PAGE_ORDER); /* hardcoded for now */
	if (!p) return -ENOMEM;
	c->mem = page_to_virt(p);

	sz = vma->vm_end - vma->vm_start;
	pages = sz/PAGE_SIZE;
	if (sz > TRANS_MAX_MAPPING) return -EINVAL;

	for (addr = vma->vm_start, i = 0 ; 
	     addr < vma->vm_end ; 
	     addr += PAGE_SIZE, i++) {
		pg = virt_to_page(&c->mem[PAGE_SIZE*i]);
		BUG_ON(!pg);
		
		if (vm_insert_page(vma, addr, pg)) {
			zap_vma_ptes(vma, vma->vm_start, addr - vma->vm_start);
			printk("translator: unknown error while inserting vm page\n");
			goto err;
		}
		//BUG_ON(pg != follow_page(vma, addr, 0));
	}
	vma->vm_flags |= (VM_RESERVED | VM_INSERTPAGE);
	vma->vm_ops = &trans_vmops;

	BUG_ON(vma->vm_private_data);
	vma->vm_private_data = c;
	c->size = sz;

	return 0;
err:
	return -EAGAIN;
}

static ssize_t 
trans_read(struct file *f, char __user *b, size_t s, loff_t *o)
{
	struct trans_channel *c = f->private_data;
	BUG_ON(!c);

	printl("trans_read\n");

	wait_event_interruptible(c->e, (c->levent)); /* block until condition */
	assert(get_cpuid() == LINUX_CORE);

	c->levent = 0;

	return s;
}

extern void cos_trans_reg(const struct cos_trans_fns *fns);
extern void cos_trans_dereg(void);
extern void cos_trans_upcall(void *acap);

static void xcore_trans_upcall(void *acap)
{
	cos_trans_upcall(acap);

	return;
}

static ssize_t
trans_write(struct file *f, const char __user *b, size_t s, loff_t *o)
{
	struct trans_channel *c = f->private_data;
	BUG_ON(!c);

	if (!c->acap) return -EINVAL;

	if (get_cpuid() == c->cpuid) {
		cos_trans_upcall(c->acap);
	} else {
		smp_call_function_single(c->cpuid, xcore_trans_upcall, c->acap, 0);
	}

	return s;
}

/***************************/
/*** Composite interface ***/
/***************************/

static void wake_up_channel(void *c)
{
	struct trans_channel *tc = (struct trans_channel *)c;
	
	assert(get_cpuid() == LINUX_CORE);
	tc->levent = 1;
	wake_up_interruptible(&tc->e);
	schedule();
	
	return;
}

/* trans_cos_* are call-back functions */

int trans_cos_evt(int channel)
{
	struct trans_channel *c;

	BUG_ON(channel < 0 || channel >= MAX_NCHANNELS);
	c = channels[channel];
	if (!c) return -1;
	if (!c->levent) {
		if (get_cpuid() == LINUX_CORE) {
			wake_up_channel(c);
		} else {
			smp_call_function_single(LINUX_CORE, wake_up_channel, c, 0);			
		}
	}
	
	return 0;
}

int trans_cos_direction(int channel)
{
	struct trans_channel *c;

	BUG_ON(channel < 0 || channel >= MAX_NCHANNELS);
	c = channels[channel];
	if (!c) return -1;
	return c->direction;
}

int trans_cos_map_sz(int channel)
{
	struct trans_channel *c;

	BUG_ON(channel < 0 || channel >= MAX_NCHANNELS);
	c = channels[channel];
	if (!c) return -1;
	
	return c->size;
}

void *trans_cos_map_kaddr(int channel)
{
	struct trans_channel *c;

	BUG_ON(channel < 0 || channel >= MAX_NCHANNELS);
	c = channels[channel];
	if (!c) return NULL;
	
	return c->mem;
}

int trans_cos_acap_created(int channel, void *acap)
{
	struct trans_channel *c;

	BUG_ON(channel < 0 || channel >= MAX_NCHANNELS);
	c = channels[channel];
	if (!c) return -1;

	c->acap = acap;
	c->cpuid = get_cpuid();

	return 0;
}

const struct cos_trans_fns trans_fns = {
	.levt          = trans_cos_evt,
	.direction     = trans_cos_direction,
	.map_kaddr     = trans_cos_map_kaddr,
	.map_sz        = trans_cos_map_sz,
	.acap_created  = trans_cos_acap_created,
};

static long
trans_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct trans_channel *c = f->private_data;
	BUG_ON(!c);

	printl("trans_ioctl\n");
	switch(cmd) {
	case TRANS_SET_CHANNEL:
	{
		if (arg >= MAX_NCHANNELS || arg < 0) return -EINVAL;
		if (channels[arg]) return -EEXIST;
		channels[arg] = c;
		c->channel = (int)arg;

		break;
	}
	case TRANS_GET_CHANNEL:
		if (copy_to_user((void*)arg, &c->channel, sizeof(int))) return -EINVAL;
		break;
	case TRANS_SET_DIRECTION:
		if (arg != COS_TRANS_DIR_LTOC && arg != COS_TRANS_DIR_CTOL) return -EINVAL;
		c->direction = (int)arg;
		break;
	}
	return 0;
}

struct file_operations trans_fsops = {
	.read    = trans_read,
	.write   = trans_write,
	.mmap    = trans_mmap,
	.open    = trans_open,
	.release = trans_close,
	.unlocked_ioctl = trans_ioctl, 
//	.ioctl   = trans_ioctl,
	.owner   = THIS_MODULE,
};

int
trans_init(void)
{
	struct proc_dir_entry *ent;

	ent = create_proc_entry(FILE_NAME, 0666, NULL);
	if(ent == NULL){
		printk("cos: Failed to register /proc/" FILE_NAME "\n");
		return -1;
	}
	ent->proc_fops = &trans_fsops;
	cos_trans_reg(&trans_fns);

	printk(MODULE_NAME ": registered successfully.\n");

	return 0;
}

void
trans_exit(void)
{
	remove_proc_entry(FILE_NAME, NULL);
	cos_trans_dereg();
	printk(MODULE_NAME ": unregistered successfully.\n");
}

module_init(trans_init);
module_exit(trans_exit);
MODULE_LICENSE("GPL");
