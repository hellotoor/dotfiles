#!/bin/bash

[[ "abc" =~ a* ]] && echo 1
[[ "abc" = "a*" ]] && echo 2
[[ "abc" == "a*" ]] && echo 2

[[ "abc" == "abc" ]] && echo 3
[[ "abc" = "abc" ]] && echo 4

/root/Downloads/client_linux_arm7 -l :8388 -r 108.61.190.67:4000 --mode fast2 --log /var/log/kcptun.log &

/usr/local/bin/sslocal  -s 127.0.0.1 -p 8388 -k Crackme666! -d start --log-file /var/log/sslocal.log

