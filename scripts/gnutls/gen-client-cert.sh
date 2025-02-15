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
CLIENTS_DIR="$ROOT_DIR/clients/$USERNAME"
mkdir -p "$CLIENTS_DIR"

echo "Generating client certificate for '$USERNAME' using certtool..."
certtool --generate-privkey --outfile "$CLIENTS_DIR/${USERNAME}-key.pem"
TMP_TEMPLATE=$(mktemp)
cat <<EOF > "$TMP_TEMPLATE"
cn = "$USERNAME"
expiration_days = $EXPIRATION_DAYS
signing_key
encryption_key
tls_www_client
EOF
certtool --generate-certificate --load-privkey "$CLIENTS_DIR/${USERNAME}-key.pem" \
  --load-ca-certificate "$CA_DIR/ca-cert.pem" --load-ca-privkey "$CA_DIR/ca-key.pem" \
  --template "$TMP_TEMPLATE" --outfile "$CLIENTS_DIR/${USERNAME}-cert.pem"
rm -f "$TMP_TEMPLATE"
certtool --to-p12 --p12-password "" \
  --load-privkey "$CLIENTS_DIR/${USERNAME}-key.pem" \
  --load-certificate "$CLIENTS_DIR/${USERNAME}-cert.pem" \
  --outfile "$CLIENTS_DIR/${USERNAME}.p12"
echo "GnuTLS client certificate for '$USERNAME' generated in $CLIENTS_DIR."
