#! /bin/sh

iptables -t nat -A POSTROUTING -s 10.8.0.0/24 -j MASQUERADE
echo 1 > /proc/sys/net/ipv4/ip_forward
