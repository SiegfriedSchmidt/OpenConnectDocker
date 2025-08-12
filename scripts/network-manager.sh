#!/bin/bash
set -e

# VPN NAT Manager

CONFIG="/ocserv.conf"
EXT_IFACE=${EXT_IFACE:-eth0}  # Default to eth0 if not specified

validate_root() {
  [ "$(id -u)" -eq 0 ] || { echo "Run as root" >&2; exit 1; }
}

netmask_to_cidr() {
  c=0 x=0$( printf '%o' ${1//./ } )
  while [ $x -gt 0 ]; do
      let c+=$((x%2)) 'x>>=1'
  done
  echo $c
}

get_subnet() {
  [ -f "$CONFIG" ] || { echo "Config file missing: $CONFIG" >&2; return 1; }

  local network=$(grep -E '^ipv4-network\s*=' "$CONFIG" | awk -F'=' '{print $2}' | sed 's/[[:space:]]*//;s/#.*//')
  local netmask=$(grep -E '^ipv4-netmask\s*=' "$CONFIG" | awk -F'=' '{print $2}' | sed 's/[[:space:]]*//;s/#.*//')

  [ -z "$network" ] && { echo "ipv4-network not found in config" >&2; return 1; }
  [ -z "$netmask" ] && { echo "ipv4-netmask not found in config" >&2; return 1; }

  local cidr=$(netmask_to_cidr "$netmask") || return 1
  echo "$network/$cidr"
}

check_interface() {
  if ! awk -v iface="$EXT_IFACE" '$1 ~ iface":" {exit 0} ENDFILE {exit 1}' /proc/net/dev 2>/dev/null; then
    echo "Interface $EXT_IFACE not found. Available interfaces:" >&2
    awk '/:/ {print $1}' /proc/net/dev | cut -d: -f1 | grep -v lo >&2
    return 1
  fi
}

enable_nat() {
  local subnet
  subnet=$(get_subnet) || exit 1
  check_interface || return 1

  # Create new table only if it doesn't exist
  if ! nft list table ip nat &>/dev/null; then
    nft create table ip nat
  fi

  # Add chains if they don't exist
  if ! nft list chain ip nat prerouting &>/dev/null; then
    nft add chain ip nat prerouting { type nat hook prerouting priority dstnat \; }
  fi

  if ! nft list chain ip nat postrouting &>/dev/null; then
    nft add chain ip nat postrouting { type nat hook postrouting priority srcnat \; }
  fi

  # Delete existing masquerade rule if exists
  nft delete rule ip nat postrouting handle $(nft -a list chain ip nat postrouting | grep "ip saddr $subnet" | awk '{print $NF}') 2>/dev/null || true

  # Add new masquerade rule
  nft add rule ip nat postrouting ip saddr $subnet oifname $EXT_IFACE masquerade

  echo "Enabled NAT for $subnet on $EXT_IFACE"
}

disable_nat() {
  # Delete our specific NAT rule if it exists
  local subnet=$(get_subnet 2>/dev/null)
  if [ -n "$subnet" ]; then
    local handle=$(nft -a list chain ip nat postrouting 2>/dev/null | \
                  awk -v subnet="$subnet" '/ip saddr == subnet/ {print $NF}')
    [ -n "$handle" ] && nft delete rule ip nat postrouting handle $handle
  fi

  # Simply delete our chains if they exist (Docker doesn't use them)
  nft delete chain ip nat prerouting 2>/dev/null && echo "Deleted prerouting chain"
  nft delete chain ip nat postrouting 2>/dev/null && echo "Deleted postrouting chain"

  # Verify cleanup
  if ! nft list chain ip nat postrouting 2>/dev/null | grep -q "ip saddr $subnet"; then
    echo "NAT disabled successfully"
  else
    echo "Failed to completely disable NAT" >&2
    return 1
  fi
}

show_status() {
  local subnet
  if subnet=$(get_subnet 2>/dev/null); then
    echo "VPN Subnet: $subnet"
  else
    echo "VPN Subnet: Unknown (config error)"
  fi

  echo -n "NAT Status: "
  if nft list chain ip nat postrouting 2>/dev/null | grep -q "ip saddr $subnet"; then
    echo "Active on $EXT_IFACE"
  else
    echo "Inactive"
  fi

  echo "Current NAT Rules:"
  nft list table ip nat 2>/dev/null || echo "No NAT rules configured"
}

validate_root
case "$1" in
  enable) enable_nat ;;
  disable) disable_nat ;;
  status) show_status ;;
  *) echo "Usage: EXT_IFACE=<interface> $0 {enable|disable|status}"; exit 1 ;;
esac
