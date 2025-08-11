#!/bin/bash
set -e

echo "Run gen-server-cert.sh script..."
/usr/local/bin/gen-server-cert.sh

# if [[ "${ENABLE_INTERNET}" =~ ^(yes|true)$ ]]; then
#     /usr/local/bin/network-manager.sh enable-internet
# else
#     echo "Internet access is not enabled."
# fi

exec "$@"