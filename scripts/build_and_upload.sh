#!/bin/bash
set -e

ip=$1

if [ -z "$ip" ]; then
  echo "Usage: $0 <ip>"
  exit 1
fi

make archive
rsync -aP --rsh="sshpass -p ark ssh -o StrictHostKeyChecking=no -l ark" package/maxpayne ark@$ip:/roms/ports/maxpayne
rsync -aP --rsh="sshpass -p ark ssh -o StrictHostKeyChecking=no -l ark" "package/Max Payne.sh" ark@$ip:/roms/ports/