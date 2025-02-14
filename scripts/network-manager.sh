#!/bin/bash
set -e

# This script manages network NAT settings for VPN clients.
# Usage: network-manager.sh <command>
# Commands:
#    enable-internet : Adds an iptables rule for VPN_SUBNET to enable internet access.
#    disable-internet: Removes the iptables rule.
#    status          : Displays current NAT rules for VPN_SUBNET.

VPN_SUBNET="${VPN_SUBNET:-10.10.10.0/24}"
EXT_IFACE="${EXT_IFACE:-eth0}"  # Change if your external interface is named differently

command="$1"
if [ -z "$command" ]; then
    echo "Usage: $0 <enable-internet|disable-internet|status>"
    exit 1
fi

case "$command" in
    enable-internet)
        echo "Enabling internet access for VPN subnet $VPN_SUBNET on interface $EXT_IFACE..."
        # Add iptables NAT rule if it doesn't already exist.
        iptables -t nat -C POSTROUTING -s "$VPN_SUBNET" -o "$EXT_IFACE" -j MASQUERADE 2>/dev/null || \
        iptables -t nat -A POSTROUTING -s "$VPN_SUBNET" -o "$EXT_IFACE" -j MASQUERADE
        echo "Internet access enabled."
        ;;
    disable-internet)
        echo "Disabling internet access for VPN subnet $VPN_SUBNET..."
        iptables -t nat -D POSTROUTING -s "$VPN_SUBNET" -o "$EXT_IFACE" -j MASQUERADE || true
        echo "Internet access disabled."
        ;;
    status)
        echo "Current NAT rules for VPN subnet $VPN_SUBNET:"
        iptables -t nat -L POSTROUTING -n --line-numbers | grep "$VPN_SUBNET" || echo "No NAT rule found for VPN subnet."
        ;;
    *)
        echo "Unknown command: $command"
        echo "Usage: $0 <enable-internet|disable-internet|status>"
        exit 1
        ;;
esac
