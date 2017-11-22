apt-get install xl2tpd
apt-get install strongswan

mv /etc/xl2tpd/xl2tpd.conf /etc/xl2tpd/xl2tpd.conf.bak 2> /dev/null
cp -f ~/dotfiles/etc/xl2tpd.conf /etc/xl2tpd/xl2tpd.conf

mv /etc/ppp/chap-secrets /etc/ppp/chap-secrets.bak 2> /dev/null
cp -f ~/dotfiles/etc/chap-secrets /etc/ppp/

cp -f ~/dotfiles/etc/options.xl2tpd /etc/ppp/

mv /etc/ipsec.conf  /etc/ipsec.conf.bak
cp -f ~/dotfiles/etc/ipsec.conf /etc/ipsec.conf

#ip = $(ip route get 8.8.8.8 | grep -v cache | cut -d ' ' -f 7)
ip=$(ip route get 8.8.8.8 | awk -F"src" '/src/{gsub(/ /,"");print $2}')
echo '$ip : PSK \"lock123456\"' >> /etc/ipsec.secrets



