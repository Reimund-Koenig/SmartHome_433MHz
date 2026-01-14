#!/bin/bash

SKETCH_DIR="./"
BUILD_DIR="$SKETCH_DIR/build"
FQBN="esp8266:esp8266:d1"
PORT="COM3" # Adapt to your COM Port of ESP32
echo "🚀 Lade bereits kompilierte Sketch-Dateien aus $BUILD_DIR auf $PORT hoch..."

arduino-cli upload \
  -p "$PORT" \
  --fqbn "$FQBN" \
  --input-dir "$BUILD_DIR" \
  --verbose \
  "$SKETCH_DIR"

if [ $? -ne 0 ]; then
  echo "❌ Upload fehlgeschlagen."
  exit 1
fi

echo "✅ Upload erfolgreich abgeschlossen."
