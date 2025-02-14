#!/bin/bash
set -e

CERTS_DIR="/etc/ocserv/certs"
CERT_TOOL="${CERT_TOOL:-gnutls}"

if [ "$CERT_TOOL" == "openssl" ]; then
    echo "Generating certificates using OpenSSL..."
    # Generate CA key and certificate
    openssl genrsa -out "${CERTS_DIR}/ca-key.pem" 2048
    openssl req -x509 -new -nodes -key "${CERTS_DIR}/ca-key.pem" -sha256 -days 365 \
         -subj "/CN=VPN CA/O=Big Corp" \
         -out "${CERTS_DIR}/ca-cert.pem"
    
    # Generate server key and CSR
    openssl genrsa -out "${CERTS_DIR}/server-key.pem" 2048
    openssl req -new -key "${CERTS_DIR}/server-key.pem" \
         -subj "/CN=VPN server/O=MyCompany" \
         -out "${CERTS_DIR}/server.csr"
    
    # Sign server certificate with CA
    openssl x509 -req -in "${CERTS_DIR}/server.csr" -CA "${CERTS_DIR}/ca-cert.pem" \
         -CAkey "${CERTS_DIR}/ca-key.pem" -CAcreateserial -out "${CERTS_DIR}/server-cert.pem" \
         -days 365 -sha256

    rm -f "${CERTS_DIR}/server.csr" "${CERTS_DIR}/ca.srl"
    
elif [ "$CERT_TOOL" == "gnutls" ]; then
    echo "Generating certificates using GnuTLS certtool..."
    # Generate CA key and self-signed certificate
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
    
    # Generate server key and certificate
    certtool --generate-privkey --outfile "${CERTS_DIR}/server-key.pem"
    cat << _EOF_ > "${CERTS_DIR}/server.tmpl"
cn = "VPN server"
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
else
    echo "Invalid CERT_TOOL specified. Use 'openssl' or 'gnutls'."
    exit 1
fi

echo "Certificate generation complete."
