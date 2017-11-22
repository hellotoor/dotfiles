#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/list.h>

#define REQUEST_MIN_LEN 	18
#define URL_MAX_LEN 	        8192	
#define URL_SPLIT_CHAR 	        ' '
#define URL_SWITCH_OFF 	  	0	
#define URL_SWITCH_ON 	  	1
#define URL_MATCH_MODE_EXACTLY 	0
#define URL_MATCH_MODE_PARTLY 	1

#define UDP_HEADER_LEN 		8
#define DNS_QUERY_OFFSET 	12
#define DNS_DOMAIN_MAX_LEN 	512


static struct proc_dir_entry *url_filter_dir, *proc_bl_switch, *proc_bl_mode, *proc_bl_url;
static struct proc_dir_entry *proc_wl_switch, *proc_wl_mode, *proc_wl_url, *proc_debug;

static unsigned long bl_switch = 0, bl_mode = 0, wl_switch = 0, wl_mode = 0;
static char bl_url[URL_MAX_LEN] = "";
static char wl_url[URL_MAX_LEN] = "";

static struct list_head bl_url_list;
static struct list_head wl_url_list;

static int url_filter_pkt_err = 0;
static int url_filter_no_host = 0;
static int url_filter_bl_exactly = 0;
static int url_filter_bl_partly= 0;
static int url_filter_wl_exactly = 0;
static int url_filter_wl_partly= 0;

struct url_info {
	struct list_head list;
	char *url;
	int url_len;
};

static char *memmem(char *src, int src_len, char *sub, int sub_len)
{
	int i, j;

	if(src == NULL || sub == NULL || src_len <= 0|| sub_len <= 0 || src_len < sub_len)
		return NULL;

	for (i = 0; i <= src_len - sub_len; i++) 
	{
		for (j = 0; j < sub_len; j++) 
		{
			if(src[i+j] != sub[j])
				break;
		}

		if(j >= sub_len) 
		{
			return src + i;
		}
	}

	return NULL;
}

static int verdict(char *domain, int domain_len) 
{
	struct url_info *entry = NULL;

	if (wl_switch == URL_SWITCH_ON) 
	{
		if(wl_mode == URL_MATCH_MODE_EXACTLY)
		{
			list_for_each_entry(entry, &wl_url_list, list) {
				if(entry->url_len == domain_len
						&& memcmp(domain, entry->url, entry->url_len) == 0)
				{
					url_filter_wl_exactly++;
					return NF_ACCEPT;
				}
			}
		}
		else 
		{
			list_for_each_entry(entry, &wl_url_list, list) {
				if(memmem(domain, domain_len, entry->url, entry->url_len)) 
				{
					url_filter_wl_partly++;
					return NF_ACCEPT;
				}
			}
		}

		return NF_DROP;
	}

	if (bl_switch == URL_SWITCH_ON) 
	{
		if(bl_mode == URL_MATCH_MODE_EXACTLY)
		{
			list_for_each_entry(entry, &bl_url_list, list) {
				if(entry->url_len == domain_len
						&& memcmp(domain, entry->url, entry->url_len) == 0)
				{
					url_filter_bl_exactly++;
					return NF_DROP;
				}
			}
		}
		else 
		{
			list_for_each_entry(entry, &bl_url_list, list) {
				if(memmem(domain, domain_len, entry->url, entry->url_len)) 
				{
					url_filter_bl_partly++;
					return NF_DROP;
				}
			}
		}

		return NF_ACCEPT;
	}

	return NF_ACCEPT;
}

static void parse_dns_query(char *app_buf, int app_len, char *domain)
{
	char *dns_query = app_buf + DNS_QUERY_OFFSET;
	char *iter;
	int label_len = 0;
	int domain_len = 0;

	strncpy(domain, dns_query + 1, DNS_DOMAIN_MAX_LEN - 1);
	domain_len = strlen(domain);

	iter = domain - 1;
	label_len = *dns_query;

	while(iter + 1 + label_len < domain + domain_len)
	{
		iter = iter + 1 + label_len;
		label_len = *iter;
		*iter = '.';
	}
}

static unsigned int url_filter_hook(unsigned int hooknum, struct sk_buff *skb,
		 		const struct net_device *in, const struct net_device *out,
		 		int (*okfn)(struct sk_buff *))
{
	int app_len = 0, url_len = 0, domain_len = 0,  i = 0;
	char *app_buf = NULL;
	struct iphdr *iph = NULL;
	struct tcphdr *thdr = NULL;
	struct udphdr *uhdr = NULL;
	char *url_start, *url_end;
	char *domain_start, *domain_end;
	char dns_domain[DNS_DOMAIN_MAX_LEN] = "";

	if (!skb)
		return NF_ACCEPT;

	iph = (struct iphdr *)skb_network_header(skb);

	if (iph == NULL)
		return NF_ACCEPT;

	if (iph->protocol == IPPROTO_TCP) 
	{
		thdr = (struct tcphdr *)(skb->data + iph->ihl * 4);
		if (thdr->dest != htons(80))
			return NF_ACCEPT;

		app_len = skb->len - iph->ihl * 4 - thdr->doff * 4;
		if(app_len < REQUEST_MIN_LEN)
			return NF_ACCEPT;

		app_buf = (char *)thdr + (thdr->doff * 4);
		if(memcmp(app_buf, "GET ", 4) != 0)
			return NF_ACCEPT;

		url_start = app_buf + 4;
		url_end = memmem(app_buf, app_len, " HTTP", sizeof(" HTTP") - 1); 
		if(url_end == NULL)
			return NF_ACCEPT;

		url_len = url_end - url_start + 1;

#if 0
		printk("get uri:");
		for(i = 0; i < url_len; i++)
		{
			printk("%c", *(url_start + i));
		}
		printk("\n");
#endif

		domain_start = memmem(app_buf, app_len, "Host: ", sizeof("Host: ") - 1);
		if(domain_start == NULL) 
		{
			printk("url filter error: no host in http packet\n");
			url_filter_no_host++;
			return NF_ACCEPT;
		}

		domain_start += (sizeof("Host: ") - 1);
		domain_end = memmem(domain_start, app_len - (domain_start - app_buf), "\r\n", 2);
		if(domain_end == NULL)
		{
			printk("url filter error: packet malformed\n");
			url_filter_pkt_err++;
			return NF_ACCEPT;
		}

		domain_end -= 1;
		domain_len = domain_end - domain_start + 1;

#if 0
		printk("get domain:");
		for(i = 0; i < domain_len; i++)
		{
			printk("%c", *(domain_start + i));
		}
		printk("\n");
#endif

		return verdict(domain_start, domain_len);
	}
	else if (iph->protocol == IPPROTO_UDP) 
	{
		uhdr = (struct udphdr *)(skb->data + iph->ihl * 4);
		if (uhdr->dest != htons(53))
			return NF_ACCEPT;

		app_len = ntohs(uhdr->len) - UDP_HEADER_LEN;

		parse_dns_query((char *)uhdr + UDP_HEADER_LEN, app_len, dns_domain);

//		printk("dns domain: %s\n", dns_domain);

		return verdict(dns_domain, strlen(dns_domain));
	}
	
	return NF_ACCEPT;
}

static struct nf_hook_ops url_filter_process = {
	.hook 		= url_filter_hook,
	.owner 		= THIS_MODULE,
	.pf 		= PF_INET,
	.hooknum 	= NF_INET_FORWARD,
	.priority 	= NF_IP_PRI_FIRST,
};

static void parse_url(char *url_buf, int buf_len, struct list_head *url_list)
{
	struct url_info *url, *n;
	char *url_end = NULL;
	int offset = 0;
	int i = 0;

	list_for_each_entry_safe(url, n, url_list, list) {
		list_del(&url->list);
		kfree(url);
	}
	
	while(url_end = strchr(url_buf + offset, ' '))
	{
		url = kmalloc(sizeof(struct url_info), GFP_KERNEL);
		if(url == NULL) 
			return;

		url->url = url_buf + offset;
		url->url_len = url_end - url_buf - offset;
		offset = url_end - url_buf + 1;
		list_add(&url->list, url_list);
	}

	url = kmalloc(sizeof(struct url_info), GFP_KERNEL);
	if(url == NULL) 
		return;

	url->url = url_buf + offset;
	url->url_len = buf_len - offset - 1;
	list_add(&url->list, url_list);

#if 0
	list_for_each_entry(url, url_list, list) {
		printk("url len: %d\n", url->url_len);
		printk("url:");
		for(i = 0; i < url->url_len; i++)
		{
			printk("%c", *(url->url + i));
		}
		printk("\n");
	}
#endif
}

static int proc_read_ul(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	unsigned long *this = data;

	return snprintf(page, count, "%ld\n", *this);
}

static int proc_write_ul(struct file *file, const char *buffer, unsigned long count, void *data)
{
	unsigned long *this = data;

	*this = simple_strtoul(buffer, NULL, 10);

	return count;
}

static int proc_read_str(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	len = sprintf(page, "%s\n", (char *)data);

	return len;
}

static int proc_write_bl_url(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len;
	char *this = data;

	if(count > URL_MAX_LEN - 1)
		len = URL_MAX_LEN - 1;
	else
		len = count;

	if(copy_from_user(this, buffer, len))
		return -EFAULT;

	this[len] = '\0';

	parse_url(bl_url, len, &bl_url_list);

	return len;
}

static int proc_write_wl_url(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len;
	char *this = data;

	if(count > URL_MAX_LEN - 1)
		len = URL_MAX_LEN - 1;
	else
		len = count;

	if(copy_from_user(this, buffer, len))
		return -EFAULT;

	this[len] = '\0';

	parse_url(wl_url, len, &wl_url_list);

	return len;
}

static int proc_read_debug(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	return snprintf(page, count, "pkt_err: %d\nno_host: %d\nbl_exactly: %d\nbl_partly: %d\nwl_exactly: %d\nwl_partly:%d\n", url_filter_pkt_err, url_filter_no_host, url_filter_bl_exactly, url_filter_bl_partly, url_filter_wl_exactly, url_filter_wl_partly);
}

static int __init url_filter_init(void)
{
	int ret = -1;

	nf_register_hook(&url_filter_process);

	url_filter_dir = proc_mkdir("url_filter", NULL);
	if(url_filter_dir == NULL) 
	{
		ret = -ENOMEM;
		goto out;
	}

 	proc_bl_switch = create_proc_entry("bl_switch", 0644, url_filter_dir);
	if(proc_bl_switch == NULL) 
	{
		ret = -ENOMEM;
		goto no_proc_bl_switch;
	}

	proc_bl_switch->data = &bl_switch;
	proc_bl_switch->read_proc = proc_read_ul;
	proc_bl_switch->write_proc = proc_write_ul;

 	proc_bl_mode = create_proc_entry("bl_mode", 0644, url_filter_dir);
	if(proc_bl_mode == NULL) 
	{
		ret = -ENOMEM;
		goto no_proc_bl_mode;
	}

	proc_bl_mode->data = &bl_mode;
	proc_bl_mode->read_proc = proc_read_ul;
	proc_bl_mode->write_proc = proc_write_ul;

 	proc_bl_url = create_proc_entry("bl_url", 0644, url_filter_dir);
	if(proc_bl_url== NULL) 
	{
		ret = -ENOMEM;
		goto no_proc_bl_url;
	}

	proc_bl_url->data = bl_url;
	proc_bl_url->read_proc = proc_read_str;
	proc_bl_url->write_proc = proc_write_bl_url;

 	proc_wl_switch = create_proc_entry("wl_switch", 0644, url_filter_dir);
	if(proc_wl_switch == NULL) 
	{
		ret = -ENOMEM;
		goto no_proc_wl_switch;
	}

	proc_wl_switch->data = &wl_switch;
	proc_wl_switch->read_proc = proc_read_ul;
	proc_wl_switch->write_proc = proc_write_ul;

 	proc_wl_mode = create_proc_entry("wl_mode", 0644, url_filter_dir);
	if(proc_wl_mode == NULL) 
	{
		ret = -ENOMEM;
		goto no_proc_wl_mode;
	}

	proc_wl_mode->data = &wl_mode;
	proc_wl_mode->read_proc = proc_read_ul;
	proc_wl_mode->write_proc = proc_write_ul;

 	proc_wl_url = create_proc_entry("wl_url", 0644, url_filter_dir);
	if(proc_wl_url== NULL) 
	{
		ret = -ENOMEM;
		goto no_proc_wl_url;
	}

	proc_wl_url->data = wl_url;
	proc_wl_url->read_proc = proc_read_str;
	proc_wl_url->write_proc = proc_write_wl_url;

	proc_debug = create_proc_read_entry("debug", 0444, url_filter_dir, proc_read_debug, NULL);
	if(proc_debug == NULL) 
	{
		ret = -ENOMEM;
		goto no_proc_debug;
	}

	INIT_LIST_HEAD(&bl_url_list);
	INIT_LIST_HEAD(&wl_url_list);

	printk("Successful to init url filter module\n");

	return 0;

no_proc_debug:
 	remove_proc_entry("wl_url", url_filter_dir);
no_proc_wl_url:
 	remove_proc_entry("wl_mode", url_filter_dir);
no_proc_wl_mode:
 	remove_proc_entry("wl_switch", url_filter_dir);
no_proc_wl_switch:
 	remove_proc_entry("bl_url", url_filter_dir);
no_proc_bl_url:
 	remove_proc_entry("bl_mode", url_filter_dir);
no_proc_bl_mode:
 	remove_proc_entry("bl_switch", url_filter_dir);
no_proc_bl_switch:
	remove_proc_entry("url_filter", NULL);
out:
	return ret;
}

static void __exit url_filter_exit(void)
{
	struct url_info *url, *n;

	list_for_each_entry_safe(url, n, &bl_url_list, list) {
		list_del(&url->list);
		kfree(url);
	}

	list_for_each_entry_safe(url, n, &wl_url_list, list) {
		list_del(&url->list);
		kfree(url);
	}

	nf_unregister_hook(&url_filter_process);

 	remove_proc_entry("wl_url", url_filter_dir);
 	remove_proc_entry("wl_mode", url_filter_dir);
 	remove_proc_entry("wl_switch", url_filter_dir);
 	remove_proc_entry("bl_url", url_filter_dir);
 	remove_proc_entry("bl_mode", url_filter_dir);
 	remove_proc_entry("bl_switch", url_filter_dir);
 	remove_proc_entry("debug", url_filter_dir);
	remove_proc_entry("url_filter", NULL);

	printk("Successful to exit url filter module\n");
}

module_init(url_filter_init);
module_exit(url_filter_exit);
