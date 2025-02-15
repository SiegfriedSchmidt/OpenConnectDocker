#!/bin/bash
set -e

# Usage: ./gen-server-cert.sh [expiration_days]
EXPIRATION_DAYS=${1:-3650}
ROOT_DIR="/etc/ocserv/certs"
SERVER_DIR="$ROOT_DIR/server"
SSL_DIR="$ROOT_DIR/ssl"

mkdir -p "$SERVER_DIR" "$SSL_DIR"

# Generate CA certificate using certtool if not exists
if [ ! -f "$SERVER_DIR/ca-cert.pem" ] || [ ! -f "$SERVER_DIR/ca-key.pem" ]; then
  echo "Generating CA certificate with certtool..."
  certtool --generate-privkey --outfile "$SERVER_DIR/ca-key.pem"
  cat <<EOF > "$SERVER_DIR/ca.tmpl"
cn = "SERVER_CA"
organization = "Big Corp"
expiration_days = -1
ca
signing_key
cert_signing_key
crl_signing_key
EOF
  certtool --generate-self-signed --load-privkey "$SERVER_DIR/ca-key.pem" \
    --template "$SERVER_DIR/ca.tmpl" --outfile "$SERVER_DIR/ca-cert.pem"
  rm -f "$SERVER_DIR/ca.tmpl"
  echo "CA generated."
else
  echo "CA already exists."
fi

# Generate CRL
echo "Generating empty revocation list..."
TMP_TEMPLATE=$(mktemp)
cat <<EOF > "$TMP_TEMPLATE"
crl_next_update = 365
crl_number = 1
EOF
certtool --generate-crl --load-ca-privkey "$SERVER_DIR/ca-key.pem" \
  --load-ca-certificate "$SERVER_DIR/ca-cert.pem" \
  --template "$TMP_TEMPLATE" --outfile "$CA_DIR/crl.pem"
rm -f "$TMP_TEMPLATE"

# Generate server certificate if auto-generation is enabled
if [ "$DISABLE_AUTO_SIGNED_CERTS" != "true" ]; then
  if [ ! -f "$SSL_DIR/server-cert.pem" ] || [ ! -f "$SSL_DIR/server-key.pem" ]; then
    echo "Generating server certificate with certtool..."
    certtool --generate-privkey --outfile "$SSL_DIR/server-key.pem"
    cat <<EOF > "$SSL_DIR/server.tmpl"
cn = "Web server"
dns_name = "web.example.com"
dns_name = "panel.example.com"
#ip_address = "1.2.3.4"
organization = "MyCompany"
expiration_days = $EXPIRATION_DAYS
signing_key
encryption_key #only if the generated key is an RSA one
tls_www_server
EOF
    certtool --generate-certificate --load-privkey "$SSL_DIR/server-key.pem" \
      --load-ca-certificate "$SERVER_DIR/ca-cert.pem" --load-ca-privkey "$SERVER_DIR/ca-key.pem" \
      --template "$SSL_DIR/server.tmpl" --outfile "$SSL_DIR/server-cert.pem"
    rm -f "$SSL_DIR/server.tmpl"
    echo "Server certificate generated."
  else
    echo "Server certificate already exists."
  fi
else
  echo "Auto-generation disabled."
fi

echo "GnuTLS gen-server-cert.sh completed."
