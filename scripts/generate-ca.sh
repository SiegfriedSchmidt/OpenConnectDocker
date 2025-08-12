#!/bin/bash
set -e

# Script to generate a CA certificate using certtool (gnutls)
# Usage: ./generate_ca.sh <expiration_date_in_days>

# Configuration variables
CERTS_DIR="/etc/ocserv/certs"     # Folder where certificates will be saved
CA_KEY="${CERTS_DIR}/ca-key.pem"              # CA private key path
CA_CERT="${CERTS_DIR}/ca-cert.pem"            # CA certificate path

# Check if expiration days argument is provided
if [ -z "$1" ]; then
    echo "Error: Please provide the expiration date in days as an argument."
    echo "Usage: $0 <expiration_date_in_days>"
    exit 1
fi

# Check if CA files already exist
if [ -f "${CA_KEY}" ] && [ -f "${CA_CERT}" ]; then
    echo "CA certificate and key already exist in ${CERTS_DIR}"
    echo "Skipping generation..."
    echo -e "\nExisting certificate information:"
    certtool --certificate-info --infile "${CA_CERT}"
    exit 0
fi

EXPIRATION_DAYS="$1"

# Create output directory if it doesn't exist
mkdir -p "${CERTS_DIR}"

# Generate private key
certtool --generate-privkey --outfile "${CA_KEY}"

# Create temporary template file
TEMPLATE_FILE="${CERTS_DIR}/ca.tmpl"

echo "Creating temporary CA template..."
cat << _EOF_ > "${TEMPLATE_FILE}"
cn = "Web Digital CA"
organization = "Routing Services"
serial = 1
expiration_days = ${EXPIRATION_DAYS}
ca
signing_key
cert_signing_key
crl_signing_key
_EOF_

# Generate self-signed CA certificate
certtool --generate-self-signed \
    --load-privkey "${CA_KEY}" \
    --template "${TEMPLATE_FILE}" \
    --outfile "${CA_CERT}"

# Display certificate information
# echo -e "\nCertificate information:"
# certtool --certificate-info --infile "${CA_CERT}"

# Clean up template file
rm -f "${TEMPLATE_FILE}"
echo -e "\nTemporary template file removed."

echo -e "\nCA certificate generation complete!"
echo "Files created in: ${CERTS_DIR}"
echo " - Private key: $(basename "${CA_KEY}")"
echo " - CA certificate: $(basename "${CA_CERT}")"
