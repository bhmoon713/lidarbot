#include "lidarbot_base/lidarbot_hardware.hpp"

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"

namespace lidarbot_base
{

LidarbotHardware::LidarbotHardware()
    : logger(rclcpp::get_logger("LidarbotHardware"))
{}

CallbackReturn LidarbotHardware::on_init(const hardware_interface::HardwareInfo & info)
{
    if (hardware_interface::SystemInterface::on_init(info) != CallbackReturn::SUCCESS)
    {
        return CallbackReturn::ERROR;
    }

    RCLCPP_INFO(logger, "Initializing...");

    time = std::chrono::system_clock::now();

    config.left_wheel_name = info_.hardware_parameters["left_wheel_name"];
    config.right_wheel_name = info_.hardware_parameters["right_wheel_name"];
    config.enc_ticks_per_rev = std::stoi(info_.hardware_parameters["enc_ticks_per_rev"]);

    // Set up wheels
    left_wheel.setup(config.left_wheel_name, config.enc_ticks_per_rev);
    right_wheel.setup(config.right_wheel_name, config.enc_ticks_per_rev);

    RCLCPP_INFO(logger, "Finished initialization");

    return CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface> LidarbotHardware::export_state_interfaces()
{
    // Set up a position and velocity interface for each wheel

    std::vector<hardware_interface::StateInterface> state_interfaces;

    state_interfaces.emplace_back(hardware_interface::StateInterface(left_wheel.name, hardware_interface::HW_IF_VELOCITY, &left_wheel.velocity));
    state_interfaces.emplace_back(hardware_interface::StateInterface(left_wheel.name, hardware_interface::HW_IF_POSITION, &left_wheel.position));
    state_interfaces.emplace_back(hardware_interface::StateInterface(right_wheel.name, hardware_interface::HW_IF_VELOCITY, &right_wheel.velocity));
    state_interfaces.emplace_back(hardware_interface::StateInterface(right_wheel.name, hardware_interface::HW_IF_POSITION, &right_wheel.position));

    return state_interfaces;
}

std::vector<hardware_interface::CommandInterface> LidarbotHardware::export_command_interfaces()
{
    // Set up a velocity command for each wheel

    std::vector<hardware_interface::CommandInterface> command_interfaces;

    command_interfaces.emplace_back(hardware_interface::CommandInterface(left_wheel.name, hardware_interface::HW_IF_VELOCITY, &left_wheel.command));
    command_interfaces.emplace_back(hardware_interface::CommandInterface(right_wheel.name, hardware_interface::HW_IF_VELOCITY, &right_wheel.command));

    return command_interfaces;
}

CallbackReturn LidarbotHardware::on_configure(const rclcpp_lifecycle::State & /*previous_state*/)
{
    RCLCPP_INFO(logger, "Configuring motor driver and encoders and GPIO pins...");

    // Motor Initialization
    Motor_Init();

    // Initialize wiringPi using GPIO BCM pin numbers
    wiringPiSetupGpio();
    
    // Setup GPIO encoder pins
    pinMode(LEFT_WHL_ENCODER, INPUT);
    pinMode(RIGHT_WHL_ENCODER, INPUT);

    // Setup pull up resistors on encoder pins
    pullUpDnControl(LEFT_WHL_ENCODER, PUD_UP);
    pullUpDnControl(RIGHT_WHL_ENCODER, PUD_UP);

    // Initialize encoder interrupts for falling signal states
    wiringPiISR(LEFT_WHL_ENCODER, INT_EDGE_FALLING, left_wheel_pulse);
    wiringPiISR(RIGHT_WHL_ENCODER, INT_EDGE_FALLING, right_wheel_pulse);

    RCLCPP_INFO(logger, "Successfully configured motors and GPIO pins!");

    return CallbackReturn::SUCCESS;
}

CallbackReturn LidarbotHardware::on_activate(const rclcpp_lifecycle::State & /*previous_state*/)
{
    RCLCPP_INFO(logger, "Starting controller ...");

    return CallbackReturn::SUCCESS;
}

CallbackReturn LidarbotHardware::on_deactivate(const rclcpp_lifecycle::State & /*previous_state*/)
{   
    RCLCPP_INFO(logger, "Stopping Controller...");

    signal(SIGINT, handler);

    return CallbackReturn::SUCCESS;
}

return_type LidarbotHardware::read(const rclcpp::Time & /*time*/, const rclcpp::Duration & period)
{
    // Calculate delta time
    auto new_time = std::chrono::system_clock::now();
    std::chrono::duration<double> diff = new_time - time;
    double delta_secs = diff.count();
    time = new_time;

    // Obtain encoder values
    read_encoder_values(left_wheel.encoder_ticks, right_wheel.encoder_ticks);

    // Calculate wheel positions and velocities
    double previous_position = left_wheel.position;
    left_wheel.position = left_wheel.calculate_encoder_angle();
    left_wheel.velocity = (left_wheel.position - previous_position) / delta_secs;

    previous_position = right_wheel.position;
    right_wheel.position = right_wheel.calculate_encoder_angle();
    right_wheel.velocity = (right_wheel.position - previous_position) / delta_secs;

    return return_type::OK;
}

return_type LidarbotHardware::write(const rclcpp::Time & /*time*/, const rclcpp::Duration & period)
{
    // Send commands to motor driver
    set_motor_speeds(left_wheel.command, right_wheel.command);

    return return_type::OK;
}

}// namespace lidarbot_base

#include "pluginlib/class_list_macros.hpp"

PLUGINLIB_EXPORT_CLASS(
    lidarbot_base::LidarbotHardware, 
    hardware_interface::SystemInterface)