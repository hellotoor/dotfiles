#! /bin/sh

#locale -a
ch=$(locale -a  | grep "en_US.utf8")
if [ -z "$ch" ]; then
    sudo localedef -i en_US -f UTF-8 -v -c --no-archive en_US.UTF-8
fi
export LANG=en_US.utf8 LANGUAGE=en_US.utf8 LC_ALL=en_US.utf8
sudo update-locale  LANG=en_US.utf8 LANGUAGE=en_US.utf8 LC_ALL=en_US.utf8
