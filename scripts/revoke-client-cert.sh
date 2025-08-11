#!/bin/bash
set -e

# Script to revoke client certificates
# Usage: ./revoke_client.sh <username> [--keep-files]

# Configuration
CERTS_DIR="/etc/ocserv/certs"
CLIENTS_DIR="${CERTS_DIR}/clients"
CRL_FILE="${CERTS_DIR}/crl.pem"
TEMPLATE_FILE="${CERTS_DIR}/crl.tmpl"
REVOKED_FILE="${CERTS_DIR}/revoked.pem"
CA_CERT="${CERTS_DIR}/ca-cert.pem"
CA_KEY="${CERTS_DIR}/ca-key.pem"

# Check arguments
if [ $# -lt 1 ]; then
    echo "Error: Please provide client username"
    echo "Usage: $0 <username> [--keep-files]"
    exit 1
fi

USERNAME="$1"
KEEP_FILES=false
CLIENT_PATH="${CLIENTS_DIR}/${USERNAME}/${USERNAME}-cert.pem"

# Check if --keep-files option is set
if [ "$2" = "--keep-files" ]; then
    KEEP_FILES=true
fi

# Validate client path
if [ ! -f "${CLIENT_PATH}" ]; then
    echo "Error: Client certificate not found at ${CLIENT_PATH}"
    exit 1
fi

# Check if CA files exist
if [ ! -f "${CA_CERT}" ] || [ ! -f "${CA_KEY}" ]; then
    echo "Error: CA certificate or key not found in ${CERTS_DIR}"
    exit 1
fi

# Check if CRL exists, create if not
if [ ! -f "${CRL_FILE}" ]; then
    echo "CRL not found, generate initial empty CRL first!"
    exit 1
fi

# Revoke certificate
echo "Revoking client certificate: ${CLIENT_PATH}"

# Add certificate to revoked.pem
cat "${CLIENT_PATH}" >> "${REVOKED_FILE}"

# Create CRL template
cat << _EOF_ > "${TEMPLATE_FILE}"
crl_next_update = 365
crl_number = 1
_EOF_

# Generate updated CRL
certtool --generate-crl \
    --load-ca-privkey "${CA_KEY}" \
    --load-ca-certificate "${CA_CERT}" \
    --load-certificate "${REVOKED_FILE}" \
    --template "${TEMPLATE_FILE}" \
    --outfile "${CRL_FILE}"

# Cleanup
rm -f "${TEMPLATE_FILE}"
rm -f "${REVOKED_FILE}"

# Determine client directory
CLIENT_DIR=$(dirname "${CLIENT_PATH}")

# Delete client files unless --keep-files was specified
if [ "$KEEP_FILES" = false ]; then
    echo "Removing client certificate files..."
    rm -r "${CLIENT_DIR}"
else
    echo "Keeping client files as requested (--keep-files)"
fi

echo -e "\nCertificate for ${USERNAME} revoked successfully!"
echo "Updated CRL: ${CRL_FILE}"
