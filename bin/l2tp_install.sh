apt-get install xl2tpd
apt-get install strongswan
cp -f ~/dotfiles/etc/xl2tpd.conf /etc/xl2tpd/
cp -f ~/dotfiles/etc/chap-secrets /etc/ppp/
cp -f ~/dotfiles/etc/options.xl2tpd /etc/ppp/
cp -f ~/dotfiles/etc/ /etc/ppp/

ip = $(ip route get 8.8.8.8 | grep -v cache | cut -d ' ' -f 7)
echo '$ip : PSK \"lock123456\"' >> /etc/ipsec.secrets



