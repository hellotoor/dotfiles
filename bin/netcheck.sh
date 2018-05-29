#! /bin/sh
echo "Start checking network status..."

default=$(ip route | grep default)
if [ -z "$default" ]; then
    echo "No default route!"
else 
    addr=$(echo "$default" | cut -d' ' -f 3)
    echo "default route: $addr"
fi

ip route | grep -v default > /tmp/tmp_route
#for route in $(echo "$routes")
while read route
do
    if=$(echo "$route" | cut -d' ' -f 3)
    addr=$(echo "$route" | cut -d' ' -f 1)
    addr=${addr%0/*}
    addr=${addr}1
    ret=$(arping -c 3 "$addr" | grep "bytes from")
    if [ -z "$ret" ]; then
        echo "link to $addr is failed!"
    else 
        echo "link to $addr is ok!"
    fi
    ret=$(ping -c 3 114.114.114.114 -I "$if" | grep "bytes from")
    if [ -z "$ret" ]; then
        echo "link to 114.114.114.114 from $if is failed!"
    else 
        echo "link to 114.114.114.114 from $if is ok!"
    fi
done < /tmp/tmp_route

echo "Done!"
