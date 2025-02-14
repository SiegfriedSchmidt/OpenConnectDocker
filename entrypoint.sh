#!/bin/bash
set -e

CERTS_DIR="/etc/ocserv/certs"
mkdir -p "${CERTS_DIR}"

if [ "${DISABLE_AUTO_SIGNED_CERTS}" != "true" ]; then
  # Only generate certificates if DISABLE_AUTO_SIGNED_CERTS is not set to true.
  if [ ! -f "${CERTS_DIR}/server-cert.pem" ] || [ ! -f "${CERTS_DIR}/server-key.pem" ]; then
      echo "Certificates not found. Generating certificates using ${CERT_TOOL:-gnutls}..."
      /usr/local/bin/gen-certs.sh
  else
      echo "Certificates already exist in ${CERTS_DIR}."
  fi
else
  echo "Certificate creation disabled by DISABLE_AUTO_SIGNED_CERTS environment variable."
fi

if [[ "${ENABLE_INTERNET}" =~ ^(yes|true)$ ]]; then
    echo "Enabling internet access for VPN subnet ${VPN_SUBNET:-10.10.10.0/24}..."
    /usr/local/bin/network-manager.sh enable-internet
else
    echo "Internet access is not enabled."
fi

exec "$@"
