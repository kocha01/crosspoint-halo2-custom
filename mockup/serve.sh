#!/bin/bash
export PATH="/opt/homebrew/bin:$PATH"
cd "$(dirname "$0")"
exec /opt/homebrew/bin/python3.13 -m http.server "$@"
