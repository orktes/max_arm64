#!/bin/bash

tag=$(git describe --exact-match --tags HEAD 2>/dev/null || true)
if [ -n "${tag}" ]; then
    printf "%s\nBuilt: %s\n" "${tag}" "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
else
    printf "%s\nBuilt: %s\n" "$(git rev-parse HEAD)" "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
fi