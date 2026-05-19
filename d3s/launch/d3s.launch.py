from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    params = PathJoinSubstitution([
        FindPackageShare("d3s"), "config", "params.yaml"
    ])

    # Namespace is intentionally not set here — provide it at the robot or
    # fleet level so the same driver works identically on every robot.

    driver = Node(
        package="d3s",
        executable="gamma_neutron_detector",
        name="d3s",
        parameters=[params],
        output="screen",
    )

    return LaunchDescription([driver])
