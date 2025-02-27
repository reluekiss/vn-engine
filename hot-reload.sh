#!/bin/bash
set -f

pattern="${1:-*.c}"
cmd="${2:-make}"

get_checksum() {
    files=$(find . -type f -name "$pattern" | sort)
    if [ -z "$files" ]; then
        echo ""
    else
        echo "$files" | xargs md5sum | md5sum
    fi
}

checksum=$(get_checksum)
if [ -z "$checksum" ]; then
    echo "No files found with pattern: $pattern"
    exit 1
fi

while true; do
    inotifywait -e modify,create,delete,move -r .
    new_checksum=$(get_checksum)
    if [ "$checksum" != "$new_checksum" ]; then
        $cmd
        checksum="$new_checksum"
    fi
done
