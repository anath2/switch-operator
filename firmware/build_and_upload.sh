#!/bin/bash

# Usage: ./build_and_upload.sh <sketch_dir> <serial_port>
# Example: ./build_and_upload.sh MySketch /dev/tty.usbmodemXYZ

set -e

SKETCH_DIR=${1:-firmware}
SERIAL_PORT=${2:-/dev/cu.usbmodem21201}

FQBN="esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=default,FlashSize=4M,EraseFlash=all"

# Compile
arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR"

# Upload
arduino-cli upload -p "$SERIAL_PORT" --fqbn "$FQBN" "$SKETCH_DIR"
