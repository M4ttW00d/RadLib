#!/bin/bash
set -e

source /opt/ros/humble/setup.bash
source /radlib_ws/install/setup.bash

ROBOT_NAME=${ROBOT_NAME:-robot1}
DETECTORS=${DETECTORS:-}
BAG_DIR=${BAG_DIR:-/data}

if [ -z "$DETECTORS" ]; then
    echo "RadLib detector drivers: no detectors configured."
    echo ""
    echo "Usage:"
    echo "  docker run --network=host \\"
    echo "    -v /host/data:/data \\"
    echo "    --device /dev/hidraw0 \\"
    echo "    --device /dev/ttyACM0 \\"
    echo "    -e ROBOT_NAME=robot1 \\"
    echo "    -e DETECTORS=gr1,d3s \\"
    echo "    ghcr.io/m4ttw00d/radlib:latest"
    echo ""
    echo "  DETECTORS   comma-separated: c12137, gr1, sigma50, d3s"
    echo "  ROBOT_NAME  ROS namespace prefix          (default: robot1)"
    echo "  BAG_DIR     bag recording path in container (default: /data)"
    echo ""
    echo "  Device access (or run with --privileged):"
    echo "    gr1, sigma50   --device /dev/hidrawN   the kernel HID node, not /dev/bus/usb; find N with 'ls -l /dev/hidraw*'"
    echo "    c12137         --device /dev/bus/usb   raw USB through libusb"
    echo "    d3s            --device /dev/ttyACM0   over USB, or /dev/rfcomm0 over Bluetooth SPP"
    echo ""
    exec bash
fi

cleanup() {
    echo "Shutting down RadLib..."
    kill $(jobs -p) 2>/dev/null || true
    wait
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

declare -A PKG=(
    [gr1]="gr1 gamma_detector"
    [sigma50]="sigma50 gamma_detector"
    [c12137]="c12137 gamma_detector"
    [d3s]="d3s gamma_neutron_detector"
)

IFS=',' read -ra DET_ARRAY <<< "$DETECTORS"
NODE_PIDS=()
for det in "${DET_ARRAY[@]}"; do
    det=$(echo "$det" | xargs)
    if [[ -z "${PKG[$det]+x}" ]]; then
        echo "Error: unknown detector '$det'. Valid: c12137, gr1, sigma50, d3s"
        exit 1
    fi
    read -ra parts <<< "${PKG[$det]}"
    echo "Starting $det, topics under /$ROBOT_NAME/$det/ ..."
    ros2 run "${parts[0]}" "${parts[1]}" \
        --ros-args -r __ns:=/"$ROBOT_NAME" -r __node:="$det" &
    NODE_PIDS+=("$!")
done

mkdir -p "$BAG_DIR"
BAG_NAME="$BAG_DIR/bag_$(date +%Y%m%d_%H%M%S)"
echo "Recording all topics to $BAG_NAME"
ros2 bag record -a -o "$BAG_NAME" &
BAG_PID=$!

echo ""
echo "RadLib running. Ctrl-C to stop."

# Stop everything as soon as any detector node or the recorder exits, and leave
# with a non-zero status so a restart policy can bring the container back.
set +e
wait -n "${NODE_PIDS[@]}" "$BAG_PID"
status=$?
for pid in "${NODE_PIDS[@]}"; do
    if ! kill -0 "$pid" 2>/dev/null; then
        echo "Error: a detector node exited with status $status; stopping."
        exit 1
    fi
done
echo "Error: ros2 bag record exited with status $status; stopping."
exit 1
