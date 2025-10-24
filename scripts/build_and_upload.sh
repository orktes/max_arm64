#!/bin/bash
set -e

ip=$1
gamePath=$2
shPath=$3

if [ -z "$ip" ]; then
  echo "Usage: $0 <ip> [gamePath] [shPath]"
  exit 1
fi

make archive
rsync -aP --rsh="sshpass -p ark ssh -o StrictHostKeyChecking=no -l ark" package/maxpayne/* ark@$ip:$gamePath

if [ ! -z "$ip" ]; then
  rsync -aP --rsh="sshpass -p ark ssh -o StrictHostKeyChecking=no -l ark" "package/Max Payne.sh" ark@$ip:$shPath
fi