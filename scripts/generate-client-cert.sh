#!/bin/bash
set -e

# Interactive script to generate client certificates with UID authentication
# Organizes certificates in individual client directories

# Configuration
CERTS_DIR="/etc/ocserv/certs"
CA_CERT="${CERTS_DIR}/ca-cert.pem"
CA_KEY="${CERTS_DIR}/ca-key.pem"
CLIENTS_DIR="${CERTS_DIR}/clients"

# Check if CA exists
if [ ! -f "${CA_CERT}" ] || [ ! -f "${CA_KEY}" ]; then
    echo "Error: CA certificates not found in ${CERTS_DIR}"
    echo "Please generate CA certificates first"
    exit 1
fi

# Create clients directory if it doesn't exist
mkdir -p "${CLIENTS_DIR}"

# Interactive prompts
echo -e "\n=== Client Certificate Generation ==="
echo "This will create a client certificate signed by your CA."

# Collect client information
echo -e "\n[1/4] Client details:"
read -p "Enter username (UID): " USERNAME
read -p "Enter full name (for CN): " FULL_NAME
read -p "Enter organization (optional): " ORGANIZATION

# Validate username
if [[ ! "${USERNAME}" =~ ^[a-zA-Z0-9_-]+$ ]]; then
    echo "Error: Username can only contain letters, numbers, underscores and hyphens"
    exit 1
fi

# Check if client directory already exists
CLIENT_DIR="${CLIENTS_DIR}/${USERNAME}"
if [ -d "${CLIENT_DIR}" ]; then
    echo -e "\nWarning: Directory for ${USERNAME} already exists!"
    read -p "Overwrite existing certificate? (y/n): " OVERWRITE
    if [ "${OVERWRITE}" != "y" ]; then
        echo "Aborted."
        exit 0
    fi
fi

# Create client directory
mkdir -p "${CLIENT_DIR}"

# Expiration
echo -e "\n[2/4] Certificate validity:"
read -p "Enter expiration in days (e.g., 365): " EXPIRATION_DAYS

# Certificate files
CLIENT_KEY="${CLIENT_DIR}/${USERNAME}-key.pem"
CLIENT_CERT="${CLIENT_DIR}/${USERNAME}-cert.pem"
CLIENT_P12="${CLIENT_DIR}/${USERNAME}.p12"
TEMPLATE_FILE="${CLIENT_DIR}/${USERNAME}.tmpl"

# Build DN
DN="cn=${FULL_NAME},UID=${USERNAME}"
[ -n "${ORGANIZATION}" ] && DN="${DN},O=${ORGANIZATION}"

# Generate private key
echo -e -n "\n[3/4] "
certtool --generate-privkey --outfile "${CLIENT_KEY}"

# Create template
echo -e "\nCreating certificate template..."
{
    echo "dn = \"${DN}\""
    echo "expiration_days = ${EXPIRATION_DAYS}"
    echo "signing_key"
    echo "tls_www_client"
} > "${TEMPLATE_FILE}"

# Generate certificate
certtool --generate-certificate \
    --load-privkey "${CLIENT_KEY}" \
    --load-ca-certificate "${CA_CERT}" \
    --load-ca-privkey "${CA_KEY}" \
    --template "${TEMPLATE_FILE}" \
    --outfile "${CLIENT_CERT}"

# Create PKCS#12 file
echo -e "\n[4/4] Creating PKCS#12 bundle..."
read -p "Set password for PKCS#12 file: " -s P12_PASS
echo ""
certtool --to-p12 \
    --load-privkey "${CLIENT_KEY}" \
    --load-certificate "${CLIENT_CERT}" \
    --pkcs-cipher 3des-pkcs12 \
    --password "${P12_PASS}" \
    --outfile "${CLIENT_P12}" \
    --outder

# Cleanup
rm -f "${TEMPLATE_FILE}"

# Set permissions
# chmod 600 "${CLIENT_KEY}" "${CLIENT_CERT}" "${CLIENT_P12}"
# chmod 700 "${CLIENT_DIR}"

# Display results
echo -e "\n=== Client Certificate Created ==="
echo "Certificate files for ${USERNAME}:"
echo -e "Location: \t${CLIENT_DIR}/"
echo -e "Private key: \t$(basename "${CLIENT_KEY}")"
echo -e "Certificate: \t$(basename "${CLIENT_CERT}")"
echo -e "PKCS#12: \t$(basename "${CLIENT_P12}")"

# echo -e "\n=== Server Configuration ==="
# echo "Add to ocserv.conf:"
# echo "cert-user-oid = 0.9.2342.19200300.100.1.1"

echo -e "\n=== Client Installation ==="
echo "1. Copy ${USERNAME}.p12 to the client device"
echo "2. Import it using the password you set"
echo "3. The PKCS#12 file contains both certificate and private key"
