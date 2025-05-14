#!/usr/bin/env bash
set -euo pipefail

BASE_URL="https://raw.githubusercontent.com/jesperkha/buddy/main"
OUTPUT="foo.h"

rm -f "$OUTPUT"

curl -sSL "$BASE_URL/buddy.h" >> "$OUTPUT"
printf "\n" >> "$OUTPUT"

printf "#ifdef BUDDY_IMPLEMENTATION\n\n" >> "$OUTPUT"

curl -sSL "$BASE_URL/buddy.c" >> "$OUTPUT"
printf "\n" >> "$OUTPUT"

printf "#endif /* BUDDY_IMPLEMENTATION */\n" >> "$OUTPUT"

echo "Done"
