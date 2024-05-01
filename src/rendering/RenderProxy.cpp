#include <rendering/RenderProxy.hpp>

namespace hyperion {

void RenderProxyList::Add(ID<Entity> entity, RenderProxy proxy)
{
    m_proxies.Set(entity, std::move(proxy));

    m_next_entities.Set(entity.ToIndex(), true);
    m_changed_entities.Set(entity.ToIndex(), true);

    if (m_next_entities.NumBits() > m_previous_entities.NumBits()) {
        m_previous_entities.Resize(m_next_entities.NumBits());
    }
}

bool RenderProxyList::MarkToKeep(ID<Entity> entity)
{
    if (!m_previous_entities.Test(entity.ToIndex())) {
        return false;
    }

    m_next_entities.Set(entity.ToIndex(), true);

    return true;
}

void RenderProxyList::MarkToRemove(ID<Entity> entity)
{
    m_next_entities.Set(entity.ToIndex(), false);
}

void RenderProxyList::GetRemovedEntities(Array<ID<Entity>> &out_entities)
{
    Bitset removed_bits = GetRemovedEntities();

    out_entities.Reserve(removed_bits.Count());

    SizeType first_set_bit_index;

    while ((first_set_bit_index = removed_bits.FirstSetBitIndex()) != -1) {
        out_entities.PushBack(ID<Entity>::FromIndex(first_set_bit_index));

        removed_bits.Set(first_set_bit_index, false);
    }
}

void RenderProxyList::GetAddedEntities(Array<RenderProxy *> &out_entities, bool include_changed)
{
    Bitset newly_added_bits = GetAddedEntities();

    if (include_changed) {
        newly_added_bits |= (GetChangedEntities());
    }

    out_entities.Reserve(newly_added_bits.Count());

    SizeType first_set_bit_index;

    while ((first_set_bit_index = newly_added_bits.FirstSetBitIndex()) != -1) {
        const ID<Entity> id = ID<Entity>::FromIndex(first_set_bit_index);

        auto it = m_proxies.Find(id);
        AssertThrow(it != m_proxies.End());

        out_entities.PushBack(&it->second);

        newly_added_bits.Set(first_set_bit_index, false);
    }
}

RenderProxy *RenderProxyList::GetProxyForEntity(ID<Entity> entity)
{
    const auto it = m_proxies.Find(entity);

    if (it == m_proxies.End()) {
        return nullptr;
    }

    return &it->second;
}

void RenderProxyList::Advance(RenderProxyListAdvanceAction action)
{
    { // Remove proxies for removed bits
        Bitset removed_bits = GetRemovedEntities();

        SizeType first_set_bit_index;

        while ((first_set_bit_index = removed_bits.FirstSetBitIndex()) != -1) {
            const ID<Entity> entity = ID<Entity>::FromIndex(first_set_bit_index);

            const auto it = m_proxies.Find(entity);

            if (it != m_proxies.End()) {
                m_proxies.Erase(it);
            }

            removed_bits.Set(first_set_bit_index, false);
        }
    }

    switch (action) {
    case RPLAA_CLEAR:
        // Next state starts out zeroed out -- and next call to Advance will remove proxies for these objs
        m_previous_entities = std::move(m_next_entities);

        break;
    case RPLAA_PERSIST:
        m_previous_entities = m_next_entities;

        break;
    }

    m_changed_entities.Clear();
}

} // namespace hyperion