#include <Lorenzo2DEditor/AssetPickingModel.hpp>

namespace l2d_editor
{
    AssetPickingModel::AssetPickingModel(AssetBrowserModel& browser,
                                         ComponentInspectorModel& inspector) noexcept
        : m_browser(&browser), m_inspector(&inspector)
    {
    }

    bool AssetPickingModel::canApplySelected(AssetPickTarget target) const
    {
        if (m_browser == nullptr || m_inspector == nullptr || !m_inspector->snapshot())
            return false;

        const AssetBrowserEntry* entry = m_browser->selectedEntry();
        if (entry == nullptr || !m_browser->isSelectedResolvable()) return false;

        if (target == AssetPickTarget::SpriteTexture)
            return entry->kind == AssetBrowserEntryKind::Texture;
        return true;
    }

    bool AssetPickingModel::applySelected(AssetPickTarget target)
    {
        if (!canApplySelected(target)) return false;

        const AssetBrowserEntry* entry = m_browser->selectedEntry();
        if (entry == nullptr) return false;

        switch (target)
        {
        case AssetPickTarget::SpriteTexture:
            return m_inspector->setSpriteTextureAsset(entry->id);
        case AssetPickTarget::AnimatorClip:
            return m_inspector->addAnimatorClipAsset(entry->id);
        case AssetPickTarget::AnimatorInitialClip:
            return m_inspector->setAnimatorInitialClipAsset(entry->id);
        }
        return false;
    }
}
