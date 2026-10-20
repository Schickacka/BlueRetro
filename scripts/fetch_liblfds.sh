#!/usr/bin/env bash
set -e

LIBLFDS_DIR="components/queue_bss/liblfds"

if [ -f "$LIBLFDS_DIR/liblfds711/inc/liblfds711.h" ]; then
    echo "liblfds already populated."
    exit 0
fi

echo "Fetching liblfds submodule..."
git submodule update --init --recursive "$LIBLFDS_DIR" || {
    echo "Submodule init failed, cloning directly..."
    rm -rf "$LIBLFDS_DIR"
    git clone https://github.com/darthcloud/liblfds7.1.1.git "$LIBLFDS_DIR"
}

echo "liblfds ready."
