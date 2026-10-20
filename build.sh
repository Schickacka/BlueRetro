#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f "components/queue_bss/liblfds/liblfds711/inc/liblfds711.h" ]; then
    if [ -x "./scripts/fetch_liblfds.sh" ]; then
        ./scripts/fetch_liblfds.sh
    elif [ -f "./scripts/fetch_liblfds.sh" ]; then
        bash ./scripts/fetch_liblfds.sh
    fi
fi

if ! command -v idf.py &> /dev/null; then
    echo "Error: idf.py not found in PATH. Please run '. \$IDF_PATH/export.sh' first."
    exit 1
fi

echo "Setting up PlayStation hw1 configuration..."
cp configs/hw1/playstation sdkconfig

echo "Building BlueRetro PS1 Custom Edition..."
BR_HW=_hw1 BR_SYS=_playstation idf.py reconfigure build

echo "Build complete. Artifacts in build/"
