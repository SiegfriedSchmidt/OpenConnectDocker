#!/bin/bash

# Check if certificates exist in the mounted directory
if [ ! -f "/etc/ocserv/server-cert.pem" ] || [ ! -f "/etc/ocserv/server-key.pem" ]; then
    echo "Generating self-signed certificates..."
    certtool --generate-privkey --outfile /etc/ocserv/server-key.pem
    certtool --generate-self-signed --load-privkey /etc/ocserv/server-key.pem \
             --template /usr/share/doc/ocserv/cert-templates/server.template \
             --outfile /etc/ocserv/server-cert.pem
fi

exec "$@"