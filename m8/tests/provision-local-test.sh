#!/bin/sh
# Host-only test harness. run-as/am/pidof are shell functions, never device tools.
root=$1
worker=$2
scenario=$3
run-as() {
    package=$1
    shift
    case "$package" in
        org.openxwa.xwaquest.m7) base="$root/source";;
        org.openxwa.xwaquest.m8) base="$root/$scenario";;
        *) return 99;;
    esac
    case "$1" in
        du) echo "1024 files/GameData";;
        df) echo "Filesystem 1024-blocks Used Available Capacity Mounted"; echo "mock 999999 0 999999 0% /";;
        *)
            (cd "$base" && "$@") || return $?
            if [ "$scenario" = corrupt ] && [ "$package" = org.openxwa.xwaquest.m8 ] && [ "$1" = tar ]; then
                printf corrupted > "$base/files/.m8-gamedata-import-v3/data/RESDATA.TXT"
            fi
            if [ "$scenario" = sourcechange ] && [ "$package" = org.openxwa.xwaquest.m8 ] && [ "$1" = tar ]; then
                printf changed > "$root/source/files/GameData/RESDATA.TXT"
            fi
            ;;
    esac
}
am() { [ "$*" = "force-stop org.openxwa.xwaquest.m8" ]; }
pidof() { return 1; }
set -- --approved-m8-copy "$4"
. "$worker"
