dhclient -v

ip tuntap add tun0 mode tun
tunctl -n -t tun0 -u root
ip link set dev tun0 up
ifconfig tun0 192.168.194.224 netmask 255.255.255.0 promisc
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE

