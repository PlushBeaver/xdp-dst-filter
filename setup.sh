#!/usr/bin/env sh
set -eu -o pipefail

if [ $# -eq 0 ]; then
    echo >&2 "Usage: $0 {up|down}"
    exit 1
fi
if [ `id -u` -ne 0 ]; then
    echo >&2 "Must be run as root."
    exit 1
fi
case "$1" in
    up)
        ip netns add xdp
        ip link add xdp0 type veth peer name xdp1
        ip link set dev xdp0 up
        ip address add 10.9.0.1/30 dev xdp0
        ip link set xdp1 netns xdp
        ip netns exec xdp  ip link set dev xdp1 up
        ip netns exec xdp  ip address add 10.9.0.2/30 dev xdp1
        ;;
    down)
        ip netns delete xdp
        ip link delete xdp0
        ;;
esac
