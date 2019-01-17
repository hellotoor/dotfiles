unsigned long arp_lock_switch = 0;

static int proc_read(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
    unsigned long *this = data;

    return(snprintf(buf, count, "%ld\n", *this));
}

static int proc_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    unsigned long *this = data;

    *this = simple_strtoul(buffer, NULL, 10);
    return(count);
}


static int  __init proc_init(void)
{
    struct proc_dir_entry *proc = NULL;

    proc = create_proc_entry("arp_lock_switch", 0644, NULL);
    if(proc == NULL) {
        printk("create arp lock switch proc error!\n");
        return -ENOMEM;
    }
    proc->data = (void *)&arp_lock_switch;
    proc->read_proc = proc_read;
    proc->write_proc = proc_write;

    return 0;
}

static void  __init proc_exit()
{
	remove_proc_entry("arp_lock_switch", NULL);
}

module_init(proc_init);
module_exit(proc_exit);
