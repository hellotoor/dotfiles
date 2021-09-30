#!/bin/sh

while true
do
    rm -f /tmp/login.log
    nc -l 12345 -vv >> /tmp/login.log 2>> /tmp/login.log
    date >> /tmp/login.log
    key=$(cat /tmp/login.log | grep 'GET /hello')
    addr=$(cat /tmp/login.log  |  grep 'Connection from'  | cut -d '['  -f 2  | cut -d ']' -f 1 )
    if [ -n "$key" ] && [ -n $addr ]; then
        outline pass $addr
    fi

    cat /tmp/login.log >> /tmp/login_his.log
done
