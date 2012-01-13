//#include <linux/init.h>
#include <linux/module.h>
//#include <linux/sched.h>
#include <linux/slab.h>
//#include <linux/fs.h>
//#include <linux/kernel.h>
#include <linux/mm.h>
//#include <linux/pagemap.h>
//#include <linux/rmap.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
#define MODULE_NAME "Cos<->Linux translator"
#define TRANS_PAGE_ORDER 8
#define TRANS_MAX_NAME_SZ 128

struct trans_channel {
	char *mem;
	unsigned long linux_addr;

	char *name;
	wait_queue_head_t e;
	int event; 		/* is data available? */

	int lrefcnt;
};

static void 
trans_mapping_deref(struct trans_channel *c)
{
	c->lrefcnt--;
	if (!c->lrefcnt) {
		free_pages((unsigned long)c->mem, TRANS_PAGE_ORDER);
		wake_up_interruptible_all(&c->e);
		if (c->name) kfree(c->name);
		kfree(c);
	}
}

void
trans_vm_close(struct vm_area_struct *vma)
{
	trans_mapping_deref(vma->vm_private_data);
	return;
}

struct vm_operations_struct trans_vmops = {
	.close = trans_vm_close
};

int
trans_open(struct inode *i, struct file *f)
{
	struct trans_channel *c;

	c = kmalloc(sizeof(struct trans_channel), GFP_KERNEL);
	if (!c) return -ENOMEM;

	init_waitqueue_head(&c->e);
	c->mem = (char*)alloc_pages(GFP_KERNEL, TRANS_PAGE_ORDER);
	if (!c->mem) goto free;
	c->name = NULL;
	c->lrefcnt = 2;
	f->private_data = c;

	memcpy(c->mem, "foobar", 6);

	return 0;
free:
	kfree(c);
	return -ENOMEM;
}

int
trans_close(struct inode *i, struct file *f)
{
	struct trans_channel *c = f->private_data;

	trans_mapping_deref(c);
	return 0;
}

int
trans_mmap(struct file *f, struct vm_area_struct *vma)
{
	struct trans_channel *c = f->private_data;
	struct page *pg = NULL;
	unsigned long addr;
	int i;

	for (addr = vma->vm_start, i = 0 ; 
	     addr < vma->vm_end ; 
	     addr += PAGE_SIZE, i++) {
		pg = virt_to_page(&c->mem[PAGE_SIZE*i]);
		BUG_ON(!pg);
		
		if (vm_insert_page(vma, addr, pg)) {
			zap_vma_ptes(vma, vma->vm_start, addr - vma->vm_start);
			goto err;
		}
		//BUG_ON(pg != follow_page(vma, addr, 0));
	}
	vma->vm_flags |= (VM_RESERVED | VM_INSERTPAGE);
	vma->vm_ops = &trans_vmops;
	BUG_ON(vma->vm_private_data);
	vma->vm_private_data = c;
	
	return 0;
err:
	return -EAGAIN;
}

ssize_t 
trans_read(struct file *f, char __user *b, size_t s, loff_t *o)
{
	struct trans_channel *c = f->private_data;

	wait_event_interruptible(c->e, (c->event)); /* if no event, block */
	//later: wake_up_interruptible(&e)
	return 0;
}

ssize_t
trans_write(struct file *f, const char __user *b, size_t s, loff_t *o)
{
	char *n;
	struct trans_channel *c = f->private_data;

	if (c->name) return s;
	if (s > TRANS_MAX_NAME_SZ) return -EINVAL;

	n = kmalloc(s, GFP_KERNEL);
	if (!n) return -ENOMEM;
	if (copy_from_user(n, b, s)) return -EFAULT;
	c->name = n;

	return s;
}

struct file_operations trans_fsops = {
	.read    = trans_read,
	.write   = trans_write,
	.mmap    = trans_mmap,
	.open    = trans_open,
	.release = trans_close,
	.owner   = THIS_MODULE,
};

int
trans_init(void)
{
	struct proc_dir_entry *ent;

	ent = create_proc_entry("translator", 0666, NULL);
	if(ent == NULL){
		printk("cos: make_proc_aed : Failed to register /proc/aed\n");
		return -1;
	}
	ent->proc_fops = &trans_fsops;

	printk("Translator: registered successfully.\n");
	
	return 0;
}

void
trans_exit(void)
{
	remove_proc_entry("translator", NULL);
	printk("Translator: unregistered successfully.\n");
}

module_init(trans_init);
module_exit(trans_exit);
