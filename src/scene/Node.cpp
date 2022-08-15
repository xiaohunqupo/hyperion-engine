#include "Node.hpp"
#include <system/Debug.hpp>

#include <animation/Bone.hpp>
#include <Engine.hpp>

#include <cstring>

namespace hyperion::v2 {

Node::Node(
    const char *name,
    const Transform &local_transform
) : Node(
        name,
        nullptr,
        local_transform
    )
{
}

Node::Node(
    const char *name,
    Ref<Entity> &&entity,
    const Transform &local_transform
) : Node(
        Type::NODE,
        name,
        std::move(entity),
        local_transform
    )
{
}

Node::Node(
    Type type,
    const char *name,
    Ref<Entity> &&entity,
    const Transform &local_transform
) : m_type(type),
    m_parent_node(nullptr),
    m_local_transform(local_transform),
    m_scene(nullptr)
{
    SetEntity(std::move(entity));

    const size_t len = std::strlen(name);
    m_name = new char[len + 1];
    Memory::CopyString(m_name, name);
}

Node::Node(Node &&other) noexcept
    : m_type(other.m_type),
      m_parent_node(other.m_parent_node),
      m_local_transform(other.m_local_transform),
      m_world_transform(other.m_world_transform),
      m_local_aabb(other.m_local_aabb),
      m_world_aabb(other.m_world_aabb),
      m_scene(other.m_scene)
{
    other.m_type = Type::NODE;
    other.m_parent_node = nullptr;
    other.m_local_transform = Transform::identity;
    other.m_world_transform = Transform::identity;
    other.m_local_aabb = BoundingBox::empty;
    other.m_world_aabb = BoundingBox::empty;
    other.m_scene = nullptr;

    m_entity = std::move(other.m_entity);
    m_entity->SetParent(this);
    other.m_entity = nullptr;

    m_name = other.m_name;
    other.m_name = nullptr;

    m_child_nodes = std::move(other.m_child_nodes);
    other.m_child_nodes.clear();

    m_descendents = std::move(other.m_descendents);
    other.m_descendents.clear();

    for (auto &node : m_child_nodes) {
        AssertThrow(node.Get() != nullptr);

        node.Get()->m_parent_node = this;
    }
}

Node &Node::operator=(Node &&other) noexcept
{
    RemoveAllChildren();

    SetEntity(nullptr);
    SetScene(nullptr);

    m_type = other.m_type;
    other.m_type = Type::NODE;

    m_parent_node = other.m_parent_node;
    other.m_parent_node = nullptr;

    m_local_transform = other.m_local_transform;
    other.m_local_transform = Transform::identity;

    m_world_transform = other.m_world_transform;
    other.m_world_transform = Transform::identity;

    m_local_aabb = other.m_local_aabb;
    other.m_local_aabb = BoundingBox::empty;

    m_world_aabb = other.m_world_aabb;
    other.m_world_aabb = BoundingBox::empty;

    m_scene = other.m_scene;
    other.m_scene = nullptr;

    m_entity = std::move(other.m_entity);
    m_entity->SetParent(this);
    other.m_entity = nullptr;

    m_name = other.m_name;
    other.m_name = nullptr;

    m_child_nodes = std::move(other.m_child_nodes);
    other.m_child_nodes.clear();

    m_descendents = std::move(other.m_descendents);
    other.m_descendents.clear();

    for (auto &node : m_child_nodes) {
        AssertThrow(node.Get() != nullptr);

        node.Get()->m_parent_node = this;
    }

    return *this;
}

Node::~Node()
{
    AssertThrow(m_ref_count.count == 0);

    SetEntity(nullptr);

    delete[] m_name;
}

void Node::SetName(const char *name)
{
    if (name == m_name || !std::strcmp(name, m_name)) {
        return;
    }

    delete[] m_name;

    const size_t len = std::strlen(name);
    m_name = new char[len + 1];
    Memory::CopyString(m_name, name);
}

void Node::SetScene(Scene *scene)
{
    if (m_scene != nullptr && m_entity != nullptr) {
        m_scene->RemoveEntity(m_entity);
    }

    m_scene = scene;

    if (m_scene != nullptr && m_entity != nullptr) {
        m_scene->AddEntity(m_entity.IncRef());
    }

    for (auto &child : m_child_nodes) {
        if (!child) {
            continue;
        }

        child.Get()->SetScene(scene);
    }
}

void Node::OnNestedNodeAdded(const NodeProxy &node)
{
    m_descendents.push_back(node);
    
    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeAdded(node);
    }
}

void Node::OnNestedNodeRemoved(const NodeProxy &node)
{
    const auto it = std::find(m_descendents.begin(), m_descendents.end(), node);

    if (it != m_descendents.end()) {
        m_descendents.erase(it);
    }

    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeRemoved(node);
    }
}

NodeProxy Node::AddChild()
{
    return AddChild(NodeProxy(new Node()));
}

NodeProxy Node::AddChild(const NodeProxy &node)
{
    AssertThrow(node.Get() != nullptr);
    AssertThrow(node.Get()->m_parent_node == nullptr);

    m_child_nodes.push_back(node);

    m_child_nodes.back().Get()->m_parent_node = this;
    m_child_nodes.back().Get()->SetScene(m_scene);

    OnNestedNodeAdded(m_child_nodes.back());

    for (auto &nested : m_child_nodes.back().Get()->GetDescendents()) {
        OnNestedNodeAdded(nested);
    }

    m_child_nodes.back().Get()->UpdateWorldTransform();

    return m_child_nodes.back();
}

bool Node::RemoveChild(NodeList::iterator iter)
{
    if (iter == m_child_nodes.end()) {
        return false;
    }

    if (auto &node = *iter) {
        AssertThrow(node.Get() != nullptr);
        AssertThrow(node.Get()->m_parent_node == this);

        for (auto &nested : node.Get()->GetDescendents()) {
            OnNestedNodeRemoved(nested);
        }

        OnNestedNodeRemoved(node);

        node.Get()->m_parent_node = nullptr;
        node.Get()->SetScene(nullptr);
    }

    m_child_nodes.erase(iter);

    UpdateWorldTransform();

    return true;
}

bool Node::RemoveChild(size_t index)
{
    if (index >= m_child_nodes.size()) {
        return false;
    }

    return RemoveChild(m_child_nodes.begin() + index);
}

bool Node::Remove()
{
    if (m_parent_node == nullptr) {
        return false;
    }

    return m_parent_node->RemoveChild(m_parent_node->FindChild(this));
}

void Node::RemoveAllChildren()
{
    for (auto it = m_child_nodes.begin(); it != m_child_nodes.end();) {
        if (auto &node = *it) {
            AssertThrow(node.Get() != nullptr);
            AssertThrow(node.Get()->m_parent_node == this);

            for (auto &nested : node.Get()->GetDescendents()) {
                OnNestedNodeRemoved(nested);
            }

            OnNestedNodeRemoved(node);

            node.Get()->m_parent_node = nullptr;
            node.Get()->SetScene(nullptr);
        }

        it = m_child_nodes.erase(it);
    }

    UpdateWorldTransform();
}

NodeProxy Node::GetChild(size_t index) const
{
    if (index >= m_child_nodes.size()) {
        return NodeProxy();
    }

    return NodeProxy(m_child_nodes[index]);
}

NodeProxy Node::Select(const char *selector)
{
    if (selector == nullptr) {
        return NodeProxy();
    }

    char ch;

    char buffer[256];
    UInt32 buffer_index = 0;
    UInt32 selector_index = 0;

    Node *search_node = this;

    while ((ch = selector[selector_index]) != '\0') {
        const char prev_selector_char = selector_index == 0
            ? '\0'
            : selector[selector_index - 1];

        ++selector_index;

        if (ch == '/' && prev_selector_char != '\\') {
            const auto it = search_node->FindChild(buffer);

            if (it == search_node->GetChildren().end()) {
                return NodeProxy();
            }

            search_node = it->Get();

            if (!search_node) {
                return NodeProxy();
            }

            buffer_index = 0;
        } else if (ch != '\\') {
            buffer[buffer_index] = ch;

            ++buffer_index;

            if (buffer_index == std::size(buffer)) {
                DebugLog(
                    LogType::Warn,
                    "Node search string too long, must be within buffer size limit of %llu\n",
                    std::size(buffer)
                );

                return NodeProxy();
            }
        }

        buffer[buffer_index] = '\0';
    }

    // find remaining
    if (buffer_index != 0) {
        const auto it = search_node->FindChild(buffer);

        if (it == search_node->GetChildren().end()) {
            return NodeProxy();
        }

        search_node = it->Get();
    }

    return NodeProxy(search_node);
}

Node::NodeList::iterator Node::FindChild(Node *node)
{
    return std::find_if(
        m_child_nodes.begin(),
        m_child_nodes.end(),
        [node](const auto &it) {
            return it.Get() == node;
        }
    );
}

Node::NodeList::iterator Node::FindChild(const char *name)
{
    return std::find_if(
        m_child_nodes.begin(),
        m_child_nodes.end(),
        [name](const auto &it) {
            return !std::strcmp(name, it.Get()->GetName());
        }
    );
}

void Node::SetLocalTransform(const Transform &transform)
{
    m_local_transform = transform;
    
    UpdateWorldTransform();
}

void Node::SetEntity(Ref<Entity> &&entity)
{
    if (m_entity == entity) {
        return;
    }

    if (m_entity != nullptr) {
        if (m_scene != nullptr) {
            m_scene->RemoveEntity(m_entity);
        }

        m_entity->SetParent(nullptr);
    }

    if (entity != nullptr) {
        m_entity = std::move(entity);

        if (m_scene != nullptr) {
            m_scene->AddEntity(m_entity.IncRef());
        }

        m_entity->SetParent(this);
        m_entity.Init();

        m_local_aabb = m_entity->GetLocalAabb();
    } else {
        m_local_aabb = BoundingBox();

        m_entity = nullptr;
    }

    UpdateWorldTransform();
}

void Node::UpdateWorldTransform()
{
    if (m_type == Type::BONE) {
        static_cast<Bone *>(this)->UpdateBoneTransform();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

    if (m_parent_node != nullptr) {
        m_world_transform = m_parent_node->GetWorldTransform() * m_local_transform;
    } else {
        m_world_transform = m_local_transform;
    }

    m_world_aabb = m_local_aabb * m_world_transform;

    for (auto &node : m_child_nodes) {
        AssertThrow(node.Get() != nullptr);
        node.Get()->UpdateWorldTransform();

        m_world_aabb.Extend(node.Get()->GetWorldAabb());
    }

    if (m_parent_node != nullptr) {
        m_parent_node->m_world_aabb.Extend(m_world_aabb);
    }

    if (m_entity != nullptr) {
        m_entity->SetTransform(m_world_transform);
    }
}

bool Node::TestRay(const Ray &ray, RayTestResults &out_results) const
{
    const bool has_node_hit = ray.TestAabb(m_world_aabb);

    bool has_entity_hit = false;

    if (has_node_hit) {
        if (m_entity != nullptr) {
            has_entity_hit = ray.TestAabb(
                m_entity->GetWorldAabb(),
                m_entity->GetId().value,
                m_entity.ptr,
                out_results
            );
        }

        for (auto &child_node : m_child_nodes) {
            if (!child_node) {
                continue;
            }

            if (child_node.Get()->TestRay(ray, out_results)) {
                has_entity_hit = true;
            }
        }
    }

    return has_entity_hit;
}

} // namespace hyperion::v2
