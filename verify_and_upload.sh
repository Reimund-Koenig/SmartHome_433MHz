#!/bin/bash

# === Kompilieren ===
echo "🔎 Starte Verifizierung mit verify.sh..."
./verify.sh

# Prüfe, ob Kompilierung erfolgreich war
if [ $? -ne 0 ]; then
  echo "❌ Verifizierung fehlgeschlagen – kein Upload."
  exit 1
fi

# === Upload ===
echo "🚀 Verifizierung erfolgreich – starte Upload..."
./upload.sh

# Prüfe Upload-Erfolg
if [ $? -ne 0 ]; then
  echo "❌ Upload fehlgeschlagen."
  exit 1
fi

echo "✅ Alles abgeschlossen: Sketch verifiziert & hochgeladen."
