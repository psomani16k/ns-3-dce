#!/bin/bash

PREFIX=${1:-"backup"}
TIMESTAMP=$(date +"%d-%m-%Y_%H:%M:%S")
BACKUP_DIR="./results_backup/${PREFIX}_${TIMESTAMP}"

mkdir -p "$BACKUP_DIR"

# Move files if they exist to avoid error messages
mv files-* *.pcap *.xml "$BACKUP_DIR" 2>/dev/null

echo "Files moved to $BACKUP_DIR"
