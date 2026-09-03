#!/usr/bin/env bash
# Validate that relative Markdown links in README and docs resolve to files in
# the repository. Remote links, anchors, and mailto/doi links are ignored.
#
# Usage: scripts/check-links.sh [file-or-dir ...]
# Defaults to README*.md and the docs/ tree.

set -u

targets=("$@")
if [ "${#targets[@]}" -eq 0 ]; then
    targets=(README.md README.tr.md README.de.md README.es.md README.fr.md docs)
fi

bad=0
checked=0

link_re='(href=|]\(|^\[[^]]*\]:[[:space:]]*)([^"'"'"'>[:space:]]+|[^[:space:]\\\)]+)'

extract() {
    # Markdown inline links `[label](target)` and HTML links `href="target"`.
    grep -oP '\]\(\K[^)]+' "$1" 2>/dev/null
    grep -oP 'href="\K[^"]+' "$1" 2>/dev/null
}

is_remote() {
    case "$1" in
        http://* | https://* | //* | mailto:* | '#'* | '') return 0 ;;
        *) return 1 ;;
    esac
}

for file in "${targets[@]}"; do
    if [ -d "$file" ]; then
        mapfile -t files < <(find "$file" -name '*.md' -type f | sort)
    elif [ -f "$file" ]; then
        files=("$file")
    else
        echo "SKIP (not found): $file"
        continue
    fi

    for md in "${files[@]}"; do
        base_dir="$(dirname "$md")"
        while IFS= read -r link; do
            [ -z "$link" ] && continue
            is_remote "$link" && continue

            # Strip a #fragment and any trailing punctuation (e.g. trailing dot
            # or comma at the end of a sentence).
            target="${link%%#*}"
            while [ -n "$target" ]; do
                case "$target" in
                    *.) target="${target%?}" ;;
                    *,) target="${target%?}" ;;
                    *:) target="${target%?}" ;;
                    *) break ;;
                esac
            done
            [ -z "$target" ] && continue

            checked=$((checked + 1))
            if [ ! -e "$base_dir/$target" ]; then
                echo "BROKEN: $md -> $link"
                bad=$((bad + 1))
            fi
        done < <(extract "$md")
    done
done

echo "Checked $checked relative markdown link(s)."
if [ "$bad" -gt 0 ]; then
    echo "Found $bad broken link(s)."
    exit 1
fi
echo "All relative markdown links resolve."
exit 0
