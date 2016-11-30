
static void *proc_seq_start(struct seq_file *seq, loff_t *pos)
{
    if (*pos == 0) {
        return proc;
    } else {
        return NULL;
    }
}

static void *proc_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    ++*pos;
    return NULL;
}
  
static void proc_seq_stop(struct seq_file *s, void *v)
{
}

static int proc_seq_show(struct seq_file *s, void *v)
{
    if ( seq_printf(s, "%-10s %-10u %-10u %-10llu %-10llu %-10llu %-10llu\n", 
                "TOTAL", total_tcp, total_udp, 0ULL, 0ULL, total_upstream, total_downstream) ) {
        return -ENOSPC;
    }
    return 0;
}

static struct seq_operations proc_seq_ops = {
	.start = proc_seq_start,
	.next  = proc_seq_next,
	.stop  = proc_seq_stop,
	.show  = proc_seq_show
};

static int proc_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &proc_seq_ops);
}

static struct file_operations proc_ops = {
    .owner   = THIS_MODULE,
    .open    = proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release_private,
};

static int __init proc_sample_init(void)
{
    struct proc_dir_entry *proc = NULL;

    proc = proc_net_fops_create(&init_net, "proc_name", 0440, &proc_ops);
    if(proc == NULL) {
        printk("create proc error!\n");
        return -1;
    }
}

static int __init proc_sample_exit(void)
{
    proc_net_remove(&init_net, "proc_name");
}

module_init(proc_sample_init);
module_exit(proc_sample_exit);
