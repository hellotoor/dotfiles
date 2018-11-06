/**********************************************************************
 * nm_pushweb.c
 *
 * Charles <charles.chen@ebang.com>
 * Time-stamp: <2011-11-09 14:30:34 >
 * Description:  Push web page to CT & ZTE if PCs browse web under our device by rules
 * BY: MAC || Push Interval || URL || Enable || SN || (telecom && ZTE)
 ***********************************************************************/
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>

#include "match.h"
#include "nm_monitor_pub.h"
#include "nm_monitor.h"
#include "nm_auth_pub.h"
#include "nm_pae_pub.h"

#include "linux_se_if.h"
#include "nm_message.h"
#include "nm_pushweb.h"

/*==debug==*/
static atomic_t pushweb_debug = ATOMIC_INIT(NM_LEVEL_NOTICE);
#define NM_PUSH_OUTPUT(i, X, Y...) NM_OUTPUT(pushweb_debug, i, X, ##Y)

/*==process handle==*/
#define MAX_AUTHRESP_DATELEN    1024 
//#define MAX_BIZ_LEN 1500
static char user_auth_http_resp[MAX_AUTHRESP_DATELEN] = {};
#define AUTHRESP_DEFAULTLEN      235
#define DIGEST_HTTP_HEAD_GET     "GET"
#define DIGEST_HTTP_HEAD_HOST  "Host:"
//fixed by Charles, support HTTP/1.0 and HTTP/1.1
//#define DIGEST_HTTP_GET_END      "HTTP/1.1"
#define DIGEST_HTTP_GET_END      "HTTP/1."
#define SOURCE_DEV_SN               "?SN=00000000000000000"

/*==proc declare==*/
struct proc_item {
    struct proc_dir_entry *pe;
    char *item_name;
};
static struct proc_dir_entry *pd = NULL;
#define PROC_MAIN                 "net_monitor"
#define PROC_PUSH_URL          "portal_url"
#define PROC_PUSH_AGE          "portal_age"
#define PROC_PUSH_ENABLE    "enable_url_redirection"
#define PROC_DEV_SN             "dev_sn"
static struct proc_item proc_items[] = {
    { NULL, PROC_PUSH_URL},
    { NULL, PROC_PUSH_AGE},
    { NULL, PROC_PUSH_ENABLE},
    { NULL, PROC_DEV_SN}
};
#define PROC_ACCESS_VALUE 0666
#define PROC_SIZE (sizeof(proc_items) / sizeof(struct proc_item))

/*==proc data==*/
//push number:2 (0:CT 1:ZTE)
enum push_category {
    PUSH_CT = 0,
    PUSH_ZTE,
    PUSH_NUM
};
#define FLAG_CT      "CT:"
#define FLAG_ZTE    "ZTE:"
#define FLAG_SN     "?SN="
#define MAX_URL_LEN       255
#define MAX_SN_LEN         21     //length of SOURCE_DEV_SN
//#define MAX_PUSH_AGE    1080 //45*24
#define MAX_PUSH_AGE    14400 //10*24*60
char push_url[PUSH_NUM][MAX_URL_LEN + 1];
unsigned int push_age[PUSH_NUM];
char push_enable[PUSH_NUM];
char dev_sn[MAX_SN_LEN + 1];
#define STR_PORTAL_AGE_LEN    10
//#define AGE_MSEC_EXCHANGE    3600000 //push age input unit is 1hour
#define AGE_MSEC_EXCHANGE    60000 //push age input unit is minute
#define MAX_PROC_BUF              512

/*==timer==*/
 //1day=24h*60m*60s*1000ms
#define CHECK_VALIDMAC_INTERVAL 3600000 //1hour
static struct timer_list tl_aging_portal; //timer to aging portal auth list.
static struct completion timer_done;
static unsigned long timebefore;

/*==mac list==*/
struct mac_entry
{
    struct hlist_node   hlist;
    struct rcu_head     rcu;
    unsigned long       portaltime[PUSH_NUM];
    unsigned long       activetime;
    unsigned char       addr[ETH_ALEN];
};
#define MAC_HASH_SIZE   256
static spinlock_t hash_lock;
static struct hlist_head hash[MAC_HASH_SIZE];

static void mac_hash_init(void)
{
    int i;

    spin_lock_init(&hash_lock);

    for (i = 0; i < MAC_HASH_SIZE; i++) 
    {
        INIT_HLIST_HEAD(&hash[i]);
    }
}

static int mac_hash(const unsigned char *mac)
{
    return jhash(mac, ETH_ALEN, 0) & (MAC_HASH_SIZE - 1);
}

static int is_valid_macaddr(const unsigned char *addr)
{
    const char zaddr[6] = {0};

    return !(addr[0] & 1) && memcmp(addr, zaddr, 6);
}

//return val: -1: donnot push; 0: Old PC, push if time reach; 1: New PC, push
static int mac_check(const unsigned char *addr, int *push_index)
{
    struct hlist_node   *nd = NULL;
    struct mac_entry    *entry = NULL;
    struct mac_entry    *me = NULL;
    struct hlist_head *head = &hash[mac_hash(addr)];
    int ret = -1;

    if (!is_valid_macaddr(addr))
        return ret;

    rcu_read_lock();
    hlist_for_each_entry_rcu(entry, nd, head, hlist) {
        if (!compare_ether_addr(entry->addr, addr))
        {
            me = entry;
            break;
        }
    }
    
    //found, base on the time to push web or not
    if (me)
    {
        unsigned long now = jiffies;    //jiffies is volatile

   //     NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, ">>>MAC list found\n");

        if (push_enable[PUSH_CT]=='1' && push_enable[PUSH_ZTE]=='1')
        {
            //CT < ZTE, just push CT, CT is high priority
            if (push_age[PUSH_CT] < push_age[PUSH_ZTE])
            {
                if ( (now-me->portaltime[PUSH_CT]) > msecs_to_jiffies(push_age[PUSH_CT]) )
                {
                    me->portaltime[PUSH_CT]= now;
                    me->portaltime[PUSH_ZTE]= now;
                    *push_index = PUSH_CT;
                    ret = 0;
                }
                else
                {
                    ret = -1;
                }
            }
            //CT = ZTE, alternate
            else if (push_age[PUSH_CT] == push_age[PUSH_ZTE])
            {
                if ( ((now-me->portaltime[PUSH_CT]) > (msecs_to_jiffies(push_age[PUSH_CT])/2) ) 
                    && (me->portaltime[PUSH_ZTE] == 0) )
                {
                    me->portaltime[PUSH_ZTE]= now;
                    *push_index = PUSH_ZTE;
                    ret = 0;
                }
                else if ( (now-me->portaltime[PUSH_CT]) > msecs_to_jiffies(push_age[PUSH_CT]) )
                {
                    me->portaltime[PUSH_CT]= now;
                    me->portaltime[PUSH_ZTE]= 0;
                    *push_index = PUSH_CT;
                    ret = 0;
                }
                else
                {
                    ret = -1;
                }
            }
            //CT > ZTE, independence
            else
            {
                if ( (now-me->portaltime[PUSH_CT]) > msecs_to_jiffies(push_age[PUSH_CT]) )
                {
                    me->portaltime[PUSH_CT]= now;
                    me->portaltime[PUSH_ZTE]= now;
                    *push_index = PUSH_CT;
                    ret = 0;
                }
                else if ( (now-me->portaltime[PUSH_ZTE]) > msecs_to_jiffies(push_age[PUSH_ZTE]) )
                {
                    me->portaltime[PUSH_ZTE]= now;
                    *push_index = PUSH_ZTE;
                    ret = 0;
                }
                else
                {
                    ret = -1;
                }
            }
        }
        else if (push_enable[PUSH_CT] == '1')
        {
            if ( (now-me->portaltime[PUSH_CT]) > msecs_to_jiffies(push_age[PUSH_CT]) )
            {
                me->portaltime[PUSH_CT]= now;
                me->portaltime[PUSH_ZTE]= now;
                *push_index = PUSH_CT;
                ret = 0;
            }
            else
            {
                ret = -1;
            }
        }
        else if (push_enable[PUSH_ZTE] == '1')
        {
            if ( (now-me->portaltime[PUSH_ZTE]) > msecs_to_jiffies(push_age[PUSH_ZTE]) )
            {
                me->portaltime[PUSH_ZTE]= now;
                *push_index = PUSH_ZTE;
                ret = 0;
            }
            else
            {
                ret = -1;
            }
        }
        else
        {
            //all push state are disabled
            ret = -1;
        }
        
        me->activetime = now;
    }
    //not found, new PC
    else
    {
        NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, ">>>MAC list not found\n");
        
        if (push_enable[PUSH_CT] == '1')
        {
            *push_index = PUSH_CT;
            ret = 1;
        }
        else if (push_enable[PUSH_ZTE] == '1')
        {
            *push_index = PUSH_ZTE;
            ret = 1;
        }
        else
        {
            ret = -1;
        }
    }
    rcu_read_unlock();
    
  //  NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "mac_check: ret=%d, push_index=%d\n", ret, *push_index);
    
    return ret;
}

static int mac_add(const unsigned char *addr)
{
    struct mac_entry    *entry = NULL;
    struct hlist_head *head = &hash[mac_hash(addr)];
    unsigned long now = jiffies;

    entry = (struct mac_entry *)kmalloc(sizeof(struct mac_entry), GFP_ATOMIC);
    if (!entry)
        return -1;
    memset(entry, 0, sizeof(struct mac_entry));

    INIT_HLIST_NODE(&entry->hlist);
    INIT_RCU_HEAD(&entry->rcu);
    memcpy(entry->addr, addr, ETH_ALEN);
    if (push_age[PUSH_CT] == push_age[PUSH_ZTE])
    {
        entry->portaltime[PUSH_CT] = now;
        entry->portaltime[PUSH_ZTE] = 0;
        entry->activetime= now;
    }
    else
    {
        entry->portaltime[PUSH_CT] = now;
        entry->portaltime[PUSH_ZTE] = now;
        entry->activetime= now;
    }
    spin_lock_bh(hash_lock);
    hlist_add_head_rcu(&entry->hlist, head);
    spin_unlock_bh(hash_lock);

    return 0;
}

static void maclist_rcu_free(struct rcu_head *head)
{
    struct mac_entry *me = container_of(head, struct mac_entry, rcu);
    if (me)
        kfree(me);
}

static int mac_del(void)
{
    struct hlist_node   *h = NULL;
    struct hlist_node   *n = NULL;
    struct mac_entry    *entry = NULL;
    int i;

    spin_lock_bh(hash_lock);
    for (i = 0; i < MAC_HASH_SIZE; i++) 
    {
        hlist_for_each_entry_safe(entry, h, n, &hash[i], hlist) {
            //CT is high priority, so delete maclisth by it's age
            if ( (jiffies - entry->activetime) > msecs_to_jiffies(push_age[PUSH_CT]) )
            {
                hlist_del_rcu(&entry->hlist);
                call_rcu(&entry->rcu, maclist_rcu_free);
            }
        }
    }
    spin_unlock_bh(hash_lock);
    
    return 0;
}

static int mac_destory(void)
{
    struct hlist_node   *h = NULL;
    struct hlist_node   *n = NULL;
    struct mac_entry    *entry = NULL;
    int i;

    spin_lock_bh(hash_lock);
    for (i = 0; i < MAC_HASH_SIZE; i++) 
    {
        hlist_for_each_entry_safe(entry, h, n, &hash[i], hlist) {
            hlist_del_rcu(&entry->hlist);
            call_rcu(&entry->rcu, maclist_rcu_free);
        }
    }
    spin_unlock_bh(hash_lock);
    
    return 0;
}

#if 0
static int pushweb_send_packet(const struct sk_buff *skb1,char* payload,short payload_len,short checksum,
                              const struct net_device *in, const struct net_device *out)
{
    struct sk_buff *skb2 = NULL;
    if(skb1 == NULL || in == NULL)
        return -1;

    skb2 = alloc_skb(payload_len, GFP_ATOMIC);
    if (skb2 == NULL)
    {
        NM_PUSH_OUTPUT(NM_LEVEL_ERROR, "pushweb_send_packet(): alloc_skb error!\n");
        return -1;
    }
    NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "pushweb_send_packet: in device is %s\n", in->name);
    skb2->dev = (struct net_device *)in;

    skb2->len = payload_len;
    skb2->csum = (int)checksum;
    memcpy(skb2->data, payload, payload_len);
    skb2->tail = skb2->data+payload_len;
     skb2->mac.raw = skb2->data;
    skb2->head = skb2->data;
    if((skb2->dev) == NULL)
    {
        NM_PUSH_OUTPUT(NM_LEVEL_ERROR, "pushweb_send_packet: bad dev");
        kfree_skb(skb2);
        return -1;
    }

    if (dev_queue_xmit(skb2) < 0)
        kfree_skb(skb2);

    return 0;
}

static int pushweb_send_resp(const struct sk_buff *skb, const struct net_device *in, const struct net_device *out)
{
    unsigned int packet_len = 0, datalen = 0;
    //char auth_resp[MAX_AUTHRESP_DATELEN];
    char *auth_resp = NULL;
    char *ptemp = NULL, *pppoe = NULL, *ptcp = NULL;
    struct iphdr iph;
    struct iphdr *old_iph = NULL;
    struct tcphdr tcph;
    struct tcphdr *oldtcph = NULL;
    struct ethhdr *mac = NULL;

    old_iph = (struct iphdr *)skb_network_header_layer(skb);

    auth_resp = kmalloc(MAX_AUTHRESP_DATELEN, GFP_ATOMIC);
    if(auth_resp == NULL)
        return -1;
    ptemp = auth_resp;
    memset(auth_resp, 0, MAX_AUTHRESP_DATELEN);
    memset(&iph, 0, sizeof(iph));
    memset(&tcph, 0, sizeof(tcph));

    /* get mac info. */
    mac = eth_hdr(skb);
    memcpy(ptemp, mac->h_source, ETH_ALEN);
    memcpy(ptemp + ETH_ALEN, mac->h_dest, ETH_ALEN);
    memcpy(ptemp + ETH_ALEN + ETH_ALEN, &mac->h_proto, sizeof(mac->h_proto));
    ptemp += sizeof(*mac);
    /* get pppoe info if needed. */
    if (mac->h_proto == htons(0x8864)) {
        pppoe = (char *)(mac + sizeof(*mac));
        memcpy(ptemp, "\x11\x00", 2);
        memcpy(ptemp + 2, pppoe + 2, 2);
        memcpy(ptemp + 4, "\x01\x3f", 2);
        memcpy(ptemp + 6, pppoe + 6, 2);
        ptemp += 8;
    }

    /* get ip header. */
    iph.ihl = 5;
    iph.version = 4;
    iph.tos = 0;
    iph.tot_len = htons(sizeof(iph) + sizeof(tcph));
    iph.id = 0;                 /* no useful when Don't fragment.  */
    iph.frag_off = htons(IP_DF);
    iph.ttl = 128;
    iph.protocol = 0x06;        /* IPPROTO_TCP */
    iph.saddr = old_iph->daddr;
    iph.daddr = old_iph->saddr;
    iph.check=ip_fast_csum((unsigned char *)&iph, iph.ihl);    

    /* get tcp header. */
    oldtcph = (struct tcphdr *)((char *)old_iph + sizeof(struct iphdr));
    tcph.urg_ptr = 0;
    tcph.window = htons(4096);
    
    tcph.source = oldtcph->dest;
    tcph.dest = oldtcph->source;
    tcph.seq = oldtcph->ack_seq;
    tcph.doff = sizeof(struct tcphdr)/4;
    tcph.ack = 1;               /* only ack flag. */
    tcph.ack_seq = htonl(ntohl(oldtcph->seq)
                         + ntohs(old_iph->tot_len)
                         - ((old_iph->ihl)<<2)
                         - ((oldtcph->doff)<<2));
    /* Adjust TCP checksum. */
    tcph.check = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
    tcph.check = tcp_v4_check(sizeof(struct tcphdr), iph.saddr, iph.daddr, 
                              csum_partial((char *)&tcph, sizeof(struct tcphdr), 0));
#else    
    tcph.check = tcp_v4_check(&tcph, sizeof(struct tcphdr), iph.saddr, iph.daddr, 
                              csum_partial((char *)&tcph, sizeof(struct tcphdr), 0));
#endif

    /* send ack packet. */
    memcpy(ptemp, &iph, sizeof(iph));
    memcpy(ptemp + sizeof(iph), &tcph, sizeof(tcph));
    packet_len = ptemp - auth_resp + sizeof(iph) + sizeof(tcph);
    pushweb_send_packet(skb, auth_resp, packet_len, iph.check, in, out);

    /* send redirect packet. */
    /* change ip header. */
    iph.tot_len = htons(sizeof(iph) + sizeof(tcph) + strlen(user_auth_http_resp));
    iph.check = 0;
    iph.check=ip_fast_csum((unsigned char *)&iph, iph.ihl);
    
    /* change tcp header. */
    tcph.fin = 1; tcph.psh = 1; tcph.ack = 1; /* fin psh ack ! */
    tcph.check = 0;
    memcpy(ptemp, &iph, sizeof(iph));
    memcpy(ptemp + sizeof(iph), &tcph, sizeof(tcph));
    memcpy(ptemp + sizeof(iph) + sizeof(tcph), user_auth_http_resp, strlen(user_auth_http_resp));
    packet_len = ptemp - auth_resp + sizeof(iph) + sizeof(tcph) + strlen(user_auth_http_resp);
    ptcp = ptemp + sizeof(iph);
    datalen = sizeof(tcph) + strlen(user_auth_http_resp);
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
    tcph.check = tcp_v4_check(datalen, iph.saddr, iph.daddr,
                              csum_partial(ptcp, datalen, 0));
#else    
    tcph.check = tcp_v4_check((struct tcphdr *)ptcp, datalen, iph.saddr, iph.daddr,
                              csum_partial(ptcp, datalen, 0));
#endif
    memcpy(ptemp + sizeof(iph), &tcph, sizeof(tcph)); /* update tcph.check. */
    pushweb_send_packet(skb, auth_resp, packet_len, iph.check, in, out);
    kfree(auth_resp);
    
    return 0;
}
#endif


static int pushweb_send_packet(const struct sk_buff *oldskb, char* payload, int payload_len, int check)
{
    int reserve = 32;           /* 14(MAC) 4(vlan) 4(ppp) */
    struct sk_buff *skb2 = NULL;

    skb2 = alloc_skb(payload_len + reserve, GFP_ATOMIC);
    if (skb2 == NULL) {
        NM_PUSH_OUTPUT(NM_LEVEL_ERROR, "portal_send_resp(): alloc_skb error!\n");
        return -1;
    }
    /* reserve for MAC VLAN PPP */
    skb_reserve(skb2, reserve);
    /* fill ip and tcp and app */
    memcpy(skb2->data, payload, payload_len);
    skb2->tail = skb2->data + payload_len;
    skb2->len = payload_len;
	skb_reset_network_header(skb2);
    skb2->csum = check;

	/* ip_route_me_harder expects skb->dst to be set */
	skb_dst_set(skb2, dst_clone(skb_dst(oldskb)));
    
    if (ip_route_me_harder(skb2, RTN_UNSPEC)) {
        NM_PUSH_OUTPUT(NM_LEVEL_INFO, "(%s): no route\n", __FUNCTION__);
        goto free_skb;
    }

    /* "Never happens" */
    if (skb2->len > dst_mtu(skb_dst(skb2))) {
        NM_PUSH_OUTPUT(NM_LEVEL_INFO, "(%s): wrong len(%d)\n", __FUNCTION__, skb2->len);         
        goto free_skb;
    }

    nf_ct_attach(skb2, (struct sk_buff *)oldskb);
    
    NM_PUSH_OUTPUT(NM_LEVEL_INFO, "(%s): send packet through dev(%s)\n", __FUNCTION__, skb_dst(skb2)->dev->name);
    ip_local_out(skb2);
    
    return 0;
    
free_skb:
    kfree_skb(skb2);
    return -1;
}

static int pushweb_send_resp(const struct sk_buff *skb, const struct net_device *in, const struct net_device *out)
{
    unsigned int packet_len = 0, datalen = 0;
    //char auth_resp[MAX_AUTHRESP_DATELEN];
    char *auth_resp = NULL;
    char *ptemp = NULL, *ptcp = NULL;
    struct iphdr iph;
    struct iphdr *old_iph = NULL;
    struct tcphdr tcph;
    struct tcphdr *oldtcph = NULL;

    old_iph = (struct iphdr *)skb_network_header_layer(skb);

    auth_resp = kmalloc(MAX_AUTHRESP_DATELEN, GFP_ATOMIC);
    if(auth_resp == NULL)
        return -1;
    ptemp = auth_resp;
    memset(auth_resp, 0, MAX_AUTHRESP_DATELEN);
    memset(&iph, 0, sizeof(iph));
    memset(&tcph, 0, sizeof(tcph));

    /* get ip header. */
    iph.ihl = 5;
    iph.version = 4;
    iph.tos = 0;
    iph.tot_len = htons(sizeof(iph) + sizeof(tcph));
    iph.id = 0;                 /* no useful when Don't fragment.  */
    iph.frag_off = htons(IP_DF);
    iph.ttl = 128;
    iph.protocol = 0x06;        /* IPPROTO_TCP */
    iph.saddr = old_iph->daddr;
    iph.daddr = old_iph->saddr;
    iph.check=ip_fast_csum((unsigned char *)&iph, iph.ihl);    

    /* get tcp header. */
    oldtcph = (struct tcphdr *)((char *)old_iph + sizeof(struct iphdr));
    tcph.urg_ptr = 0;
    tcph.window = htons(4096);
    
    tcph.source = oldtcph->dest;
    tcph.dest = oldtcph->source;
    tcph.seq = oldtcph->ack_seq;
    tcph.doff = sizeof(struct tcphdr)/4;
    tcph.ack = 1;               /* only ack flag. */
    tcph.ack_seq = htonl(ntohl(oldtcph->seq)
                         + ntohs(old_iph->tot_len)
                         - ((old_iph->ihl)<<2)
                         - ((oldtcph->doff)<<2));
    /* Adjust TCP checksum. */
    tcph.check = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
    tcph.check = tcp_v4_check(sizeof(struct tcphdr), iph.saddr, iph.daddr, 
                              csum_partial((char *)&tcph, sizeof(struct tcphdr), 0));
#else    
    tcph.check = tcp_v4_check(&tcph, sizeof(struct tcphdr), iph.saddr, iph.daddr, 
                              csum_partial((char *)&tcph, sizeof(struct tcphdr), 0));
#endif

    /* send redirect packet. */
    /* change ip header. */
    iph.tot_len = htons(sizeof(iph) + sizeof(tcph) + strlen(user_auth_http_resp));
    iph.check = 0;
    iph.check=ip_fast_csum((unsigned char *)&iph, iph.ihl);
    
    /* change tcp header. */
    tcph.fin = 1; tcph.psh = 1; tcph.ack = 1; /* fin psh ack ! */
    tcph.check = 0;
    memcpy(ptemp, &iph, sizeof(iph));
    memcpy(ptemp + sizeof(iph), &tcph, sizeof(tcph));
    memcpy(ptemp + sizeof(iph) + sizeof(tcph), user_auth_http_resp, strlen(user_auth_http_resp));
    packet_len = ptemp - auth_resp + sizeof(iph) + sizeof(tcph) + strlen(user_auth_http_resp);
    ptcp = ptemp + sizeof(iph);
    datalen = sizeof(tcph) + strlen(user_auth_http_resp);
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
    tcph.check = tcp_v4_check(datalen, iph.saddr, iph.daddr,
                              csum_partial(ptcp, datalen, 0));
#else    
    tcph.check = tcp_v4_check((struct tcphdr *)ptcp, datalen, iph.saddr, iph.daddr,
                              csum_partial(ptcp, datalen, 0));
#endif
    memcpy(ptemp + sizeof(iph), &tcph, sizeof(tcph)); /* update tcph.check. */
    pushweb_send_packet(skb, auth_resp, packet_len, iph.check);
    kfree(auth_resp);
    
    return 0;
}


static int pushweb_get_resp(const struct net_device *input_dev, const int push_index)
{
    struct in_device *in_dev = NULL;
    
    if (input_dev == NULL)
        return -1;
    
    in_dev = in_dev_get(input_dev);
    if (in_dev == NULL || in_dev->ifa_list == NULL)
    {
        NM_PUSH_OUTPUT(NM_LEVEL_WARNING, "pushweb_get_resp: in_dev NULL or has no ip addr.\n");
        in_dev_put(in_dev);
        return -1;
    }
    in_dev_put(in_dev);
    
    memset(user_auth_http_resp,0,MAX_AUTHRESP_DATELEN);

    if(push_url[push_index][0] == '\0')
    {
        NM_PUSH_OUTPUT(NM_LEVEL_WARNING, "pushweb_get_resp: No valid push web(%d) information\n", push_index);
        return -1;
    }
    else
    {
        //len = strlen(push_url[push_index]) + AUTHRESP_DEFAULTLEN + 3;
        //Charles: actually, the Content-Length is 0 becaues of no html content
        snprintf(user_auth_http_resp,MAX_AUTHRESP_DATELEN,"HTTP/1.1 302 Found\r\nDate: Thu, 01 May 2008 12:00:00 GMT\r\n"
                    "Server: Apache/2.2.3 (Debian) PHP/5.2.0-10\r\nLocation: %s\r\nContent-Length: 0\r\n"
                    "Keep-Alive: timeout=10, max=100\r\nConnection: Keep-Alive\r\nContent-Type: text/html; charset=iso-8859-1\r\n\r\n",
                    push_url[push_index]);
    }
    return 0;
}

/*
static unsigned short tcp_checksum(unsigned short *buffer, int size)
{          
    unsigned long check=0;
    while(size>1)          
    {
        check += *buffer++;
        size -= sizeof(unsigned short);
    }
    if(size)
    {
        check += *(unsigned short*)buffer;
    }          
    check = (check >>16) + (check & 0xffff);
    check += (check >>16);
    return  (unsigned short)(~check);
}
*/

static int http_revise_sn(const struct sk_buff *skb, unsigned char * skb_trans_raw, unsigned char* skb_application_raw)
{
    unsigned char *skb_applitcation_data = skb_application_raw;
    unsigned char *http_head_host = NULL;
    int skb_appplayload_len = 0;
    unsigned char *dev_sn_start = NULL;
    int source_dev_sn_len = 0;
    struct tcphdr *tcph;
    struct iphdr *iph;
/*
    struct PSDHEADER psdHeader;
    unsigned char auth_resp[MAX_BIZ_LEN];
    struct TCPHEADER *tcpHeader = NULL;
    struct IPHEADER *ipHeader = NULL;
    int checklen = 0;
*/
    if (NULL == skb_application_raw)
        return -1;

    if (push_url[PUSH_ZTE][0] == '\0')
        return -1;
    
    //memset(auth_resp,0,MAX_BIZ_LEN);
    //memset(&psdHeader,0,sizeof(psdHeader));
    skb_appplayload_len = skb->tail - skb_application_raw;
    
    //tcpHeader = (struct TCPHEADER *)skb_trans_raw;
    //ipHeader = (struct IPHEADER *)skb->nh.raw;
    
    // search "Host:"
    if ((http_head_host = memmem(skb_applitcation_data, skb_appplayload_len, DIGEST_HTTP_HEAD_HOST, strlen(DIGEST_HTTP_HEAD_HOST))) != NULL)
    {
        // compare URl
        int host_len = strcspn(push_url[PUSH_ZTE] + 7, "/");
        if( 0 != strncmp(http_head_host + (strlen(DIGEST_HTTP_HEAD_HOST) + 1), push_url[PUSH_ZTE] + 7, host_len))
        {
            NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "ZTE push URL=%s, host length=%d\n", push_url[PUSH_ZTE], host_len);
            return -1;
        }
        dev_sn_start = memmem(skb_applitcation_data , http_head_host - skb_applitcation_data, DIGEST_HTTP_GET_END, strlen(DIGEST_HTTP_GET_END));
        if(NULL == dev_sn_start)
            return -1;
        dev_sn_start -= 1;
        source_dev_sn_len = dev_sn_start - (skb_applitcation_data + strlen(DIGEST_HTTP_HEAD_GET) + 2);
        dev_sn_start = memmem(skb_applitcation_data + strlen(DIGEST_HTTP_HEAD_GET) + 2, source_dev_sn_len, SOURCE_DEV_SN, MAX_SN_LEN);
        if (NULL == dev_sn_start)
        {
            NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "sourc device sn=%d, device sn=%s\n", source_dev_sn_len, dev_sn);
            return -1;
        }
        //memcpy(skb_applitcation_data + strlen(DIGEST_HTTP_HEAD_GET) + 2, dev_sn, MAX_SN_LEN);
        memcpy(dev_sn_start, dev_sn, MAX_SN_LEN);
#if 0
        psdHeader.saddr=ipHeader->sourceIP;
        psdHeader.daddr=ipHeader->destIP;
        psdHeader.mbz=0;
        psdHeader.ptcl=0x06;//IPPROTO_TCP;
        psdHeader.tcpl=htons(sizeof(struct TCPHEADER)); //? TCP length not TCPHEADER length
            
        memcpy(auth_resp, &psdHeader, sizeof(psdHeader));
        memcpy(auth_resp + sizeof(psdHeader), tcpHeader, sizeof(struct TCPHEADER));            
        strncpy(auth_resp + sizeof(psdHeader) + sizeof(struct TCPHEADER), skb_applitcation_data, skb_appplayload_len);
        checklen = sizeof(psdHeader) + sizeof(struct TCPHEADER) + skb_appplayload_len;
        tcpHeader->th_sum=tcp_checksum((unsigned short *)auth_resp, checklen);
        tcpHeader->th_sum = htons(ntohs(tcpHeader->th_sum)-(unsigned short)(skb_appplayload_len));
#endif
        iph = (struct iphdr *)skb_network_header_layer(skb);
        tcph = (void *)iph + iph->ihl*4;
        tcph->check = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
        tcph->check = tcp_v4_check(skb->len - iph->ihl*4, iph->saddr, iph->daddr,
                                   csum_partial((char *)tcph, skb->len - iph->ihl*4, 0));
#else
        tcph->check = tcp_v4_check(tcph, skb->len - iph->ihl*4, iph->saddr, iph->daddr,
                                   csum_partial((char *)tcph, skb->len - iph->ihl*4, 0));
#endif
    }

    return 0;
}

static int check_accept(char *app_buf, int app_len)
{
    char *line_s = NULL, *line_e = NULL;
    int line_len = 0;
    int ret = 0;

    line_s = memmem(app_buf, app_len, "Accept: ", sizeof("Accept: ")-1);
    if(line_s){
        line_e = memmem(line_s, app_len-(line_s-app_buf), "\r\n",sizeof("\r\n")-1);
        if(line_e){
            line_len = line_e - line_s;
            if(line_e = memmem(line_s, line_len, ";",1)){
                line_len = line_e - line_s;
            }

            if(memmem(line_s, line_len, "text/html", sizeof("test/html")-1)) {
                ret = 1;
                NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "check_accept: ret=%d \n", ret);
            }
        }
    }

end:
    return ret;
}

static int push_web(const struct sk_buff *skb, unsigned char *app_buf, int app_len,
                       const struct net_device *in, const struct net_device *out, int mode)
{    
    struct my_conntrack *ct = nm_get_myconntrack(skb);
    unsigned char * skb_trans_raw = NULL;
    int ip_hl = 0;
    int accept_flag = 0;
    
    if(ct == NULL)
    {
        NM_PUSH_OUTPUT(NM_LEVEL_ERROR, "push_web: skb NULL pointer\n");
        goto end;
    }
    
    if(app_buf == NULL){
        goto end;
    }
    
    if ( skb->dir == PACKET_DIRECT_OUT 
        /*&& mode != SMP_RUN_IN_MIRROR */
        && ct->http_flag == 1 )
    {
        if( !memcmp(app_buf, "GET /", sizeof("GET /")-1) )
        {
            unsigned char *srcmac = NULL;
            int ret = -1;
            int push_index = PUSH_NUM;

            accept_flag = check_accept(app_buf, app_len);

            if(!accept_flag){
                goto end;
            }
            
            srcmac = skb_mac_header_layer(skb) + 6;
            ret = mac_check(srcmac, &push_index);
            //Passive push: Old PC, arrive push time
            if (ret >= 0)
            {
                NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "push_web: MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
                                        *srcmac, *(srcmac+1), *(srcmac+2), *(srcmac+3), *(srcmac+4), *(srcmac+5));
                NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "push_web: ret=%d, push_index=%d\n", ret, push_index);
                if (pushweb_get_resp(in, push_index) != 0)
                {
                    NM_PUSH_OUTPUT(NM_LEVEL_WARNING, "push_web: pushweb_get_resp return error\n");
                    goto end;
                }
                if (pushweb_send_resp(skb, in, out) != 0) 
                {
                    NM_PUSH_OUTPUT(NM_LEVEL_WARNING, "push_web: pushweb_send_resp return error\n");
                    goto end;
                }
                //new PC
                if (ret == 1)
                {
                    mac_add(srcmac);
                }
            }
            //Active push: Add SN info if URL is ZTE bizstore web
            else
            {
/*                 eth_head_len = ((*(skb->nh.raw) & 0x0F) * 4); */
/*                 skb_trans_raw = skb->nh.raw + eth_head_len; */
                ip_hl = (ip_hdr_layer(skb)->ihl)<<2;
                skb_trans_raw = skb_network_header_layer(skb) + ip_hl;
                http_revise_sn(skb, skb_trans_raw, app_buf);
            }
        }
    }

end:    
    return NM_MAIN_CONTINUE;
}

static int push_response(int type, char *buf, int len)
{
    int ret = 0;
    
    switch(type)
    {
        case REQ_DEBUG_SWITCH_SET:
            ret = nm_set_debug(buf, len, NM_MODULE_PUSHWEB, &pushweb_debug);
            break;
        default:
            break;
    }
    
    NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "push_response.\n");
    return ret;
}

static int pushweb_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data) 
{
    int len = 0;
    if (0 == strcmp(data, PROC_PUSH_URL))
    {
        len = sprintf(page, "%s%s %s%s\n", FLAG_CT, push_url[PUSH_CT], FLAG_ZTE, push_url[PUSH_ZTE]);
        *eof = 1;
    }
    else if (0 == strcmp(data, PROC_PUSH_AGE))
    {
        len = sprintf(page, "%s%d  %s%d\n", FLAG_CT, push_age[PUSH_CT]/AGE_MSEC_EXCHANGE, FLAG_ZTE, push_age[PUSH_ZTE]/AGE_MSEC_EXCHANGE);
        *eof = 1;
    }
    else if (0 == strcmp(data, PROC_PUSH_ENABLE))
    {
        len = sprintf(page, "%s%c  %s%c\n", FLAG_CT,  push_enable[PUSH_CT], FLAG_ZTE,  push_enable[PUSH_ZTE]);
        *eof = 1;
    }
    else if (0 == strcmp(data, PROC_DEV_SN))
    {
        len = sprintf(page, "%s\n", dev_sn);
        *eof = 1;
    }

    return len;
}

static int pushweb_write_proc(struct file *file, const char __user *buffer, unsigned long len, void *data)
{
    int    flag_index = 0;
    char  strPortal_age[STR_PORTAL_AGE_LEN + 1] = {0};
    int    count = len;
    char  buf[MAX_PROC_BUF+1] = {0};
    char  *pbuf = buf;
    
    NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "pushweb_write_proc: buffer=%.*s\n", (int)len, buffer);
    if(len > MAX_PROC_BUF)
    {
        NM_PUSH_OUTPUT(NM_LEVEL_ERROR, "pushweb_write_proc: buffer over flow\n");
        return -ENOSPC;
    }
    
    memset(buf, 0, MAX_PROC_BUF+1);
    if (copy_from_user(buf, buffer, len))
    {
        NM_PUSH_OUTPUT(NM_LEVEL_ERROR, "pushweb_write_proc: copy_from_user error\n");
        return -EFAULT;
    }
        
    //all write data is start from CT: or ZTE:, to find the real data
    if( 0==memcmp(pbuf, FLAG_CT, strlen(FLAG_CT)) )
    {
        flag_index = PUSH_CT;
        pbuf += strlen(FLAG_CT);
        count -= strlen(FLAG_CT);
    }
    else if( 0==memcmp(pbuf, FLAG_ZTE, strlen(FLAG_ZTE)) )
    {
        flag_index = PUSH_ZTE;
        pbuf += strlen(FLAG_ZTE);
        count -= strlen(FLAG_ZTE);
    }
    else if( 0==memcmp(pbuf, FLAG_SN, strlen(FLAG_SN)) )
    {
        flag_index = PUSH_NUM;
    }
    else
    {
        NM_PUSH_OUTPUT(NM_LEVEL_ERROR, "pushweb_write_proc: data format error, must start with CT: or ZTE:\n");
        return len;
    }
    if( count == 0 && strcmp(data, PROC_PUSH_URL) != 0)
    {
        NM_PUSH_OUTPUT(NM_LEVEL_ERROR, "pushweb_write_proc: data empty\n");
        return len;
    }
    
    if (0 == strcmp(data, PROC_PUSH_URL))
    {
        if (count > MAX_URL_LEN)
        {
            count = MAX_URL_LEN;
        }
        
        memcpy(push_url[flag_index], pbuf, count);
        push_url[flag_index][count] = '\0';
        NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "URL: %s \n",  push_url[flag_index]);
        //change the push URL will push again
        if( flag_index == PUSH_CT)
        {
            mac_destory();
            NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "pushweb_write_proc: MAC list destory, clear push info\n");
        }
    }
    else if (0 == strcmp(data, PROC_PUSH_AGE))
    {
        if (count > STR_PORTAL_AGE_LEN)
        {
            count = STR_PORTAL_AGE_LEN;
        }
        
        memcpy(strPortal_age, pbuf, count);
        strPortal_age[count] = '\0';
        push_age[flag_index] = simple_strtol(strPortal_age, NULL, 10);
        //bigger than 10 days will overflow
        if (push_age[flag_index] > MAX_PUSH_AGE)
            push_age[flag_index] = MAX_PUSH_AGE;
        NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "AGE: %d \n",  push_age[flag_index]);
        push_age[flag_index] = push_age[flag_index] * AGE_MSEC_EXCHANGE;
    }
    else if (0 == strcmp(data, PROC_PUSH_ENABLE))
    {
        count = 1;
        memcpy(&push_enable[flag_index], pbuf, count);
        NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "Enable: %c \n", push_enable[flag_index]);
        //CT enable is 0 will clear push info
        if( (flag_index == PUSH_CT) && (push_enable[flag_index] == '0' ) )
        {
            mac_destory();
            NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "pushweb_write_proc: MAC list destory, clear push info\n");
        }
    }
    else if (0 == strcmp(data, PROC_DEV_SN))
    {
        if (count > MAX_SN_LEN)
        {
            count = MAX_SN_LEN;
        }
        
        memcpy(dev_sn, pbuf, count);
        dev_sn[count] = '\0';
        NM_PUSH_OUTPUT(NM_LEVEL_DEBUG, "SN: %s \n", dev_sn);
    }

    //the following line will affect the judgement of push time; just open it in testing
    //mac_destory();

    return len;
}

static int pushweb_create_proc(void) 
{
    int i = 0;

    pd = proc_mkdir(PROC_MAIN, NULL);
    if (NULL == pd)
    {
        NM_PUSH_OUTPUT(NM_LEVEL_WARNING, "%s : create proc directory failed", PROC_MAIN);
        return URL_REDIR_CREATE_PROC_FAILED;
    }
/*     pd->owner = THIS_MODULE; */

    for (i = 0; i < PROC_SIZE; i++)
    {
        proc_items[i].pe = create_proc_entry(proc_items[i].item_name, PROC_ACCESS_VALUE, pd);
        if (NULL == proc_items[i].pe)
        {
            NM_PUSH_OUTPUT(NM_LEVEL_WARNING, "%s : create proc entry (%s) failed", PROC_MAIN, proc_items[i].item_name);
            return URL_REDIR_CREATE_PROC_FAILED;
        }
        proc_items[i].pe->read_proc = pushweb_read_proc;
        proc_items[i].pe->write_proc = pushweb_write_proc;
/*         proc_items[i].pe->owner = THIS_MODULE; */
        proc_items[i].pe->data = proc_items[i].item_name;
    }

    return URL_REDIR_OK;
}

static void aging_portal(unsigned long data)
{
    unsigned long now = jiffies;
    NM_PUSH_OUTPUT(NM_LEVEL_INFO, "jiffies:%ld \n", now );
    mac_del();
    if(now < timebefore)    //jiffies maybe overflow(circle) to 0
    {
        mac_destory();
    }
    timebefore = now;
    mod_timer(&tl_aging_portal, now + msecs_to_jiffies(CHECK_VALIDMAC_INTERVAL));
}

static int url_redirection_init(void)
{
    int ret = URL_REDIR_OK;
    
    memset(push_url, 0, sizeof(push_url));
    memset(push_age, 0, sizeof(push_age));
    memset(push_enable, '0', sizeof(push_enable));
    memset(dev_sn, 0, sizeof(dev_sn));
    
    timebefore = 0;
        
    ret = pushweb_create_proc();
    if (URL_REDIR_OK != ret)
    {
        return ret;
    }

    init_completion(&timer_done);
    setup_timer(&tl_aging_portal, aging_portal, 0);
    ret = mod_timer(&tl_aging_portal, jiffies + msecs_to_jiffies(CHECK_VALIDMAC_INTERVAL));
    if (ret)
    {
        NM_PUSH_OUTPUT(NM_LEVEL_WARNING, "%s : Failed to arm portal aging timer: %i\n", PROC_MAIN, ret);
        ret = URL_REDIR_ARM_TIMER_FAILED;
    }

    return ret;
}

static int pushweb_unload_proc(void)
{
    int i = 0;
    int ret = URL_REDIR_OK;

    for (i = 0; i < PROC_SIZE; i++)
    {
        if (NULL != proc_items[i].pe)
        {
            remove_proc_entry(proc_items[i].item_name, pd);
        }
    }
    remove_proc_entry(PROC_MAIN, NULL);

    return ret;
}

static int url_redirection_exit(void)
{
    int ret = URL_REDIR_OK;

    ret = pushweb_unload_proc();
    del_timer(&tl_aging_portal);

    mac_destory();
    return ret;
}

static struct nm_process push_process[] = {
    {
        .priority = NM_PRIORITY_PAE + 1, /* after protocol analysis. */
        .analysis = push_web, 
    }, 
};

static struct nm_message push_message[] = {
    {
        .type = 0,
        .mask = 0,
        .analysis = push_response, 
    }, 
};

static int __init pushweb_init (void)
{
    int ret = URL_REDIR_OK;
        
    /* register message hook. */
    ret = nm_register_message(push_message, sizeof(push_message)/sizeof(push_message[0]));
    if (ret) {
        NM_PUSH_OUTPUT(NM_LEVEL_ERROR, "failed to init pushweb module: cannot register message\n");
        goto end;
    }

    /* register process hook. */
    ret = nm_register_process(push_process, sizeof(push_process)/sizeof(push_process[0]));
    if (ret)
    {
        NM_PUSH_OUTPUT(NM_LEVEL_ERROR, "failed to init pushweb module: cannot register process\n");
        goto clean_message_hook;
    }
    
    mac_hash_init();
    
    ret = url_redirection_init();
    if (URL_REDIR_OK != ret)
    {
        goto clean_process_hook;
    }
    
#if DEBUG
    NM_PUSH_OUTPUT(NM_LEVEL_NOTICE, "Successful to init pushweb module\n");
#endif    
    return 0;

clean_process_hook:
    nm_unregister_process(push_process, sizeof(push_process)/sizeof(push_process[0]));
clean_message_hook:
    nm_unregister_message(push_message, sizeof(push_message)/sizeof(push_message[0]));
end:
    return ret;
}

static void __exit pushweb_exit(void)
{
    int ret = URL_REDIR_OK;
    
    nm_unregister_process(push_process, sizeof(push_process)/sizeof(push_process[0]));
    nm_unregister_message(push_message, sizeof(push_message)/sizeof(push_message[0]));
    
    ret = url_redirection_exit();
    if (URL_REDIR_OK != ret)
    {
        NM_PUSH_OUTPUT(NM_LEVEL_NOTICE, "url_redirection_exit failed\n");
    }
    
    synchronize_rcu();

    NM_PUSH_OUTPUT(NM_LEVEL_NOTICE, "Successful to exit pushweb module\n");    
}


module_init(pushweb_init);
module_exit(pushweb_exit);

MODULE_LICENSE("GPL");

