#! /bin/sh

apt-get install openvpn -y

cp -fr ./files/* /etc/openvpn/

openvpn --daemon --config /etc/openvpn/server.conf

echo "Done."
