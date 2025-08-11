#!/bin/bash
# This script is called when a client disconnects.
# It logs additional statistics provided by ocserv.

TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

echo "----------------------------------------"
echo "Timestamp      : ${TIMESTAMP}"
echo "Event          : DISCONNECT"
echo "Username       : ${USERNAME}"
echo "Reason         : ${REASON}"
echo "Real IP        : ${IP_REAL}"
echo "VPN IP         : ${IP_REMOTE}"
echo "Session Duration (sec): ${STATS_DURATION}"
echo "Bytes In       : ${STATS_BYTES_IN}"
echo "Bytes Out      : ${STATS_BYTES_OUT}"
echo "----------------------------------------"

exit 0
