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
openssl ca -config "$CA_DIR/openssl.cnf" -revoke "$CLIENT_CERT" -batch
echo "Generating updated CRL..."
openssl ca -config "$CA_DIR/openssl.cnf" -gencrl -out "$CA_DIR/crl.pem" -batch
echo "Certificate for '$USERNAME' revoked; new CRL is at $CA_DIR/crl.pem."
