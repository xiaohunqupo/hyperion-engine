#ifndef HYPERION_V2_ECS_SYSTEM_HPP
#define HYPERION_V2_ECS_SYSTEM_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/TypeID.hpp>
#include <scene/ecs/EntitySet.hpp>
#include <scene/ecs/ComponentContainer.hpp>
#include <GameCounter.hpp>

#include <tuple>

namespace hyperion::v2 {

class EntityManager;

class SystemBase
{
public:
    virtual ~SystemBase() = default;

    bool IsEntityInitialized(ID<Entity> entity) const
        { return m_initialized_entities.Contains(entity); }

    virtual bool AllowParallelExecution() const
        { return true; }

    /*! \brief Returns the TypeIDs of the components this System operates on.
     *  To be used by the EntityManager in order to properly order the Systems based on their dependencies.
     *
     *  \return The TypeIDs of the components this System operates on.
     */
    const Array<TypeID> &GetComponentTypeIDs() const
        { return m_component_type_ids; }

    /*! \brief Returns true if all given TypeIDs are operated on by this System, false otherwise.
     *
     *  \param component_type_ids The TypeIDs of the components to check.
     *  \param receive_events_context If true, this function will skip components that do not receive events for this System.
     *
     *  \return True if all given TypeIDs are operated on by this System, false otherwise.
     */
    bool ActsOnComponents(const Array<TypeID> &component_type_ids, bool receive_events_context) const
    {
        for (const TypeID component_type_id : m_component_type_ids) {
            const ComponentInfo &component_info = GetComponentInfo(component_type_id);

            // skip observe-only components
            if (!(component_info.rw_flags & COMPONENT_RW_FLAGS_READ) && !(component_info.rw_flags & COMPONENT_RW_FLAGS_WRITE)) {
                continue;
            }

            if (receive_events_context && !component_info.receives_events) {
                continue;
            }
            
            if (!component_type_ids.Contains(component_type_id)) {
                return false;
            }
        }

        return true;
    }


    /*! \brief Returns true if this System operates on the component with the given TypeID, false otherwise.
     *
     *  \param component_type_id The TypeID of the component to check.
     *  \param include_read_only If true, this function will return true even if the component is read-only.
     *  Otherwise, read-only components will be ignored.
     *
     *  \return True if this System operates on the component with the given TypeID, false otherwise.
     */
    bool HasComponentTypeID(TypeID component_type_id, bool include_read_only = true) const
    {
        const bool has_component_type_id = m_component_type_ids.Contains(component_type_id);

        if (!has_component_type_id) {
            return false;
        }

        if (include_read_only) {
            return true;
        }

        const ComponentInfo &component_info = GetComponentInfo(component_type_id);

        return component_info.rw_flags & COMPONENT_RW_FLAGS_WRITE;
    }

    /*! \brief Returns the ComponentInfo of the component with the given TypeID.
     *
     *  \param component_type_id The TypeID of the component to check.
     *
     *  \return The ComponentInfo of the component with the given TypeID.
     */
    const ComponentInfo &GetComponentInfo(TypeID component_type_id) const
    {
        const auto it = m_component_type_ids.Find(component_type_id);
        AssertThrowMsg(it != m_component_type_ids.End(), "Component type ID not found");

        const SizeType index = m_component_type_ids.IndexOf(it);
        AssertThrowMsg(index != SizeType(-1), "Component type ID not found");

        return m_component_infos[index];
    }

    virtual void OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity_id)
    {
        m_initialized_entities.Insert(entity_id);
    }

    virtual void OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity_id)
    {
        m_initialized_entities.Erase(entity_id);
    }

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) = 0;

protected:
    SystemBase(Array<TypeID> &&component_type_ids, Array<ComponentInfo> &&component_infos)
        : m_component_type_ids(std::move(component_type_ids)),
          m_component_infos(std::move(component_infos))
    {
        AssertThrowMsg(m_component_type_ids.Size() == m_component_infos.Size(), "Component type ID count and component infos count mismatch");
    }

    Array<TypeID>           m_component_type_ids;
    Array<ComponentInfo>    m_component_infos;

private:
    FlatSet<ID<Entity>>     m_initialized_entities;
};

/*! \brief A System is a class that operates on a set of components.
 *
 *  \tparam ComponentDescriptors ComponentDescriptor structs for the Components this System operates on.
 */
template <class ... ComponentDescriptors>
class System : public SystemBase
{
public:
    using ComponentDescriptorTypes = std::tuple<ComponentDescriptors...>;

    System()
        : SystemBase({ TypeID::ForType<typename ComponentDescriptors::Type>()... }, { ComponentInfo(ComponentDescriptors())... })
    {
    }

    System(const System &other)                 = delete;
    System &operator=(const System &other)      = delete;
    System(System &&other) noexcept             = delete;
    System &operator=(System &&other) noexcept  = delete;
    virtual ~System() override                  = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override = 0;
};

} // namespace hyperion::v2

#endif
