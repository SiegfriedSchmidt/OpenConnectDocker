#!/bin/bash
set -e

# Script to generate a server certificate signed by the CA
# Usage: ./generate_server_cert.sh <expiration_date_in_days> <server_common_name> [dns_name1,dns_name2,...]

# Configuration variables
CERTS_DIR="/etc/ocserv/certs"
SERVER_KEY="${CERTS_DIR}/server-key.pem"
SERVER_CERT="${CERTS_DIR}/server-cert.pem"
CA_CERT="${CERTS_DIR}/ca-cert.pem"
CA_KEY="${CERTS_DIR}/ca-key.pem"

# Check arguments
if [ $# -lt 2 ]; then
    echo "Error: Please provide expiration days and server common name"
    echo "Usage: $0 <expiration_days> <server_cn> [dns_name1,dns_name2,...]"
    exit 1
fi

EXPIRATION_DAYS="$1"
SERVER_CN="$2"
DNS_NAMES="${3:-}"

# Check if CA files exist
if [ ! -f "${CA_CERT}" ] || [ ! -f "${CA_KEY}" ]; then
    echo "Error: CA certificate or key not found in ${CERTS_DIR}"
    echo "Please generate CA certificates first"
    exit 1
fi

# Check if server cert already exists
if [ -f "${SERVER_KEY}" ] && [ -f "${SERVER_CERT}" ]; then
    echo "Server certificate and key already exist in ${CERTS_DIR}"
    echo "Skipping generation..."
    echo -e "\nExisting server certificate information:"
    certtool --certificate-info --infile "${SERVER_CERT}"
    exit 0
fi

# Generate server private key
certtool --generate-privkey --outfile "${SERVER_KEY}"

# Create temporary template file
TEMPLATE_FILE="${CERTS_DIR}/server.tmpl"
echo "Creating server certificate template..."

# Base template
cat << _EOF_ > "${TEMPLATE_FILE}"
cn = "${SERVER_CN}"
organization = "MyCompany"
expiration_days = ${EXPIRATION_DAYS}
signing_key
encryption_key
tls_www_server
_EOF_

# Add DNS names if provided
if [ -n "${DNS_NAMES}" ]; then
    IFS=',' read -ra DNS_ARRAY <<< "${DNS_NAMES}"
    for dns in "${DNS_ARRAY[@]}"; do
        echo "dns_name = \"${dns}\"" >> "${TEMPLATE_FILE}"
    done
fi

# Generate server certificate
certtool --generate-certificate \
    --load-privkey "${SERVER_KEY}" \
    --load-ca-certificate "${CA_CERT}" \
    --load-ca-privkey "${CA_KEY}" \
    --template "${TEMPLATE_FILE}" \
    --outfile "${SERVER_CERT}"

# Display certificate information
# echo -e "\nServer certificate information:"
# certtool --certificate-info --infile "${SERVER_CERT}"

# Clean up template file
rm -f "${TEMPLATE_FILE}"
echo -e "\nTemporary template file removed."

echo -e "\nServer certificate generation complete!"
echo "Files created in: ${CERTS_DIR}"
echo " - Private key: $(basename "${SERVER_KEY}")"
echo " - Certificate: $(basename "${SERVER_CERT}")"
