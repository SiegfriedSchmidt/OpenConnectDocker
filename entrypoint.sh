#!/bin/bash

CERTS_DIR="/etc/ocserv/certs"
mkdir -p "${CERTS_DIR}"

# Check if certificates exist
if [ ! -f "${CERTS_DIR}/server-cert.pem" ] || [ ! -f "${CERTS_DIR}/server-key.pem" ]; then
    echo "Generating self-signed certificates in ${CERTS_DIR}..."
    
    # Generate CA
    certtool --generate-privkey --outfile "${CERTS_DIR}/ca-key.pem"
    cat << _EOF_ > "${CERTS_DIR}/ca.tmpl"
cn = "VPN CA"
organization = "Big Corp"
serial = 1
expiration_days = -1
ca
signing_key
cert_signing_key
crl_signing_key
_EOF_
    certtool --generate-self-signed \
        --load-privkey "${CERTS_DIR}/ca-key.pem" \
        --template "${CERTS_DIR}/ca.tmpl" \
        --outfile "${CERTS_DIR}/ca-cert.pem"

    # Generate Server Certificate
    certtool --generate-privkey --outfile "${CERTS_DIR}/server-key.pem"
    cat << _EOF_ > "${CERTS_DIR}/server.tmpl"
cn = "VPN server"
dns_name = "www.example.com"
dns_name = "vpn.example.com"
organization = "MyCompany"
expiration_days = -1
signing_key
encryption_key
tls_www_server
_EOF_
    certtool --generate-certificate \
        --load-privkey "${CERTS_DIR}/server-key.pem" \
        --load-ca-certificate "${CERTS_DIR}/ca-cert.pem" \
        --load-ca-privkey "${CERTS_DIR}/ca-key.pem" \
        --template "${CERTS_DIR}/server.tmpl" \
        --outfile "${CERTS_DIR}/server-cert.pem"

    echo "Certificate generation complete in ${CERTS_DIR}"
fi

exec "$@"