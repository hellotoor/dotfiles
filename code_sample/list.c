#include <linux/list.h>

static struct list_head bl_url_list;

INIT_LIST_HEAD(&bl_url_list);

struct url_info {
	struct list_head list;
	char *url;
	int url_len;
};

list_for_each_entry(entry, &bl_url_list, list) {
	if(entry->url_len == domain_len && memcmp(domain, entry->url, entry->url_len) == 0)
	{
		url_filter_bl_exactly++;
		return NF_DROP;
	}
}

struct url_info *url;
list_add(&url->list, url_list);

struct url_info *url, *n;
list_for_each_entry_safe(url, n, &bl_url_list, list) {
	list_del(&url->list);
	kfree(url);
}

