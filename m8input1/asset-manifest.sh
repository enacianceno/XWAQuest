#!/system/bin/sh
# Prepend asset-selection.sh before streaming to run-as. Never writes source.
set -eu
set -o pipefail
export LC_ALL=C
cd "$1"
test -z "$(find . ! -type d ! -type f)" || { echo "Special file/link in asset tree" >&2; exit 1; }
asset_paths | while IFS= read -r file; do
    case "$file" in ./*) ;; *) echo "Unsafe asset path" >&2; exit 1;; esac
    test -f "$file" && test ! -L "$file" || { echo "Missing selected asset: $file" >&2; exit 1; }
    hash=$(sha256sum "$file")
    hash=${hash%% *}
    bytes=$(stat -c %s "$file")
    printf "%s\t%s\t%s\n" "$hash" "$bytes" "$file"
done
