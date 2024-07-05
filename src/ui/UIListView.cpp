/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIListView.hpp>
#include <ui/UIText.hpp>

#include <input/InputManager.hpp>

#include <core/utilities/Format.hpp>
#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

#pragma region UIListViewItem

UIListViewItem::UIListViewItem(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::LIST_VIEW_ITEM),
      m_data_source_element_uuid(UUID::Invalid())
{
}

void UIListViewItem::Init()
{
    UIPanel::Init();
}

#pragma endregion UIListViewItem

#pragma region UIListView

UIListView::UIListView(UIStage *parent, NodeProxy node_proxy)
    : UIPanel(parent, std::move(node_proxy), UIObjectType::LIST_VIEW)
{
}

void UIListView::SetDataSource(UniquePtr<UIDataSourceBase> &&data_source)
{
    HYP_SCOPE;

    if (m_data_source) {
        m_data_source_on_change_handler.Reset();
        m_data_source_on_element_add_handler.Reset();
        m_data_source_on_element_remove_handler.Reset();
        m_data_source_on_element_update_handler.Reset();
    }

    m_data_source = std::move(data_source);

    if (!m_data_source) {
        return;
    }

    // @TODO initial setup of list view items!
    m_data_source_on_element_add_handler = m_data_source->OnElementAdd.Bind([this](UIDataSourceBase *data_source_ptr, UUID uuid, ConstAnyRef ref)
    {
        HYP_NAMED_SCOPE("Add element from data source to list view");

        RC<UIListViewItem> list_view_item = GetStage()->CreateUIObject<UIListViewItem>(Name::Unique("ListViewItem"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        list_view_item->GetNode()->AddTag(NAME("DataSourceElementUUID"), NodeTag(uuid));
        list_view_item->SetDataSourceElementUUID(uuid);
        
        list_view_item->OnClick.Bind([this, list_view_item_weak = list_view_item.ToWeak()](const MouseEvent &event) -> UIEventHandlerResult
        {
            RC<UIListViewItem> list_view_item = list_view_item_weak.Lock();

            if (!list_view_item) {
                return UIEventHandlerResult::ERR;
            }

            list_view_item->Focus();
            m_selected_item = list_view_item;

            OnSelectedItemChange.Broadcast(list_view_item.Get());

            return UIEventHandlerResult::OK;
        }).Detach();

        // @TODO : Display the data in the list view item
        // temp : just display the uuid
        RC<UIText> text = GetStage()->CreateUIObject<UIText>(Name::Unique("Text"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        text->SetText(uuid.ToString());
        list_view_item->AddChildUIObject(text);

        // add the list view item to the list view
        AddChildUIObject(list_view_item);
    });

    m_data_source_on_element_remove_handler = m_data_source->OnElementRemove.Bind([this](UIDataSourceBase *data_source_ptr, UUID uuid, ConstAnyRef ref)
    {
        HYP_NAMED_SCOPE("Remove element from data source from list view");

        const auto it = m_list_view_items.FindIf([uuid](UIObject *ui_object)
        {
            if (!ui_object) {
                return false;
            }

            // @TODO Only store UIListViewItem in m_list_view_items so we don't have to cast
            UIListViewItem *list_view_item = dynamic_cast<UIListViewItem *>(ui_object);

            if (!list_view_item) {
                return false;
            }

            return list_view_item->GetDataSourceElementUUID() == uuid;
        });

        if (it != m_list_view_items.End()) {
            // If the item is selected, deselect it
            if (m_selected_item.Lock().Get() == *it) {
                m_selected_item.Reset();

                OnSelectedItemChange.Broadcast(nullptr);
            }
            
            RemoveChildUIObject(*it);
        }
    });

    m_data_source_on_element_update_handler = m_data_source->OnElementUpdate.Bind([this](UIDataSourceBase *data_source_ptr, UUID uuid, ConstAnyRef ref)
    {
        HYP_NAMED_SCOPE("Update element from data source in list view");

        // @TODO : update the list view item with the new data
    });
}

void UIListView::Init()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    UIPanel::Init();

    AssertThrow(GetStage() != nullptr);
}

void UIListView::AddChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    // RC<UIListViewItem> list_view_item = GetStage()->CreateUIObject<UIListViewItem>(Name::Unique("ListViewItem"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    // list_view_item->AddChildUIObject(ui_object);

    // m_list_view_items.PushBack(list_view_item);

    // UIObject::AddChildUIObject(std::move(list_view_item));

    if (!ui_object) {
        return;
    }

    m_list_view_items.PushBack(ui_object);

    UIObject::AddChildUIObject(ui_object);

    UpdateSize(false);
}

bool UIListView::RemoveChildUIObject(UIObject *ui_object)
{
    HYP_SCOPE;

    if (!ui_object) {
        return false;
    }
    
    for (SizeType i = 0; i < m_list_view_items.Size(); i++) {
        if (m_list_view_items[i] == ui_object) {
            AssertThrow(UIObject::RemoveChildUIObject(ui_object));

            m_list_view_items.EraseAt(i);

            // Updates layout as well
            UpdateSize(false);

            return true;
        }
    }

    return UIObject::RemoveChildUIObject(ui_object);
}

void UIListView::UpdateSize(bool update_children)
{
    HYP_SCOPE;

    UIPanel::UpdateSize(update_children);

    UpdateLayout();
}

void UIListView::UpdateLayout()
{
    HYP_SCOPE;

    if (m_list_view_items.Empty()) {
        return;
    }

    int y_offset = 0;

    const Vec2i actual_size = GetActualSize();

    for (SizeType i = 0; i < m_list_view_items.Size(); i++) {
        UIObject *list_view_item = m_list_view_items[i];

        if (!list_view_item) {
            continue;
        }

        list_view_item->SetPosition({ 0, y_offset });

        y_offset += list_view_item->GetActualSize().y;
    }
}

#pragma region UIListView

} // namespace hyperion
