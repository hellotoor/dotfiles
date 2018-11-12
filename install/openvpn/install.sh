#! /bin/sh

apt-get install openvpn -y

cp -fr ./files/* /etc/openvpn/

openvpn --daemon --config /etc/openvpn/server.conf

iptables -t nat -A POSTROUTING -s 10.8.0.0/24 -j MASQUERADE

echo "Done."
