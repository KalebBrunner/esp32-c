#!/usr/bin/env bash
# Run with: bash esp32.sh help
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "$project_dir"

usage() {
    cat <<'HELP'
ESP32 helper (run from this folder)
  bash esp32.sh build       Compile firmware without uploading
  bash esp32.sh upload      Build and upload; stop the monitor first
  bash esp32.sh monitor     Read USB serial output; Ctrl+C exits
  bash esp32.sh ports       List connected serial devices
  bash esp32.sh clean       Remove generated build output
  bash esp32.sh compiledb   Generate editor compiler information
  bash esp32.sh verbose     Build and show compiler commands
  bash esp32.sh notes       Print commands, settings, and BOOT/RESET instructions
  bash esp32.sh help        Show this list

After upload, if the board stays in download mode:
release BOOT, then press and release RESET.
HELP
}

action="${1:-help}"
case "$action" in
    help|-h|--help) usage; exit 0 ;;
    notes) cat QUICKSTART.txt; exit 0 ;;
    build|upload|monitor|ports|clean|compiledb|verbose) ;;
    *) printf 'Unknown command: %s\n' "$action" >&2; usage >&2; exit 2 ;;
esac

if ! command -v pio >/dev/null 2>&1; then
    printf 'PlatformIO (pio) is missing from PATH. See QUICKSTART.txt.\n' >&2
    exit 127
fi

case "$action" in
    build) exec pio run ;;
    upload)
        printf 'Stop other serial monitors before uploading.\n'
        printf 'If it remains in download mode afterward, release BOOT and press RESET.\n'
        exec pio run --target upload ;;
    monitor) exec pio device monitor ;;
    ports) exec pio device list ;;
    clean) exec pio run --target clean ;;
    compiledb) exec pio run --target compiledb ;;
    verbose) exec pio run -v ;;
esac
