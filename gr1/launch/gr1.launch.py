from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    params = PathJoinSubstitution([
        FindPackageShare("gr1"), "config", "params.yaml"
    ])

    # Namespace is intentionally not set here — provide it at the robot or
    # fleet level so the same driver works identically on every robot.
    #
    # Example from a parent launch file:
    #   IncludeLaunchDescription(..., launch_arguments={"__ns": "/robot1"})

    driver = Node(
        package="gr1",
        executable="gamma_detector",
        name="gr1",
        parameters=[params],
        output="screen",
    )

    return LaunchDescription([driver])
