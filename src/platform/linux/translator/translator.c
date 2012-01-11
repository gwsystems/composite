//#include <linux/init.h>
#include <linux/module.h>
//#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
//#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>

#define NAME "cos translator"

static void *mem;
char *name;
wait_queue_head_t e;

int
trans_open(struct inode *i, struct file *f)
{
	init_waitqueue_head(&e);
	
	return 0;
}

int
trans_close(struct inode *i, struct file *f)
{
	free(name);
	name = NULL;

	return 0;
}

int
trans_mmap(struct file *f, struct vm_area_struct *vma)
{
	unsigned long start;
	int i;

	for (i = 0, start = vma->vm_start ; 
	     start < vma->vm_end ; 
	     start += PAGE_SIZE, i++) {
		vm_insert_pfn(vma, start, __pa(&mem[i]));
	}
	
	return 0;
}

ssize_t 
trans_read(struct file *f, char __user *b, size_t s, loff_t *o)
{
	//wait_event_interruptable(&e, (sleep if false));
	//later: wake_up_interruptable(&e)
}

ssize_t
trans_write(struct file *f, const char __user *b, size_t s, loff_t *o)
{
	char *n;

	if (s > 1024 || s < 0 || name) return -EINVAL;
	n = malloc(s);
	if (!n) return -ENOMEM;

	if (copy_from_user(n, b, s)) return -EFAULT;
	name = n;

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

void
trans_vm_close(struct vm_area_struct *vma)
{
	return;
}

struct vm_operations_struct trans_vmops = {
	.close = trans_close
};

int
trans_init(void)
{
	struct proc_dir_entry *ent;

	ent = create_proc_entry("translator", 0222, NULL);
	if(ent == NULL){
		printk("cos: make_proc_aed : Failed to register /proc/aed\n");
		return -1;
	}
	ent->proc_fops = &trans_fsops;

	mem = alloc_pages(GFP_KERNEL, 20);
	if (!mem) goto unreg;

	printk("Translator: registered successfully.\n");
	
	return 0;
unreg:
	remove_proc_entry("translator", NULL);
	return -1;
}

void
trans_exit(void)
{
	free_pages(mem, 20);
	remove_proc_entry("translator", NULL);
	printk("Translator: unregistered successfully.\n");
}

module_init(trans_init);
module_exit(trans_exit);
