/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_TAB_VIEW_HPP
#define HYPERION_UI_TAB_VIEW_HPP

#include <ui/UIPanel.hpp>

#include <core/containers/Array.hpp>

namespace hyperion {

class UIText;

#pragma region UITab

class HYP_API UITab : public UIPanel
{
public:
    UITab(UIStage *stage, NodeProxy node_proxy);
    UITab(const UITab &other)                   = delete;
    UITab &operator=(const UITab &other)        = delete;
    UITab(UITab &&other) noexcept               = delete;
    UITab &operator=(UITab &&other) noexcept    = delete;
    virtual ~UITab() override                   = default;

    /*! \brief Get the title of the tab.
     * 
     * \return The title of the tab.
     */
    [[nodiscard]]
    HYP_FORCE_INLINE
    const String &GetTitle() const
        { return m_title; }

    /*! \brief Set the title of the tab.
     * 
     * \param title The title of the tab.
     */
    void SetTitle(const String &title);

    [[nodiscard]]
    HYP_FORCE_INLINE
    RC<UIPanel> GetContents() const
        { return m_contents; }

    virtual void Init() override;

protected:
    virtual Handle<Material> GetMaterial() const override;

private:
    String      m_title;
    RC<UIText>  m_title_text;

    RC<UIPanel> m_contents;
};

#pragma endregion UITab

#pragma region UITabView

class HYP_API UITabView : public UIPanel
{
public:
    UITabView(UIStage *stage, NodeProxy node_proxy);
    UITabView(const UITabView &other)                   = delete;
    UITabView &operator=(const UITabView &other)        = delete;
    UITabView(UITabView &&other) noexcept               = delete;
    UITabView &operator=(UITabView &&other) noexcept    = delete;
    virtual ~UITabView() override                       = default;

    virtual void Init() override;

    /*! \brief Gets the index of the selected tab.
     * 
     * \return The index of the selected tab.
     */
    [[nodiscard]]
    HYP_FORCE_INLINE
    uint GetSelectedTabIndex() const
        { return m_selected_tab_index; }

    /*! \brief Sets the selected tab by index.
     * 
     * \param index The index of the tab.
     */
    void SetSelectedTabIndex(uint index);

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Array<RC<UITab>> &GetTabs() const
        { return m_tabs; }

    /*! \brief Add a tab to the tab view.
     * 
     * \param name The name of the tab.
     * \param title The title of the tab.
     * \return A reference counted pointer to the tab.
     */
    RC<UITab> AddTab(Name name, const String &title);

    /*! \brief Get a tab by name. Returns nullptr if the tab does not exist.
     * 
     * \param name The name of the tab.
     * \return A reference counted pointer to tab, or nullptr if the tab does not exist.
     */
    [[nodiscard]]
    RC<UITab> GetTab(Name name) const;

    /*! \brief Get the tab index by name. Returns ~0u if the tab does not exist.
     * 
     * \param name The name of the tab.
     * \return The index of the tab.
     */
    [[nodiscard]]
    uint GetTabIndex(Name name) const;

    /*! \brief Remove a tab by name.
     * 
     * \param name The name of the tab.
     * \return True if the tab was removed, false if the tab does not exist.
     */
    bool RemoveTab(Name name);

private:
    void UpdateTabSizes();

    RC<UIPanel>         m_container;

    Array<RC<UITab>>    m_tabs;

    uint                m_selected_tab_index;
};

#pragma endregion UITabView

} // namespace hyperion

#endif