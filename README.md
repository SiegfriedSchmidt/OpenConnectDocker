# OpenConnect VPN Server (ocserv) in Docker

[![Docker Pulls](https://img.shields.io/docker/pulls/siegfriedschmidt/ocserv)](https://hub.docker.com/r/siegfriedschmidt/ocserv)

A secure, enterprise-grade SSL/TLS VPN server based on OpenConnect (ocserv) running in Docker. This solution provides a modern alternative to IPsec VPNs with excellent security features and cross-platform compatibility.

## SSL/TLS VPN Overview
SSL/TLS VPNs like OpenConnect offer significant advantages over traditional VPNs:
- **Strong Encryption**: Uses TLS 1.2/1.3 with modern cipher suites
- **Firewall Friendly**: Operates over standard HTTPS (TCP/443)
- **NAT Traversal**: Built-in support for NAT environments
- **Web Integration**: Can camouflage as a regular web server
- **Fine-grained Control**: Per-user/group policies and bandwidth management

OpenConnect implements the Cisco AnyConnect protocol, providing compatibility with a wide range of clients while being open source and freely available.

## Features
- 🔒 Certificate and password authentication
- 📱 Cross-platform client support (Windows, macOS, Linux, iOS, Android)
- 🌐 IPv4 and IPv6 support
- ⚡️ DTLS for reduced latency
- 📊 Bandwidth management and QoS
- 🛡️ Built-in security headers and TLS hardening
- 🔄 Automatic certificate management scripts

## Quick Start

```bash
mkdir ocserv && cd ocserv
curl -O https://raw.githubusercontent.com/siegfriedschmidt/openconnectdocker/master/docker-compose.yml
curl -O https://raw.githubusercontent.com/siegfriedschmidt/openconnectdocker/master/ocserv.conf
mkdir data

docker compose up -d
```

The openconnect server will be available on port 443 (but you can change it anytime in docker-compose.yml)

## Synology NAS Setup

Synology DSM doesn't enable TUN/TAP devices by default. Create `/dev/net/tun` with this script:

```bash
#!/bin/sh

# Create the necessary file structure for /dev/net/tun
if ( [ ! -c /dev/net/tun ] ); then
  if ( [ ! -d /dev/net ] ); then
    mkdir -m 600 /dev/net
  fi
  mknod /dev/net/tun c 10 200
  chmod 600 /dev/net/tun
fi

# Load the tun module if not already loaded
if ( !(lsmod | grep -q "^tun\s") ); then
  insmod /lib/modules/tun.ko
fi
```
1. Save as `/usr/local/bin/enable-tun.sh`
2. Make executable: `chmod +x /usr/local/bin/enable-tun.sh`
3. Add to startup via Task Scheduler (Trigger: Boot-up)

## Authentification

This docker image allows two methods of authentification. By default in ocserv.conf certficate authentification enabled

### Password

Create user with password (follow the prompts)
```bash
docker exec -it ocserv ocpasswd -c /etc/ocserv/ocpasswd username
```

Remove user
```bash
docker exec -it ocserv ocpasswd -c /etc/ocserv/ocpasswd -d username
```

### Certificate

Generate user certificate (follow the prompts)
```bash
docker exec -it ocserv generate-client-cert.sh
```

Revoke user certificate
```bash
docker exec -it ocserv revoke-client-cert.sh username
```

## Clients

### Windows

- [openconnect-GUI](https://gui.openconnect-vpn.net/download/)
- [anyconnect](https://apps.microsoft.com/detail/9wzdncrdj8lh?hl=en-US&gl=US)
- [oneconnect](https://apps.microsoft.com/detail/clavister-oneconnect/9P2L1BWS7BB6?hl=en-US&gl=US)

### Linux

[openconnect installation](https://www.infradead.org/openconnect/packages.html)
```bash
sudo apt install openconnect
```

### MacOS / IOS 

- [anyconnect](https://apps.apple.com/us/app/cisco-secure-client/id1135064690)
- [oneconnect](https://apps.apple.com/us/app/clavister-oneconnect/id1565970099)
> [!TIP]
> To install the certificate on iOS, you should click on the certificate and then share it with the anyconnect app.
> 
> For macOS, you should create a small web server (for example, http.server in python) and import the certificate using the URL

### Android

- [anyconnect](https://play.google.com/store/apps/details?id=com.cisco.anyconnect.vpn.android.avf&hl=en)
- [openconnect](https://play.google.com/store/apps/details?id=xyz.opcx.mph&pcampaignid=web_share)
- [oneconnect](https://play.google.com/store/apps/details?id=com.clavister.oneconnect&hl=en_US)

## Configuration 

[Ocserv documentation](https://ocserv.openconnect-vpn.net/ocserv.8.html)

## Links

[How vpn works](https://ocserv.openconnect-vpn.net/technical.html)

[Ocserv source code repository](https://gitlab.com/openconnect/ocserv)

## Support

**If this project is useful to you, you may wish to give it a**:star2:

- BTC (Bitcoin): `bc1qkrpftuqy3suh9j3e348s06hdtxhk3tg8382nwj`
- ETH (Ethereum): `0xDBbF67c7f998837E9Ae2175327Ba31197991c031`
- TON (TON): `UQArWditRe48dGghp1bVDOnIprFZcVKzElQqQb_q68D5Im4E`
