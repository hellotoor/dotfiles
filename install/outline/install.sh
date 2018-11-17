#! /bin/sh

#install docker 
curl -sS https://get.docker.com/ | sh

#start docker
service docker start

#install outline-server
bash -c "$(wget -qO- https://raw.githubusercontent.com/Jigsaw-Code/outline-server/master/src/server_manager/install_scripts/install_server.sh)"

