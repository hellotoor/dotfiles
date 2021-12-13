#!/bin/sh
port=50124


mkdir -p /root/work/

#curl -fsSL https://get.docker.com | sh 


#install v2ray
mkdir -p /etc/v2ray/
cd /root/work/ && git clone https://github.com/v2fly/v2ray-examples.git
cp /root/work/v2ray-examples/VMess-mKCPSeed/config_server.json /etc/v2ray/config.json

echo "Please input v2ray listening port:"
echo "Please input vmess id:"
echo "Please input mkcp seed:"

docker pull v2fly/v2fly-core
ufw allow $port
docker run -d --name v2ray -v /etc/v2ray:/etc/v2ray -p 50124:50124/udp v2fly/v2fly-core
