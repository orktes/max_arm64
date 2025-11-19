#!/bin/bash
# Check GLIBC version requirements for maxpayne_arm64 binary
# This ensures compatibility with target devices that have GLIBC 2.29

set -e

BINARY="${1:-maxpayne_arm64}"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary not found: $BINARY"
    echo "Usage: $0 [binary_path]"
    exit 1
fi

echo "Checking GLIBC version requirements for: $BINARY"
echo "================================================"

# Get all GLIBC versions
GLIBC_VERSIONS=$(objdump -T "$BINARY" | grep GLIBC_ | awk '{print $5}' | grep -v "^0*$" | sort -u)

if [ -z "$GLIBC_VERSIONS" ]; then
    echo "WARNING: No GLIBC version information found"
    exit 0
fi

echo "Required GLIBC versions:"
echo "$GLIBC_VERSIONS"
echo ""

# Get the maximum version
MAX_GLIBC=$(echo "$GLIBC_VERSIONS" | sort -V | tail -n1)
echo "Maximum GLIBC version: $MAX_GLIBC"
echo ""

# Check if it exceeds 2.29
MAX_ALLOWED="GLIBC_2.29"

if [ "$MAX_GLIBC" != "$MAX_ALLOWED" ]; then
    # Extract version numbers for comparison
    MAX_VER=$(echo "$MAX_GLIBC" | sed 's/GLIBC_//')
    ALLOWED_VER=$(echo "$MAX_ALLOWED" | sed 's/GLIBC_//')
    
    if [ "$(printf '%s\n' "$ALLOWED_VER" "$MAX_VER" | sort -V | head -n1)" != "$MAX_VER" ]; then
        echo "✗ FAIL: Binary requires $MAX_GLIBC but must not exceed $MAX_ALLOWED"
        echo ""
        echo "This binary will NOT work on target devices!"
        echo "Target devices only support up to $MAX_ALLOWED"
        echo ""
        echo "Common causes:"
        echo "  - Using sysconf() instead of getpagesize()"
        echo "  - Using newer C library functions"
        echo "  - Linking against newer system libraries"
        exit 1
    fi
fi

echo "✓ PASS: GLIBC version requirements are compatible with target devices"
exit 0
