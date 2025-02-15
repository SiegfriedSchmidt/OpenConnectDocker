#!/bin/bash
set -e

# Usage: ./revoke-client-cert.sh username
if [ -z "$1" ]; then
  echo "Usage: $0 username"
  exit 1
fi

USERNAME="$1"
ROOT_DIR="/etc/ocserv/certs"
CA_DIR="$ROOT_DIR/server"
CLIENT_CERT="$ROOT_DIR/clients/$USERNAME/${USERNAME}-cert.pem"

if [ ! -f "$CLIENT_CERT" ]; then
  echo "Certificate for '$USERNAME' not found at $CLIENT_CERT"
  exit 1
fi

echo "Revoking certificate for '$USERNAME'..."

TMP_TEMPLATE=$(mktemp)
cat <<EOF > "$TMP_TEMPLATE"
crl_next_update = 365
crl_number = 1
EOF
certtool --generate-crl --load-ca-privkey "$CA_DIR/ca-key.pem" \
    --load-ca-certificate "$CA_DIR/ca-cert.pem" --load-certificate "$CLIENT_CERT" \
    --template "$TMP_TEMPLATE" --outfile "$CA_DIR/crl.pem"

rm -f "$TMP_TEMPLATE"

echo "Certificate for '$USERNAME' revoked; new CRL is at $CA_DIR/crl.pem."
