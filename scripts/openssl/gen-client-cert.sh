#!/bin/bash
set -e

# Usage: ./gen-client-cert.sh username [expiration_days]
if [ -z "$1" ]; then
  echo "Usage: $0 username [expiration_days]"
  exit 1
fi

USERNAME="$1"
EXPIRATION_DAYS=${2:-3650}
ROOT_DIR="/etc/ocserv/certs"
CA_DIR="$ROOT_DIR/server"
CLIENT_DIR="$ROOT_DIR/clients/$USERNAME"

mkdir -p "$CLIENT_DIR"

echo "Generating client certificate for '$USERNAME'..."
openssl genrsa -out "$CLIENT_DIR/${USERNAME}-key.pem" 2048
openssl req -new -key "$CLIENT_DIR/${USERNAME}-key.pem" \
  -subj "/CN=$USERNAME" -out "$CLIENT_DIR/${USERNAME}.csr"
openssl ca -config "$CA_DIR/openssl.cnf" -days "$EXPIRATION_DAYS" -in "$CLIENT_DIR/${USERNAME}.csr" \
  -out "$CLIENT_DIR/${USERNAME}-cert.pem" -batch -notext
rm -f "$CLIENT_DIR/${USERNAME}.csr"
# Create a PKCS#12 bundle (empty password; change -passout if needed)
openssl pkcs12 -export -out "$CLIENT_DIR/${USERNAME}.p12" \
  -inkey "$CLIENT_DIR/${USERNAME}-key.pem" \
  -in "$CLIENT_DIR/${USERNAME}-cert.pem" \
  -certfile "$CA_DIR/ca-cert.pem" -passout pass:123

echo "Client certificate for '$USERNAME' generated in $CLIENT_DIR."
