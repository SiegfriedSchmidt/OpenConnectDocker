#!/bin/bash
set -e

# Script to generate empty CRL (Certificate Revocation List)
# Usage: ./generate_crl.sh [--force]

# Configuration
CERTS_DIR="/etc/ocserv/certs"
CRL_FILE="${CERTS_DIR}/crl.pem"
TEMPLATE_FILE="${CERTS_DIR}/crl.tmpl"
CA_CERT="${CERTS_DIR}/ca-cert.pem"
CA_KEY="${CERTS_DIR}/ca-key.pem"

# Check if CA files exist
if [ ! -f "${CA_CERT}" ] || [ ! -f "${CA_KEY}" ]; then
    echo "Error: CA certificate or key not found in ${CERTS_DIR}"
    echo "Please generate CA certificates first"
    exit 1
fi

# Check if CRL already exists
if [ -f "${CRL_FILE}" ]; then
    if [ "$1" = "--force" ]; then
        echo "CRL file exists, forcing regeneration..."
    else
        echo "CRL file already exists: ${CRL_FILE}"
        echo "Use '--force' argument to regenerate"
        exit 0
    fi
fi

# Create template
echo "Generating CRL template..."
cat << _EOF_ > "${TEMPLATE_FILE}"
crl_next_update = 365
crl_number = 1
_EOF_

# Generate CRL
certtool --generate-crl \
    --load-ca-privkey "${CA_KEY}" \
    --load-ca-certificate "${CA_CERT}" \
    --template "${TEMPLATE_FILE}" \
    --outfile "${CRL_FILE}"

# Cleanup
rm -f "${TEMPLATE_FILE}"

echo "Empty CRL generated: ${CRL_FILE}"
