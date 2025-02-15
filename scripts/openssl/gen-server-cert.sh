#!/bin/bash
set -e

# Usage: ./gen-server-cert.sh [expiration_days]
EXPIRATION_DAYS=${1:-3650}
ROOT_DIR="/etc/ocserv/certs"
CA_DIR="$ROOT_DIR/server"
SSL_DIR="$ROOT_DIR/ssl"

mkdir -p "$CA_DIR" "$SSL_DIR" "$CA_DIR/certs" "$CA_DIR/newcerts"

# Create minimal CA structure if not exists
[ -f "$CA_DIR/index.txt" ] || touch "$CA_DIR/index.txt"
[ -f "$CA_DIR/serial" ] || echo 1000 > "$CA_DIR/serial"
[ -f "$CA_DIR/crlnumber" ] || echo 1000 > "$CA_DIR/crlnumber"

# Create a minimal openssl.cnf if not exists
if [ ! -f "$CA_DIR/openssl.cnf" ]; then
  cat <<EOF > "$CA_DIR/openssl.cnf"
[ ca ]
default_ca = CA_default

[ CA_default ]
dir               = $CA_DIR
certs             = \$dir/certs
new_certs_dir     = \$dir/newcerts
database          = \$dir/index.txt
serial            = \$dir/serial
crlnumber         = \$dir/crlnumber
crl               = \$dir/crl.pem
crl_extensions    = crl_ext
default_crl_days  = 30
private_key       = \$dir/ca-key.pem
certificate       = \$dir/ca-cert.pem
default_md        = sha256
name_opt          = ca_default
cert_opt          = ca_default
default_days      = $EXPIRATION_DAYS
preserve          = no
policy            = policy_any

[ policy_any ]
countryName             = optional
stateOrProvinceName     = optional
localityName            = optional
organizationName        = optional
organizationalUnitName  = optional
commonName              = supplied
emailAddress            = optional

[ req ]
default_bits        = 2048
default_keyfile     = privkey.pem
distinguished_name  = req_distinguished_name
string_mask         = utf8only

[ req_distinguished_name ]
commonName = Common Name

[ crl_ext ]
authorityKeyIdentifier=keyid:always
EOF
fi

# Generate CA certificate and key if not exists
if [ ! -f "$CA_DIR/ca-cert.pem" ] || [ ! -f "$CA_DIR/ca-key.pem" ]; then
  echo "Generating CA certificate and key..."
  openssl genrsa -out "$CA_DIR/ca-key.pem" 4096
  openssl req -new -x509 -days "$EXPIRATION_DAYS" -key "$CA_DIR/ca-key.pem" \
    -subj "/CN=VPN_CA" -out "$CA_DIR/ca-cert.pem"
  echo "CA generated."
else
  echo "CA certificate and key already exist."
fi

# Generate CRL (always update)
echo "Generating CRL..."
openssl ca -gencrl -config "$CA_DIR/openssl.cnf" -out "$CA_DIR/crl.pem" -batch

# Generate server certificate and key if auto-generation is enabled
if [ "$DISABLE_AUTO_SIGNED_CERTS" != "true" ]; then
  if [ ! -f "$SSL_DIR/server-cert.pem" ] || [ ! -f "$SSL_DIR/server-key.pem" ]; then
    echo "Generating server certificate..."
    openssl genrsa -out "$SSL_DIR/server-key.pem" 2048
    openssl req -new -key "$SSL_DIR/server-key.pem" -subj "/CN=$(hostname)" \
      -out "$SSL_DIR/server.csr"
    # Sign using our CA:
    openssl ca -config "$CA_DIR/openssl.cnf" -days "$EXPIRATION_DAYS" -in "$SSL_DIR/server.csr" \
      -out "$SSL_DIR/server-cert.pem" -batch -notext
    rm -f "$SSL_DIR/server.csr"
    echo "Server certificate generated."
  else
    echo "Server certificate already exists."
  fi
else
  echo "Auto-generation of server certificate disabled (using external cert, e.g. Let's Encrypt)."
fi

echo "gen-server-cert.sh completed."
echo "CA and CRL are in $CA_DIR, server cert in $SSL_DIR."
