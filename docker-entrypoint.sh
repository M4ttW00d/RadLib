#!/bin/bash
set -e

source /opt/ros/humble/setup.bash
source /radlib_ws/install/setup.bash

ROBOT_NAME=${ROBOT_NAME:-robot1}
DETECTORS=${DETECTORS:-}
BAG_DIR=${BAG_DIR:-/data}

if [ -z "$DETECTORS" ]; then
    echo "RadLib detector drivers — no detectors configured."
    echo ""
    echo "Usage:"
    echo "  docker run --network=host \\"
    echo "    -v /host/data:/data \\"
    echo "    --device /dev/bus/usb \\"
    echo "    -e ROBOT_NAME=robot1 \\"
    echo "    -e DETECTORS=gr1,d3s \\"
    echo "    ghcr.io/m4ttw00d/radlib:latest"
    echo ""
    echo "  DETECTORS   comma-separated: c12137, gr1, sigma50, d3s"
    echo "  ROBOT_NAME  ROS namespace prefix          (default: robot1)"
    echo "  BAG_DIR     bag recording path in container (default: /data)"
    echo ""
    echo "  D3S requires --device /dev/ttyACM0 (or the relevant ttyACM* port)."
    echo "  USB HID/bulk detectors require --device /dev/bus/usb or --privileged."
    echo ""
    exec bash
fi

cleanup() {
    echo "Shutting down RadLib..."
    kill $(jobs -p) 2>/dev/null || true
    wait
}
trap cleanup EXIT INT TERM

declare -A PKG=(
    [gr1]="gr1 gamma_detector"
    [sigma50]="sigma50 gamma_detector"
    [c12137]="c12137 gamma_detector"
    [d3s]="d3s gamma_neutron_detector"
)

IFS=',' read -ra DET_ARRAY <<< "$DETECTORS"
for det in "${DET_ARRAY[@]}"; do
    det=$(echo "$det" | xargs)
    if [[ -z "${PKG[$det]+x}" ]]; then
        echo "Error: unknown detector '$det'. Valid: c12137, gr1, sigma50, d3s"
        exit 1
    fi
    read -ra parts <<< "${PKG[$det]}"
    echo "Starting $det — topics under /$ROBOT_NAME/$det/ ..."
    ros2 run "${parts[0]}" "${parts[1]}" \
        --ros-args -r __ns:=/"$ROBOT_NAME" -r __node:="$det" &
done

mkdir -p "$BAG_DIR"
BAG_NAME="$BAG_DIR/bag_$(date +%Y%m%d_%H%M%S)"
echo "Recording all topics to $BAG_NAME"
ros2 bag record -a -o "$BAG_NAME" &

echo ""
echo "RadLib running. Ctrl-C to stop."
wait
