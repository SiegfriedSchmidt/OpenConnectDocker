#!/bin/bash
set -e

# VPN NAT Manager

CONFIG="/ocserv.conf"
EXT_IFACE=${EXT_IFACE:-eth0}  # Default to eth0 if not specified

validate_root() {
  [ "$(id -u)" -eq 0 ] || { echo "Run as root" >&2; exit 1; }
}

# Detect available NAT backend
detect_nat_backend() {
  # Check for iptables-legacy first (Synology compatibility)
  if command -v nft >/dev/null && nft list ruleset >/dev/null 2>&1; then
    echo "nftables"
  elif command -v iptables-legacy >/dev/null && iptables-legacy -L >/dev/null 2>&1; then
    echo "iptables-legacy"
  elif command -v iptables >/dev/null && iptables -L >/dev/null 2>&1; then
    echo "iptables"
  else
    echo "none"
  fi
}

NAT_BACKEND=$(detect_nat_backend)

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

# Backend implementations
enable_nat_nft() {
  local subnet=$1
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

  nft delete rule ip nat postrouting handle $(nft -a list chain ip nat postrouting | grep "ip saddr $subnet" | awk '{print $NF}') 2>/dev/null || true
  nft add rule ip nat postrouting ip saddr $subnet oifname $EXT_IFACE masquerade
}

disable_nat_nft() {
  local subnet=$1
  local handle=$(nft -a list chain ip nat postrouting 2>/dev/null | \
                awk -v subnet="$subnet" '/ip saddr == subnet/ {print $NF}')
  [ -n "$handle" ] && nft delete rule ip nat postrouting handle $handle
  
  nft delete chain ip nat prerouting 2>/dev/null || true
  nft delete chain ip nat postrouting 2>/dev/null || true
}

# IPTables implementation
enable_nat_ipt() {
  local subnet=$1
  local ipt_cmd=${2:-iptables}
  $ipt_cmd -t nat -C POSTROUTING -s $subnet -o $EXT_IFACE -j MASQUERADE 2>/dev/null || \
  $ipt_cmd -t nat -A POSTROUTING -s $subnet -o $EXT_IFACE -j MASQUERADE
}

disable_nat_ipt() {
  local ipt_cmd=${1:-iptables}
  $ipt_cmd -t nat -D POSTROUTING -s $(get_subnet) -o $EXT_IFACE -j MASQUERADE 2>/dev/null || true
}

# Main functions
enable_nat() {
  local subnet
  subnet=$(get_subnet) || exit 1
  check_interface || return 1

  case "$NAT_BACKEND" in
    nftables)
      enable_nat_nft "$subnet"
      ;;
    iptables|iptables-legacy)
      enable_nat_ipt "$subnet" "$NAT_BACKEND"
      ;;
    *)
      echo "No supported NAT backend found" >&2
      return 1
      ;;
  esac

  echo "Enabled NAT for $subnet on $EXT_IFACE using $NAT_BACKEND"
}

disable_nat() {
  case "$NAT_BACKEND" in
    nftables)
      disable_nat_nft
      ;;
    iptables|iptables-legacy)
      disable_nat_ipt "$NAT_BACKEND"
      ;;
    *)
      echo "No supported NAT backend found" >&2
      return 1
      ;;
  esac

  echo "Disabled NAT using $NAT_BACKEND"
}

show_status() {
  local subnet
  if subnet=$(get_subnet 2>/dev/null); then
    echo "VPN Subnet: $subnet"
  else
    echo "VPN Subnet: Unknown (config error)"
  fi

  echo -n "NAT Status: "
  case "$NAT_BACKEND" in
    nftables)
      if nft list chain ip nat postrouting 2>/dev/null | grep -q "ip saddr $subnet"; then
        echo "Active on $EXT_IFACE (nftables)"
      else
        echo "Inactive"
      fi
      echo "Current Rules:"
      nft list table ip nat 2>/dev/null
      ;;
    iptables|iptables-legacy)
      local ipt_cmd="$NAT_BACKEND"
      if $ipt_cmd -t nat -nL POSTROUTING 2>/dev/null | grep -q "$subnet"; then
        echo "Active on $EXT_IFACE ($NAT_BACKEND)"
      else
        echo "Inactive"
      fi
      echo "Current Rules:"
      $ipt_cmd -t nat -nL 2>/dev/null
      ;;
    *)
      echo "No NAT backend available"
      ;;
  esac
}

validate_root
case "$1" in
  enable) enable_nat ;;
  disable) disable_nat ;;
  status) show_status ;;
  *) echo "Usage: EXT_IFACE=<interface> $0 {enable|disable|status}"; exit 1 ;;
esac
