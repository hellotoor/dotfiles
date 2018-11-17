#! /bin/sh

iptables -t nat -D POSTROUTING -s 10.8.0.0/24 -j MASQUERADE
