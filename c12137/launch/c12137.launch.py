from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    params = PathJoinSubstitution([
        FindPackageShare("c12137"), "config", "params.yaml"
    ])

    # Namespace is intentionally not set here — it should be provided at the
    # robot or fleet level (e.g. via __ns argument or a parent launch file).
    # Every robot in the fleet can run this node under the same name;
    # the robot namespace keeps them separate on the ROS graph.
    #
    # Example from a parent launch file:
    #   IncludeLaunchDescription(..., launch_arguments={"__ns": "/robot1"})

    driver = Node(
        package="c12137",
        executable="gamma_detector",
        name="c12137",
        parameters=[params],
        output="screen",
    )

    return LaunchDescription([driver])
