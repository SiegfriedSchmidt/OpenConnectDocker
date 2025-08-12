#!/bin/bash
set -e

echo "Run generate-ca.sh script..."
/usr/local/bin/generate-ca.sh 3650 
echo "Run generate-server-cert.sh script..."
/usr/local/bin/generate-server-cert.sh 825 "Web Tech" "example.com,www.example.com" 
echo "Run generate-crl.sh script..."
/usr/local/bin/generate-crl.sh

if [[ "${FORWARD_TO_EXTERNAL_INTERFACE}" =~ ^(yes|true)$ ]]; then
    /usr/local/bin/network-manager.sh enable
fi
/usr/local/bin/network-manager.sh status

exec "$@"