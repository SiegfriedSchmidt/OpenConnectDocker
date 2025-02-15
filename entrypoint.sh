#!/bin/bash
set -e

echo "Run gen-server-cert.sh script..."
/usr/local/bin/gen-server-cert.sh

if [[ "${ENABLE_INTERNET}" =~ ^(yes|true)$ ]]; then
    echo "Enabling internet access for VPN subnet ${VPN_SUBNET:-10.10.10.0/24}..."
    /usr/local/bin/network-manager.sh enable-internet
else
    echo "Internet access is not enabled."
fi

exec "$@"
