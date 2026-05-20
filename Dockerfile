FROM ros:humble

RUN apt-get update && apt-get install -y \
    libusb-1.0-0-dev \
    libhidapi-dev \
    python3-colcon-common-extensions \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /radlib_ws

COPY c12137  src/c12137
COPY gr1     src/gr1
COPY sigma50 src/sigma50
COPY d3s     src/d3s

RUN /bin/bash -c "\
    source /opt/ros/humble/setup.bash && \
    colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release && \
    rm -rf build log"

COPY docker-entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

VOLUME ["/data"]

ENTRYPOINT ["/entrypoint.sh"]
