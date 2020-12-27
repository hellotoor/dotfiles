#! /bin/sh

#locale -a
ch=$(locale -a  | grep "en_US.utf8")
if [ -z "$ch" ]
export LANG=en_US.utf8 LANGUAGE=en_US.utf8 LC_ALL=en_US.utf8
update-locale  LANG=en_US.utf8 LANGUAGE=en_US.utf8 LC_ALL=en_US.utf8
