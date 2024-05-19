/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_OBJECT_HPP
#define HYPERION_UI_OBJECT_HPP

#include <core/Base.hpp>
#include <core/containers/Array.hpp>
#include <core/functional/Delegate.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Scene.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/FullScreenPass.hpp>

#include <Types.hpp>

namespace hyperion {

class UIStage;
class UIRenderer;

// Helper function to get the scene from a UIStage
template <class UIStageType>
static inline Scene *GetScene(UIStageType *stage)
{
    AssertThrow(stage != nullptr);

    return stage->GetScene().Get();
}

#pragma region UIObject

// Used for interop with the C# UIObject class.
// Ensure that the enum values match the C# UIObjectType enum.
enum class UIObjectType : uint32
{
    UNKNOWN             = ~0u,
    STAGE               = 0,
    BUTTON              = 1,
    TEXT                = 2,
    PANEL               = 3,
    IMAGE               = 4,
    TAB_VIEW            = 5,
    TAB                 = 6,
    GRID                = 7,
    GRID_ROW            = 8,
    GRID_COLUMN         = 9,
    MENU_BAR            = 10,
    MENU_ITEM           = 11,
    DOCKABLE_CONTAINER  = 12,
    DOCKABLE_ITEM       = 13
};

HYP_MAKE_ENUM_FLAGS(UIObjectType)

enum class UIObjectAlignment : uint32
{
    TOP_LEFT        = 0,
    TOP_RIGHT       = 1,

    CENTER          = 2,

    BOTTOM_LEFT     = 3,
    BOTTOM_RIGHT    = 4
};

enum class UIObjectFocusState : uint32
{
    NONE    = 0x0,
    HOVER   = 0x1,
    PRESSED = 0x2,
    TOGGLED = 0x4,
    FOCUSED = 0x8
};

HYP_MAKE_ENUM_FLAGS(UIObjectFocusState)

enum class UIObjectBorderFlags : uint32
{
    NONE    = 0x0,
    TOP     = 0x1,
    LEFT    = 0x2,
    BOTTOM  = 0x4,
    RIGHT   = 0x8,
    ALL     = TOP | LEFT | BOTTOM | RIGHT
};

HYP_MAKE_ENUM_FLAGS(UIObjectBorderFlags)

struct UIObjectSize
{
    using Flags = uint32;

    enum FlagBits : Flags
    {
        AUTO            = 0x04,

        PIXEL           = 0x10,
        PERCENT         = 0x20,

        DEFAULT         = PIXEL
    };

    UIObjectSize()
        : flags { DEFAULT, DEFAULT },
          value(0, 0)
    {
    }

    UIObjectSize(Vec2i value)
        : flags { DEFAULT, DEFAULT },
          value(value)
    {
    }

    UIObjectSize(Vec2i value, Flags flags)
        : flags { flags, flags },
          value(value)
    {
        ApplyDefaultFlags();
    }

    /*! \brief Construct by only providing flags. Used primarily for the DYNAMIC type. */
    UIObjectSize(Flags flags)
        : flags { flags, flags },
          value(0, 0)
    {
        ApplyDefaultFlags();
    }

    /*! \brief Construct by providing specific flags for each axis. */
    UIObjectSize(Pair<int, Flags> x, Pair<int, Flags> y)
        : flags { x.second, y.second },
          value(x.first, y.first)
    {
        ApplyDefaultFlags();
    }

    UIObjectSize(const UIObjectSize &other)                 = default;
    UIObjectSize &operator=(const UIObjectSize &other)      = default;
    UIObjectSize(UIObjectSize &&other) noexcept             = default;
    UIObjectSize &operator=(UIObjectSize &&other) noexcept  = default;
    ~UIObjectSize()                                         = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Vec2i &GetValue() const
        { return value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Flags GetFlagsX() const
        { return flags[0]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Flags GetFlagsY() const
        { return flags[1]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Flags GetAllFlags() const
        { return flags[0] | flags[1]; }

private:
    Flags   flags[2];
    Vec2i   value;

    template <Flags mask>
    HYP_FORCE_INLINE
    void ApplyDefaultFlagMask()
    { 
        for (int i = 0; i < 2; i++) {
            if (!(flags[i] & mask)) {
                flags[i] |= (DEFAULT & mask);
            }
        }
    }

    HYP_FORCE_INLINE
    void ApplyDefaultFlags()
    {
        ApplyDefaultFlagMask<PIXEL | PERCENT>();
    }
};

struct UIObjectID : UniqueID { };

enum class UIObjectUpdateSizeFlags : uint32
{
    NONE                = 0x0,

    MAX_SIZE            = 0x1,
    INNER_SIZE          = 0x2,
    OUTER_SIZE          = 0x4,

    CLAMP_OUTER_SIZE    = 0x8,

    DEFAULT             = MAX_SIZE | INNER_SIZE | OUTER_SIZE | CLAMP_OUTER_SIZE
};

HYP_MAKE_ENUM_FLAGS(UIObjectUpdateSizeFlags)

class HYP_API UIObject : public EnableRefCountedPtrFromThis<UIObject>
{
protected:
    enum class UpdateSizePhase
    {
        BEFORE_CHILDREN,
        AFTER_CHILDREN
    };

    UIObject(UIObjectType type);

public:
    friend class UIRenderer;
    friend class UIStage;

    UIObject(UIStage *stage, NodeProxy node_proxy, UIObjectType type);
    UIObject(const UIObject &other)                 = delete;
    UIObject &operator=(const UIObject &other)      = delete;
    UIObject(UIObject &&other) noexcept             = delete;
    UIObject &operator=(UIObject &&other) noexcept  = delete;
    virtual ~UIObject();

    virtual void Init();
    virtual void Update(GameCounter::TickUnit delta) final;

    [[nodiscard]]
    HYP_FORCE_INLINE
    const UIObjectID &GetID() const
        { return m_id; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    UIObjectType GetType() const
        { return m_type; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    ID<Entity> GetEntity() const
        { return m_node_proxy.IsValid() ? m_node_proxy->GetEntity() : ID<Entity>::invalid; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    UIStage *GetStage() const
        { return m_stage; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsInit() const
        { return m_is_init; }

    Name GetName() const;
    void SetName(Name name);

    Vec2i GetPosition() const;
    void SetPosition(Vec2i position);

    Vec2f GetOffsetPosition() const;
    Vec2f GetAbsolutePosition() const;

    UIObjectSize GetSize() const;
    void SetSize(UIObjectSize size);

    UIObjectSize GetInnerSize() const;
    void SetInnerSize(UIObjectSize size);

    UIObjectSize GetMaxSize() const;
    void SetMaxSize(UIObjectSize size);
    
    /*! \brief Get the computed size (in pixels) of the UI object.
     *  The actual size of the UI object is calculated based on the size of the parent object and the size of the object itself.
     *  \return The computed size of the UI object */
    [[nodiscard]]
    HYP_FORCE_INLINE
    Vec2i GetActualSize() const
        { return m_actual_size; }

    /*! \brief Get the computed inner size (in pixels) of the UI object.
     *  \return The computed inner size of the UI object */
    [[nodiscard]]
    HYP_FORCE_INLINE
    Vec2i GetActualInnerSize() const
        { return m_actual_inner_size; }

    /*! \brief Get the scroll offset (in pixels) of the UI object.
     *  \return The scroll offset of the UI object */
    [[nodiscard]]
    Vec2i GetScrollOffset() const;

    /*! \brief Set the scroll offset (in pixels) of the UI object.
     *  \param scroll_offset The scroll offset of the UI object */
    void SetScrollOffset(Vec2i scroll_offset);

    /*! \brief Get the depth of the UI object, or the computed depth from the Node  if none has been explicitly set.
     *  \see{Node::CalculateDepth}
     *  \return The depth of the UI object */
    int GetComputedDepth() const;

    /*! \brief Get the depth of the UI object
     *  The depth of the UI object is used to determine the rendering order of the object in the scene relative to its sibling elements, with higher depth values being rendered on top of lower depth values.
     *  If the depth value is set to 0, the depth will be determined by the node's depth in the scene.
     *  \return The depth of the UI object */
    int GetDepth() const;

    /*! \brief Set the depth of the UI object
     *  The depth of the UI object is used to determine the rendering order of the object in the scene relative to its sibling elements, with higher depth values being rendered on top of lower depth values.
     *  Set the depth to a value between UIStage::min_depth and UIStage::max_depth. If the depth value is set to 0, the depth will be determined by the node's depth in the scene.
     *  \param depth The depth of the UI object
     */
    void SetDepth(int depth);

    /*! \brief Check if the UI object accepts focus. All UIObjects accept focus by default, unless overridden by derived classes or set using \ref{SetAcceptsFocus}.
     *  \return True if the this object accepts focus, false otherwise */
    [[nodiscard]]
    virtual bool AcceptsFocus() const
        { return m_accepts_focus; }

    /*! \brief Set whether the UI object accepts focus.
     *  \details If set to true, the UI object can receive focus. If set to false, the UI object cannot receive focus.
     *  \note If a class deriving \ref{UIObject} overrides \ref{AcceptsFocus}, this function has no effect. */
    void SetAcceptsFocus(bool accepts_focus);

    /*! \brief Set the focus to this UI object, if AcceptsFocus() returns true.
     * This function is called when the UI object is focused. */
    virtual void Focus();

    /*! \brief Remove the focus from this UI object, if AcceptsFocus() returns true.
     *  This function is called when the UI object loses focus.
     *  \param blur_children If true, also remove focus from all child objects. */
    virtual void Blur(bool blur_children = true);

    /*! \brief Get the border radius of the UI object
     *  \details The border radius of the UI object is used to create rounded corners for the object's border.
     *  \return The border radius of the UI object */
    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 GetBorderRadius() const
        { return m_border_radius; }

    /*! \brief Set the border radius of the UI object
     *  \details The border radius of the UI object is used to create rounded corners for the object's border.
     *  \param border_radius The border radius of the UI object */
    void SetBorderRadius(uint32 border_radius);

    /*! \brief Get the border flags of the UI object
     *  \details The border flags of the UI object are used to determine which borders of the object should be rounded, if the border radius is set to a non-zero value.
     *  \example To display a border radius the top left and right corners of the object, set the border flags to \code{UOB_TOP | UOB_LEFT | UOB_RIGHT}.
     *  \return The border flags of the UI object */
    [[nodiscard]]
    HYP_FORCE_INLINE
    EnumFlags<UIObjectBorderFlags> GetBorderFlags() const
        { return m_border_flags; }

    void SetBorderFlags(EnumFlags<UIObjectBorderFlags> border_flags);

    UIObjectAlignment GetOriginAlignment() const;
    void SetOriginAlignment(UIObjectAlignment alignment);

    UIObjectAlignment GetParentAlignment() const;
    void SetParentAlignment(UIObjectAlignment alignment);

    /*! \brief Get the padding of the UI object
     * The padding of the UI object is used to add space around the object's content.
     * \return The padding of the UI object */
    [[nodiscard]]
    HYP_FORCE_INLINE
    Vec2i GetPadding() const
        { return m_padding; }

    /*! \brief Set the padding of the UI object
     * The padding of the UI object is used to add space around the object's content.
     * \param padding The padding of the UI object */
    void SetPadding(Vec2i padding);

    /*! \brief Check if the UI object is visible.
     * \return True if the object is visible, false otherwise. */
    [[nodiscard]]
    bool IsVisible() const;

    /*! \brief Set the visibility of the UI object.
     *  \details The visibility of the UI object determines whether the object is rendered in the UI scene.
     *  Can be used to hide the object without removing it from the scene.
     *  \param is_visible Whether to set the object as visible or not. */
    void SetIsVisible(bool is_visible);

    /*! \brief Check if the UI object has focus. If \ref{include_children} is true, also return true if any child objects have focus.
     *  \details The focus state of the UI object is used to determine if the object is currently focused.
     *  \param include_children If true, check if any child objects have focus.
     *  \return True if the object has focus, false otherwise. */
    [[nodiscard]]
    bool HasFocus(bool include_children = true) const;

    /*! \brief Check if \ref{other} is either a parent of this object or is equal to the current object.
     *  \details Comparison is performed by using \ref{Node::IsOrHasParent}. If either this or \ref{other} does not have a Node,
     *  false is returned.
     *  \param other The UIObject to check if it is a parent of this object.
     *  \return Whether \ref{other} is a parent of this object or equal to the current object.
     */
    [[nodiscard]]
    bool IsOrHasParent(const UIObject *other) const;

    /*! \brief Get the parent UIObject to this object, if one exists.
     *  \returns A pointer to the parent UIObject or nullptr if none exists. */
    [[nodiscard]]
    RC<UIObject> GetParentUIObject() const;

    virtual void AddChildUIObject(UIObject *ui_object);
    virtual bool RemoveChildUIObject(UIObject *ui_object);

    /*! \brief Find a child UIObject by its Name. Checks descendents recursively. If multiple children have the same Name, the first one found is returned.
     *  If no child UIObject with the specified Name is found, nullptr is returned.
     *  \param name The Name of the child UIObject to find.
     *  \return The child UIObject with the specified Name, or nullptr if no child UIObject with the specified Name was found. */
    RC<UIObject> FindChildUIObject(Name name) const;

    bool HasChildUIObjects() const;

    const NodeProxy &GetNode() const;

    Scene *GetScene() const;

    BoundingBox GetLocalAABB() const;
    BoundingBox GetWorldAABB() const;

    virtual void UpdatePosition(bool update_children = true);
    virtual void UpdateSize(bool update_children = true);

    /*! \brief Get the focus state of the UI object.
     *  \details The focus state of the UI object is used to determine if the object is currently focused, hovered, pressed, etc.
     *  \return The focus state of the UI object. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    EnumFlags<UIObjectFocusState> GetFocusState() const
        { return m_focus_state; }

    /*! \brief Set the focus state of the UI object.
     *  \details The focus state of the UI object is used to determine if the object is currently focused, hovered, pressed, etc.
     *  \param focus_state The focus state of the UI object. */
    void SetFocusState(EnumFlags<UIObjectFocusState> focus_state);

    /*! \brief Collect all nested UIObjects in the hierarchy, calling `proc` for each collected UIObject.
     *  \param proc The function to call for each collected UIObject. */
    void CollectObjects(const Proc<void, const RC<UIObject> &> &proc) const;

    /*! \brief Collect all nested UIObjects in the hierarchy and push them to the `out_objects` array.
     *  \param out_objects The array to store the collected UIObjects in. */
    void CollectObjects(Array<RC<UIObject>> &out_objects) const;

    /*! \brief Transform a screen coordinate to a relative coordinate within the UIObject.
     *  \param coords The screen coordinates to transform.
     *  \return The relative coordinates within the UIObject. */
    Vec2f TransformScreenCoordsToRelative(Vec2i coords) const;

    // Events
    Delegate<UIEventHandlerResult, const UIMouseEventData &>    OnMouseDown;
    Delegate<UIEventHandlerResult, const UIMouseEventData &>    OnMouseUp;
    Delegate<UIEventHandlerResult, const UIMouseEventData &>    OnMouseDrag;
    Delegate<UIEventHandlerResult, const UIMouseEventData &>    OnMouseHover;
    Delegate<UIEventHandlerResult, const UIMouseEventData &>    OnMouseLeave;
    Delegate<UIEventHandlerResult, const UIMouseEventData &>    OnMouseMove;
    Delegate<UIEventHandlerResult, const UIMouseEventData &>    OnGainFocus;
    Delegate<UIEventHandlerResult, const UIMouseEventData &>    OnLoseFocus;
    Delegate<UIEventHandlerResult, const UIMouseEventData &>    OnScroll;
    Delegate<UIEventHandlerResult, const UIMouseEventData &>    OnClick;
    Delegate<UIEventHandlerResult, const UIKeyEventData &>      OnKeyDown;
    Delegate<UIEventHandlerResult, const UIKeyEventData &>      OnKeyUp;

protected:
    virtual void Update_Internal(GameCounter::TickUnit delta);

    virtual void OnAttached_Internal(UIObject *parent);
    virtual void OnDetached_Internal();

    /*! \brief Sets the NodeProxy for this UIObject.
     *  \note To be called internally from UIStage */
    void SetNodeProxy(NodeProxy);

    /*! \brief Get the shared quad mesh used for rendering UI objects. Vertices are in range: 0..1, with the origin at the top-left corner.
     *
     * \return The shared quad mesh
     */
    static Handle<Mesh> GetQuadMesh();

    /*! \brief Calculate the local (object space) bounding box (in pixels) of this object.
     *  should be in range of (0,0,0):(size,size,size). */
    virtual BoundingBox CalculateAABB() const;

    /*! \brief Override to have the UIObject use a different material. */
    virtual Handle<Material> GetMaterial() const;

    const Handle<Mesh> &GetMesh() const;

    void SetAABB(const BoundingBox &aabb);

    void UpdateMeshData();
    void UpdateMaterial(bool update_children = true);

    UIStage                         *m_stage;

    Name                            m_name;

    Vec2i                           m_position;
    Vec2f                           m_offset_position;

    UIObjectSize                    m_size;
    Vec2i                           m_actual_size;

    UIObjectSize                    m_inner_size;
    Vec2i                           m_actual_inner_size;

    UIObjectSize                    m_max_size;
    Vec2i                           m_actual_max_size;

    Vec2i                           m_scroll_offset;

    Vec2i                           m_padding;

    int                             m_depth; // manually set depth; otherwise defaults to the node's depth in the scene

    uint32                          m_border_radius;
    EnumFlags<UIObjectBorderFlags>  m_border_flags;

    UIObjectAlignment               m_origin_alignment;
    UIObjectAlignment               m_parent_alignment;

private:
    void ComputeOffsetPosition();

    void UpdateActualSizes(UpdateSizePhase phase, EnumFlags<UIObjectUpdateSizeFlags> flags);
    void ComputeActualSize(const UIObjectSize &size, Vec2i &out_actual_size, UpdateSizePhase phase, bool is_inner = false);

    template <class Lambda>
    void ForEachChildUIObject(Lambda &&lambda) const;

    // For UIStage only.
    void SetAllChildUIObjectsStage(UIStage *stage);

    const UIObjectID                m_id;
    const UIObjectType              m_type;

    bool                            m_is_init;

    EnumFlags<UIObjectFocusState>   m_focus_state;

    bool                            m_is_visible;
    
    bool                            m_accepts_focus;

    NodeProxy                       m_node_proxy;
};

#pragma endregion UIObject

} // namespace hyperion

#endif