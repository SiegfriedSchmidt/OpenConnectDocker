#!/bin/bash
# This script is called when a client connects.
# It uses environment variables provided by ocserv.

TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

# Log the connection event

echo "----------------------------------------"
echo "Timestamp      : ${TIMESTAMP}"
echo "Event          : CONNECT"
echo "Reason         : ${REASON}"
echo "Username       : ${USERNAME}"
echo "Groupname      : ${GROUPNAME}"
echo "Real IP        : ${IP_REAL}"
echo "Remote Host    : ${REMOTE_HOSTNAME}"
echo "Local Interface IP (client side): ${IP_REAL_LOCAL}"
echo "VPN IP         : ${IP_REMOTE}"
echo "Routes         : ${OCSERV_ROUTES}"
echo "DNS Servers    : ${OCSERV_DNS}"
echo "Device         : ${DEVICE}"
echo "ID             : ${ID}"
echo "----------------------------------------"

exit 0
