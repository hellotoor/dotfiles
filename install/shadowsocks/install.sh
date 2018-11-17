#! /bin/sh

apt-get install python-pip -y
pip install shadowsocks
#apt–get install python–m2crypto -y
/usr/bin/python /usr/local/bin/ssserver -p 51218 -k Crackme666! -d start --log-file /root/shadowsocks.log -v
