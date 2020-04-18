#include "camera.h"

namespace apex {

Camera::Camera(int width, int height, float near, float far)
    : m_width(width),
      m_height(height),
      m_near(near),
      m_far(far),
      m_translation(Vector3::Zero()),
      m_direction(Vector3::UnitZ()),
      m_up(Vector3::UnitY())
{
}

void Camera::SetTranslation(const Vector3 &translation)
{
    m_translation = translation;
}

void Camera::Rotate(const Vector3 &axis, float radians)
{
    m_direction.Rotate(axis, radians);
    m_direction.Normalize();
}

void Camera::Update(double dt)
{
    UpdateLogic(dt);
    UpdateMatrices();
}

} // namespace apex
