# Docker Compose
It is preferable to run the container in docker compose.
```yml
services:
  ocserv:
    platform: linux/amd64
    image: siegfriedschmidt/ocserv
    container_name: ocserv
    ports:
      - "8443:443"
    environment:
      # true value executes "network-manager.sh enable" to forward the vpn subnet to the external interface (default eth0)
      - FORWARD_TO_EXTERNAL_INTERFACE=true
    volumes:
      - ./ocserv.conf:/ocserv.conf:ro # configuration
      - ./data:/etc/ocserv #
    devices:
      - /dev/net/tun:/dev/net/tun
    cap_add:
      - NET_ADMIN
    sysctls:
      - net.ipv4.ip_forward=1
    security_opt:
      - no-new-privileges:true
    restart: unless-stopped
```

Run this code on your computer with running docker engine

```bash
mkdir ocserv
cd ocserv
touch docker-compose.yml # <- copy docker-compose.yml
touch ocserv.conf # <- copy ocserv.conf from repository
mkdir data
docker-compose up -d
```
The openconnect will be available on port 8443

# Authentification
This docker image supports two authentication methods. Certification authentication is enabled in ocserv.conf by default.

### Password
Create user with password (follow the prompts)
```bash
docker exec -it ocserv ocpasswd -c /etc/ocserv/ocpasswd username
```

### Certificate
Create user certificate (follow the prompts)
```bash
docker exec -it ocserv generate-client-cert.sh
```

# Clients
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

### Android
- [anyconnect](https://play.google.com/store/apps/details?id=com.cisco.anyconnect.vpn.android.avf&hl=en)
- [openconnect](https://play.google.com/store/apps/details?id=xyz.opcx.mph&pcampaignid=web_share)
- [oneconnect](https://play.google.com/store/apps/details?id=com.clavister.oneconnect&hl=en_US)

# Configuration 
[Ocserv documentation](https://ocserv.openconnect-vpn.net/ocserv.8.html)

# Links
[How vpn works](https://ocserv.openconnect-vpn.net/technical.html)

[Ocserv source code repository](https://gitlab.com/openconnect/ocserv)
