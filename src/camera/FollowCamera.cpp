#include "FollowCamera.hpp"

namespace hyperion::v2 {
FollowCamera::FollowCamera(
    const Vector3 &target, const Vector3 &offset,
    int width, int height,
    float fov,
    float _near, float _far
) : PerspectiveCamera(fov, width, height, _near, _far),
    m_offset(offset),
    m_real_offset(offset),
    m_mx(0.0f),
    m_my(0.0f),
    m_prev_mx(0.0f),
    m_prev_my(0.0f),
    m_desired_distance(target.Distance(offset))
{
    SetTarget(target);
}

void FollowCamera::UpdateLogic(double dt)
{
    m_real_offset.Lerp(m_offset, MathUtil::Clamp(static_cast<float>(dt) * 25.0f, 0.0f, 1.0f));

    const auto origin = GetTarget();
    const auto normalized_offset_direction = (origin - (origin + m_real_offset)).Normalized();

    SetTranslation(origin + normalized_offset_direction * m_desired_distance);
}

void FollowCamera::RespondToCommand(const CameraCommand &command, GameCounter::TickUnit dt)
{
    switch (command.command) {
    case CameraCommand::CAMERA_COMMAND_MAG:
    {
        m_mx = command.mag_data.mx;
        m_my = command.mag_data.my;

        m_mag = {
            m_mx - m_prev_mx,
            m_my - m_prev_my
        };

        constexpr float mouse_speed = 80.0f;

        m_offset = Vector3(
            -std::sin(m_mag.x * 4.0f) * mouse_speed,
            -std::sin(m_mag.y * 4.0f) * mouse_speed,
            std::cos(m_mag.x  * 4.0f) * mouse_speed
        );
    
        break;
    }
    case CameraCommand::CAMERA_COMMAND_SCROLL:
    {
        constexpr float scroll_speed = 150.0f;
        
        m_desired_distance -= static_cast<float>(command.scroll_data.wheel_y) * scroll_speed * dt;

        break;
    }
    case CameraCommand::CAMERA_COMMAND_MOVEMENT:
    {
        constexpr float movement_speed = 500.0f;
        const float speed = movement_speed * dt;

        const auto dir_cross_y = Vector3(m_direction).Cross(m_up);

        switch (command.movement_data.movement_type) {
        case CameraCommand::CAMERA_MOVEMENT_FORWARD:
            m_offset -= m_up * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_BACKWARD:
            m_offset += m_up * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_LEFT:
            m_offset += dir_cross_y * speed;

            break;
        case CameraCommand::CAMERA_MOVEMENT_RIGHT:
            m_offset -= dir_cross_y * speed;

            break;
        default:
            break;
        }

        break;
    }
    default:
        break;
    }
}

} // namespace hyperion::v2
