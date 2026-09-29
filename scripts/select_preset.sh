#!/usr/bin/env bash

set -e

PRESET="$1"

if [[ -z "$PRESET" ]]; then
    echo "Usage: $0 <preset>"
    exit 1
fi

cmake --preset "$PRESET"

ln -sfn "build/$PRESET/compile_commands.json" compile_commands.json

echo "compile_commands.json -> build/$PRESET/compile_commands.json"