#!/bin/bash

if [ "$1" = "client" ]; then
    kcptun-client -l :8388 -r 108.61.190.67:4000 --mode fast2 --log /tmp/kcptun.log &
    ss-local -s 127.0.0.1 -p 8388 -k Crackme666! -l 1080 -m aes-256-cfb > /tmp/shadowsocks.log  &
else
    /usr/local/bin/ssserver -p 51218 -k Crackme666! -d start --log-file /var/log/shadowsocks.log -v
fi

