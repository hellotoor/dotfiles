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
#include <net/ip.h>
#include <net/tcp.h>
#include <net/route.h>
#include <net/dst.h>

#define URL_MAX_LEN        	256
#define REQUEST_MIN_LEN 	18

static struct proc_dir_entry *url_redirect_dir, *proc_enable, *proc_url;
static unsigned long url_redirect_enable = 0;
static char url_redirect_url[URL_MAX_LEN] = "";
static char http_response[512] = "";

enum {
	DIR_UNKNOW = 0,
	DIR_LAN_TO_WAN = 1,
 	DIR_WAN_TO_LAN = 2,
	DIR_LAN_TO_LAN = 3,
};

static int get_skb_dir(const struct net_device *in, const struct net_device *out)
{
	if (memcmp(in->name, "br-lan", 6) == 0 && memcmp(out->name, "br-lan", 6) != 0)
		return DIR_LAN_TO_WAN;
	else if (memcmp(in->name, "br-lan", 6) != 0 && memcmp(out->name, "br-lan", 6) == 0)
		return DIR_WAN_TO_LAN;
	else 
		return DIR_UNKNOW;
}

static void send_reply(struct sk_buff *oldskb)
{
	struct sk_buff *nskb;
	const struct iphdr *oiph;
	struct iphdr *niph;
	const struct tcphdr *oth;
	struct tcphdr _otcph, *tcph;
	unsigned int addr_type;
	char *app_buf = NULL;
	int app_len = 0;

	/* IP header checks: fragment. */
	if (ip_hdr(oldskb)->frag_off & htons(IP_OFFSET))
		return;

	oth = skb_header_pointer(oldskb, ip_hdrlen(oldskb), sizeof(_otcph), &_otcph);
	if (oth == NULL)
		return;

	oiph = ip_hdr(oldskb);

	app_len = strlen(http_response);

	nskb = alloc_skb(sizeof(struct iphdr) + sizeof(struct tcphdr) + app_len +
			 LL_MAX_HEADER, GFP_ATOMIC);
	if (!nskb)
		return;

	skb_reserve(nskb, LL_MAX_HEADER);

	skb_reset_network_header(nskb);
	niph = (struct iphdr *)skb_put(nskb, sizeof(struct iphdr));
	niph->version	= 4;
	niph->ihl	= sizeof(struct iphdr) / 4;
	niph->tos	= 0;
	niph->id	= 0;
	niph->frag_off	= htons(IP_DF);
	niph->protocol	= IPPROTO_TCP;
	niph->check	= 0;
	niph->saddr	= oiph->daddr;
	niph->daddr	= oiph->saddr;

	tcph = (struct tcphdr *)skb_put(nskb, sizeof(struct tcphdr));
	memset(tcph, 0, sizeof(*tcph));
	tcph->source	= oth->dest;
	tcph->dest	= oth->source;
	tcph->doff	= sizeof(struct tcphdr) / 4;
	tcph->window 	= htons(4096);
	tcph->seq 	= oth->ack_seq;
	tcph->ack_seq 	= htonl(ntohl(oth->seq) + oth->syn + oth->fin +
				oldskb->len - ip_hdrlen(oldskb) -
				(oth->doff << 2));
	tcph->ack 	= 1;
//	tcph->rst	= 1;
	tcph->fin 	= 1; 
	tcph->psh 	= 1; 

	app_buf = (char *)skb_put(nskb, app_len);
	memcpy(app_buf, http_response, app_len);

	tcph->check	= tcp_v4_check(sizeof(struct tcphdr) + app_len,
				       niph->saddr, niph->daddr,
				       csum_partial(tcph, sizeof(struct tcphdr) + app_len, 0));

	addr_type = RTN_LOCAL;

	/* ip_route_me_harder expects skb->dst to be set */
	skb_dst_set(nskb, dst_clone(skb_dst(oldskb)));

	if (ip_route_me_harder(nskb, addr_type))
		goto free_nskb;

	niph->ttl	= dst_metric(skb_dst(nskb), RTAX_HOPLIMIT);
	nskb->ip_summed = CHECKSUM_NONE;

	/* "Never happens" */
	if (nskb->len > dst_mtu(skb_dst(nskb)))
		goto free_nskb;

	nf_ct_attach(nskb, oldskb);

	ip_local_out(nskb);
	return;

free_nskb:
	kfree_skb(nskb);
}

static unsigned int url_redirect_hook(unsigned int hooknum, struct sk_buff *skb,
		 		const struct net_device *in, const struct net_device *out,
		 		int (*okfn)(struct sk_buff *))
{
	int app_len = 0;
	struct iphdr *iph = NULL;
	char *app_buf = NULL;
	struct tcphdr *thdr = NULL;
	int dir = DIR_UNKNOW;

	if (!skb)
		return NF_ACCEPT;

	if(in == NULL || out == NULL)
		return NF_ACCEPT;

	dir = get_skb_dir(in, out);

	if(dir == DIR_LAN_TO_WAN)
	{
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

			/* Only process http get request */
			if(url_redirect_enable == 1 && url_redirect_url[0] != '\0')
			{
				/* Does it necessary to check Accept Option containing 'text/html' or '* / *'? */
				snprintf(http_response, 512, "HTTP/1.1 302 Found\r\n"
						"Server: nginx\r\n"
						"Date: Tue, 07 Nov 2017 07:01:56 GMT\r\n"
						"Location: %s\r\n"
						"Content-Length: 0\r\n"
						"Content-Type: text/html\r\n"
						"\r\n", url_redirect_url);
				send_reply(skb);
				return NF_DROP;
			}
		}
	}
	
	return NF_ACCEPT;
}

static struct nf_hook_ops url_redirect_process = {
	.hook 		= url_redirect_hook,
	.owner 		= THIS_MODULE,
	.pf 		= PF_INET,
	.hooknum 	= NF_INET_FORWARD,
	.priority 	= NF_IP_PRI_FIRST,
};

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

	len = sprintf(page, "%s", (char *)data);

	return len;
}

static int proc_write_str(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len, i;
	char *this = data;

	if(count > URL_MAX_LEN)
		len = URL_MAX_LEN;
	else
		len = count;

	if(copy_from_user(this, buffer, len))
		return -EFAULT;

	this[len] = '\0';

	for(i = 0; i < len; i++)
	{
		if(this[i] == '\n' || this[i] == '\r')
			this[i] = '\0';
	}

	return len;
}

static int __init url_redirect_init(void)
{
	int ret = -1;

	nf_register_hook(&url_redirect_process);

	url_redirect_dir = proc_mkdir("url_redirect", NULL);
	if(url_redirect_dir == NULL) 
	{
		ret = -ENOMEM;
		goto out;
	}

 	proc_enable = create_proc_entry("enable", 0644, url_redirect_dir);
	if(proc_enable == NULL) 
	{
		ret = -ENOMEM;
		goto no_proc_enable;
	}

	proc_enable->data = &url_redirect_enable;
	proc_enable->read_proc = proc_read_ul;
	proc_enable->write_proc = proc_write_ul;

 	proc_url = create_proc_entry("url", 0644, url_redirect_dir);
	if(proc_url == NULL) 
	{
		ret = -ENOMEM;
		goto no_proc_url;
	}

	proc_url->data = &url_redirect_url;
	proc_url->read_proc = proc_read_str;
	proc_url->write_proc = proc_write_str;

	printk("Successful to init url redirect module\n");

	return 0;

no_proc_url:
 	remove_proc_entry("enable", url_redirect_dir);
no_proc_enable:
	remove_proc_entry("url_redirect", NULL);
out:
	return ret;
}

static void __exit url_redirect_exit(void)
{
	nf_unregister_hook(&url_redirect_process);

 	remove_proc_entry("enable", url_redirect_dir);
 	remove_proc_entry("url", url_redirect_dir);
	remove_proc_entry("url_redirect", NULL);

	printk("Successful to exit url redirect module\n");
}

module_init(url_redirect_init);
module_exit(url_redirect_exit);

MODULE_LICENSE("GPL");
