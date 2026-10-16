#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PS5_ROOT="${PS5_ROOT:-$HOME/PS5}"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-$PS5_ROOT/ps5-native-app-boilerplate/.deps/native/ps5-payload-sdk}"
PS5_HOST="${PS5_HOST:-192.168.1.85}"
FTP_PORT="${FTP_PORT:-2121}"
TITLE_ID="${TITLE_ID:-PPSA98542}"

LOG="$(mktemp)"
"$HERE/build.sh" -k 0 > "$LOG" 2>&1 || true
if grep -E "error:" "$LOG" | grep -v -e prospero-lld -e "linker command failed"; then
    echo "build failed: nothing uploaded" >&2
    exit 1
fi
python3 "$HERE/make_folder.py" | tail -1
"$HERE/upload_assets.sh" || true

for attempt in 1 2 3 4 5 6; do
    if curl -s -T "$HERE/../dist/$TITLE_ID/eboot.bin" "ftp://$PS5_HOST:$FTP_PORT/data/homebrew/$TITLE_ID/eboot.bin"; then
        echo "uploaded eboot.bin"
        exit 0
    fi
    echo "upload failed (attempt $attempt): the app is probably open; retrying in 5 s"
    sleep 5
done
echo "upload failed: close the app on the console and run this again" >&2
exit 1
