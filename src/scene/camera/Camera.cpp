/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/camera/Camera.hpp>

#include <math/Halton.hpp>

#include <rendering/ShaderGlobals.hpp>
#include <rendering/Camera.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/system/AppContext.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Result;

class Camera;

#pragma region CameraController

CameraController::CameraController(CameraProjectionMode projection_mode)
    : m_input_handler(MakeUnique<NullInputHandler>()),
      m_camera(nullptr),
      m_projection_mode(projection_mode),
      m_command_queue_count { 0 },
      m_mouse_lock_requested(false)
{
}

void CameraController::OnAdded(Camera *camera)
{
    HYP_SCOPE;

    m_camera = camera;

    OnAdded();
}

void CameraController::OnAdded()
{
    // Do nothing
}

void CameraController::OnRemoved()
{
    // Do nothing
}

void CameraController::OnActivated()
{
    // Do nothing
}

void CameraController::OnDeactivated()
{
    // Do nothing
}

void CameraController::PushCommand(const CameraCommand &command)
{
    HYP_SCOPE;

    std::lock_guard guard(m_command_queue_mutex);

    ++m_command_queue_count;

    m_command_queue.Push(command);
}

void CameraController::UpdateCommandQueue(GameCounter::TickUnit dt)
{
    HYP_SCOPE;

    if (m_command_queue_count == 0) {
        return;
    }

    std::lock_guard guard(m_command_queue_mutex);

    while (m_command_queue.Any()) {
        RespondToCommand(m_command_queue.Front(), dt);

        m_command_queue.Pop();
    }

    m_command_queue_count = 0;
}

void CameraController::SetIsMouseLockRequested(bool mouse_lock_requested)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    if (mouse_lock_requested == m_mouse_lock_requested) {
        return;
    }

    m_mouse_lock_requested = mouse_lock_requested;
}

#pragma endregion CameraController

#pragma region NullCameraController

NullCameraController::NullCameraController()
    : CameraController(CameraProjectionMode::NONE)
{
}

void NullCameraController::UpdateLogic(double dt)
{
}

void NullCameraController::UpdateViewMatrix()
{
}

void NullCameraController::UpdateProjectionMatrix()
{
}

#pragma endregion NullCameraController

#pragma region Camera

Camera::Camera()
    : Camera(128, 128)
{
}

Camera::Camera(int width, int height)
    : HypObject(),
      m_name(Name::Unique("Camera_")),
      m_fov(50.0f),
      m_width(width),
      m_height(height),
      m_near(0.01f),
      m_far(1000.0f),
      m_left(0.0f),
      m_right(0.0f),
      m_bottom(0.0f),
      m_top(0.0f),
      m_translation(Vec3f::Zero()),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY()),
      m_render_resources(nullptr)
{
    // make sure there is always at least 1 camera controller
    m_camera_controllers.PushBack(MakeRefCountedPtr<NullCameraController>());
}

Camera::Camera(float fov, int width, int height, float _near, float _far)
    : HypObject(),
      m_name(Name::Unique("Camera_")),
      m_fov(fov),
      m_width(width),
      m_height(height),
      m_translation(Vec3f::Zero()),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY()),
      m_render_resources(nullptr)
{
    // make sure there is always at least 1 camera controller
    m_camera_controllers.PushBack(MakeRefCountedPtr<NullCameraController>());
    
    SetToPerspectiveProjection(fov, _near, _far);
}

Camera::Camera(int width, int height, float left, float right, float bottom, float top, float _near, float _far)
    : HypObject(),
      m_name(Name::Unique("Camera_")),
      m_fov(0.0f),
      m_width(width),
      m_height(height),
      m_translation(Vec3f::Zero()),
      m_direction(Vec3f::UnitZ()),
      m_up(Vec3f::UnitY()),
      m_render_resources(nullptr)
{
    // make sure there is always at least 1 camera controller
    m_camera_controllers.PushBack(MakeRefCountedPtr<NullCameraController>());

    SetToOrthographicProjection(left, right, bottom, top, _near, _far);
}

Camera::~Camera()
{
    while (HasActiveCameraController()) {
        const RC<CameraController> camera_controller = m_camera_controllers.PopBack();

        camera_controller->OnDeactivated();
        camera_controller->OnRemoved();
    }

    if (m_render_resources != nullptr) {
        m_render_resources->EnqueueUnbind();
        m_render_resources->Unclaim();
        FreeResource(m_render_resources);
    }

    SafeRelease(std::move(m_framebuffer));

    // Sync render commands to prevent dangling pointers to this
    HYP_SYNC_RENDER();
}

void Camera::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
    {
        if (m_render_resources != nullptr) {
            m_render_resources->Unclaim();
            FreeResource(m_render_resources);

            m_render_resources = nullptr;
        }

        SafeRelease(std::move(m_framebuffer));
    }));

    m_render_resources = AllocateResource<CameraRenderResources>(this);

    UpdateMatrices();

    m_render_resources->SetBufferData(CameraShaderData {
        .view               = m_view_mat,
        .projection         = m_proj_mat,
        .previous_view      = m_previous_view_matrix,
        .dimensions         = Vec4u { uint32(MathUtil::Abs(m_width)), uint32(MathUtil::Abs(m_height)), 0, 1 },
        .camera_position    = Vec4f(m_translation, 1.0f),
        .camera_direction   = Vec4f(m_direction, 1.0f),
        .camera_near        = m_near,
        .camera_far         = m_far,
        .camera_fov         = m_fov,
        .id                 = GetID().Value()
    });

    m_render_resources->Claim();

    DeferCreate(m_framebuffer, g_engine->GetGPUDevice());

    SetReady(true);
}

void Camera::AddCameraController(const RC<CameraController> &camera_controller)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertThrow(camera_controller != nullptr);
    AssertThrowMsg(camera_controller->InstanceClass() != NullCameraController::Class(),
        "Cannot add NullCameraController instance");

    AssertThrow(!m_camera_controllers.Contains(camera_controller));

    if (HasActiveCameraController()) {
        if (const RC<CameraController> &current_camera_controller = GetCameraController()) {
            current_camera_controller->OnDeactivated();
        }
    }

    m_camera_controllers.PushBack(camera_controller);

    camera_controller->OnAdded(this);
    camera_controller->OnActivated();

    UpdateMouseLocked();

    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetFramebuffer(const FramebufferRef &framebuffer)
{
    m_framebuffer = framebuffer;
}

void Camera::SetTranslation(const Vec3f &translation)
{
    HYP_SCOPE;

    m_translation = translation;
    m_next_translation = translation;
    
    m_previous_view_matrix = m_view_mat;

    if (HasActiveCameraController()) {
        if (const RC<CameraController> &camera_controller = GetCameraController()) {
            camera_controller->SetTranslation(translation);
        }
    }

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetNextTranslation(const Vec3f &translation)
{
    HYP_SCOPE;

    m_next_translation = translation;

    if (HasActiveCameraController()) {
        if (const RC<CameraController> &camera_controller = GetCameraController()) {
            camera_controller->SetNextTranslation(translation);
        }
    }
}

void Camera::SetDirection(const Vec3f &direction)
{
    HYP_SCOPE;

    m_direction = direction;

    if (HasActiveCameraController()) {
        if (const RC<CameraController> &camera_controller = GetCameraController()) {
            camera_controller->SetDirection(direction);
        }
    }

    // UpdateViewMatrix();
    // UpdateViewProjectionMatrix();
}

void Camera::SetUpVector(const Vec3f &up)
{
    HYP_SCOPE;

    m_up = up;

    if (HasActiveCameraController()) {
        if (const RC<CameraController> &camera_controller = GetCameraController()) {
            camera_controller->SetUpVector(up);
        }
    }

    // UpdateViewMatrix();
    // UpdateViewProjectionMatrix();
}

void Camera::Rotate(const Vec3f &axis, float radians)
{
    HYP_SCOPE;

    m_direction.Rotate(axis, radians);
    m_direction.Normalize();
    
    // m_previous_view_matrix = m_view_mat;

    // UpdateViewMatrix();
    // UpdateViewProjectionMatrix();
}

void Camera::SetViewMatrix(const Matrix4 &view_mat)
{
    HYP_SCOPE;

    m_previous_view_matrix = m_view_mat;
    m_view_mat = view_mat;

    UpdateViewProjectionMatrix();
}

void Camera::SetProjectionMatrix(const Matrix4 &proj_mat)
{
    HYP_SCOPE;

    m_proj_mat = proj_mat;

    UpdateViewProjectionMatrix();
}

void Camera::SetViewProjectionMatrix(const Matrix4 &view_mat, const Matrix4 &proj_mat)
{
    HYP_SCOPE;

    m_previous_view_matrix = m_view_mat;

    m_view_mat = view_mat;
    m_proj_mat = proj_mat;

    UpdateViewProjectionMatrix();
}

void Camera::UpdateViewProjectionMatrix()
{
    HYP_SCOPE;

    m_view_proj_mat = m_proj_mat * m_view_mat;

    m_frustum.SetFromViewProjectionMatrix(m_view_proj_mat);
}

Vec3f Camera::TransformScreenToNDC(const Vec2f &screen) const
{
    // [0, 1] -> [-1, 1]

    return {
        screen.x * 2.0f - 1.0f,//1.0f - (2.0f * screen.x),
        screen.y * 2.0f - 1.0f,//1.0f - (2.0f * screen.y),
        1.0f
    };
}

Vec4f Camera::TransformNDCToWorld(const Vec3f &ndc) const
{
    const Vec4f clip(ndc, 1.0f);

    Vec4f eye = m_proj_mat.Inverted() * clip;
    eye /= eye.w;
    // eye = Vec4f(eye.x, eye.y, -1.0f, 0.0f);

    return m_view_mat.Inverted() * eye;
}

Vec3f Camera::TransformWorldToNDC(const Vec3f &world) const
{
    return m_view_proj_mat * world;
}

Vec2f Camera::TransformWorldToScreen(const Vec3f &world) const
{
    return TransformNDCToScreen(m_view_proj_mat * world);
}

Vec2f Camera::TransformNDCToScreen(const Vec3f &ndc) const
{
    return {
        (0.5f * ndc.x) + 0.5f,
        (0.5f * ndc.y) + 0.5f
    };
}

Vec4f Camera::TransformScreenToWorld(const Vec2f &screen) const
{
    return TransformNDCToWorld(TransformScreenToNDC(screen));
}

Vec2f Camera::GetPixelSize() const
{
    return Vec2f::One() / Vec2f { float(GetWidth()), float(GetHeight()) };
}

void Camera::Update(GameCounter::TickUnit dt)
{
    HYP_SCOPE;

    AssertReady();

    if (HasActiveCameraController()) {
        if (const RC<CameraController> &camera_controller = GetCameraController()) {
            UpdateMouseLocked();

            camera_controller->UpdateCommandQueue(dt);
            camera_controller->UpdateLogic(dt);
        }
    }

    m_translation = m_next_translation;

    UpdateMatrices();

    m_render_resources->SetBufferData(CameraShaderData {
        .view               = m_view_mat,
        .projection         = m_proj_mat,
        .previous_view      = m_previous_view_matrix,
        .dimensions         = Vec4u { uint32(MathUtil::Abs(m_width)), uint32(MathUtil::Abs(m_height)), 0, 1 },
        .camera_position    = Vec4f(m_translation, 1.0f),
        .camera_direction   = Vec4f(m_translation, 1.0f),
        .camera_near        = m_near,
        .camera_far         = m_far,
        .camera_fov         = m_fov,
        .id                 = GetID().Value()
    });
}

void Camera::UpdateViewMatrix()
{
    HYP_SCOPE;

    m_previous_view_matrix = m_view_mat;

    if (HasActiveCameraController()) {
        if (const RC<CameraController> &camera_controller = GetCameraController()) {
            camera_controller->UpdateViewMatrix();
        }
    }
}

void Camera::UpdateProjectionMatrix()
{
    HYP_SCOPE;

    if (HasActiveCameraController()) {
        if (const RC<CameraController> &camera_controller = GetCameraController()) {
            camera_controller->UpdateProjectionMatrix();
        }
    }
}

void Camera::UpdateMatrices()
{
    HYP_SCOPE;

    m_previous_view_matrix = m_view_mat;

    if (HasActiveCameraController()) {
        if (const RC<CameraController> &camera_controller = GetCameraController()) {
            camera_controller->UpdateViewMatrix();
            camera_controller->UpdateProjectionMatrix();
        }
    }

    UpdateViewProjectionMatrix();
}

void Camera::UpdateMouseLocked()
{
    HYP_SCOPE;

    if (!HasActiveCameraController()) {
        return;
    }

    // @TODO MouseLockState tied to the camera so we can tell if the lock belongs to us and release it when we're done
        
    if (const RC<AppContext> &app_context = g_engine->GetAppContext()) {
        if (const RC<CameraController> &camera_controller = GetCameraController()) {
            if (!camera_controller->IsMouseLockAllowed()) {
                return;
            }

            const bool should_lock_mouse = camera_controller->IsMouseLockRequested();

            if (app_context->GetInputManager()->IsMouseLocked() != should_lock_mouse) {
                app_context->GetInputManager()->SetIsMouseLocked(should_lock_mouse);
            }
        }
    }
}

#pragma endregion Camera

} // namespace hyperion
