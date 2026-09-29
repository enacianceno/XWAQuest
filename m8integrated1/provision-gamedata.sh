#!/system/bin/sh
set -eu
set -o pipefail
[ "${1:-}" = "--approved-m8-copy" ] || exit 1
helper=$2
SRC=org.openxwa.xwaquest.m8test3
DST=org.openxwa.xwaquest.m8integrated1
stage=files/.m8-gamedata-import-v3
fail() { echo "M8_DATA_STOP $*" >&2; exit 1; }
if pidof "$SRC" >/dev/null 2>&1; then fail "Close M7B normally before copying"; fi
run-as "$SRC" sh -c 'test -d files/GameData && test ! -L files/GameData && test ! -L files' || fail "M7 GameData unavailable"
run-as "$DST" sh -c 'set -eu; test ! -L files; mkdir -p files; test ! -e files/GameData && test ! -L files/GameData && test ! -e files/.m8-gamedata-import-v3 && test ! -L files/.m8-gamedata-import-v3' || fail "Destination/staging exists; no overwrite"
am force-stop "$DST"
run-as "$DST" sh -c 'set -eu; umask 077; mkdir files/.m8-gamedata-import-v3; mkdir files/.m8-gamedata-import-v3/data'
cat "$helper" | run-as "$SRC" sh -s -- files/GameData |
    run-as "$DST" sh -c 'cat > files/.m8-gamedata-import-v3/manifest.tsv'
count=$(run-as "$DST" sh -c 'wc -l < files/.m8-gamedata-import-v3/manifest.tsv')
bytes=$(run-as "$DST" cat "$stage/manifest.tsv" | awk -F '\t' '{total+=$2} END {printf "%.0f",total}')
[ "$count" -gt 0 ] && [ "$bytes" -gt 0 ] || fail "Empty manifest"
available=$(run-as "$DST" df -Pk files | awk 'END {print $4}')
[ "$available" -gt "$(( (bytes + 1023) / 1024 + count * 4 + 262144 ))" ] || fail "Insufficient free space"
echo "M8_DATA_SOURCE_VALIDATED assets=$count bytes=$bytes manifest=manifest.tsv"
# Only filenames from this exact manifest enter tar. No full-tree copy.
run-as "$DST" cut -f3 "$stage/manifest.tsv" |
    run-as "$SRC" tar -cf - -C files/GameData -T - |
    run-as "$DST" tar -xf - -C "$stage/data"
cat "$helper" | run-as "$SRC" sh -s -- files/GameData |
    run-as "$DST" sh -c 'cat > files/.m8-gamedata-import-v3/source-after.tsv'
cat "$helper" | run-as "$DST" sh -s -- "$stage/data" |
    run-as "$DST" sh -c 'cat > files/.m8-gamedata-import-v3/destination.tsv'
run-as "$DST" sh -c '
set -eu
cd files/.m8-gamedata-import-v3
cmp manifest.tsv source-after.tsv
cmp manifest.tsv destination.tsv
test "$(find data -type f | wc -l)" -eq "$(wc -l < manifest.tsv)"
' || fail "Hash/path/size/source verification failed; final GameData absent"

run-as "$DST" sh -c '
set -eu
test ! -e files/GameData && test ! -L files/GameData
find files/.m8-gamedata-import-v3/data -type f -exec chmod 600 {} +
find files/.m8-gamedata-import-v3/data -type d -exec chmod 700 {} +
mv -T -n files/.m8-gamedata-import-v3/data files/GameData
test ! -e files/.m8-gamedata-import-v3/data
test -d files/GameData
' || fail "Promotion failed; no overwrite requested"
cat "$helper" | run-as "$DST" sh -s -- files/GameData |
    run-as "$DST" sh -c 'cat > files/.m8-gamedata-import-v3/final.tsv'
run-as "$DST" sh -c 'cmp files/.m8-gamedata-import-v3/manifest.tsv files/.m8-gamedata-import-v3/final.tsv' || fail "Final verification failed"
echo "M8_DATA_HASHES_OK assets=$count bytes=$bytes source_unchanged=1 destination_matches=1 final_matches=1"
echo "M8_DATA_READY package=$DST assets=$count bytes=$bytes private_writable=1 pilot_copied=0"
