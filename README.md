### Docker
Running docker engine is required

```bash
mkdir ocserv
cd ocserv
touch docker-compose.yml # <- copy docker-compose.yml
mkdir config
cd config
touch ocserv.conf # <- copy ocserv.conf
docker-compose up -d
```

### Create user
#### Password
```bash
docker exec -it ocserv ocpasswd -c /etc/ocserv/ocpasswd username
```

#### Certificate
```bash
docker exec -it ocserv gen-client-cert.sh username
```

### Network-manager
Usage: network-manager.sh <command>

Commands:

    enable-internet : Adds an iptables rule for VPN_SUBNET to enable internet access.

    disable-internet: Removes the iptables rule.
	
    status          : Displays current NAT rules for VPN_SUBNET.

```bash
docker exec -it ocserv network-manager.sh enable-internet
```
