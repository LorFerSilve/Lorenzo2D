#pragma once

#include <Lorenzo2DEditor/AssetBrowserModel.hpp>
#include <Lorenzo2DEditor/ComponentInspectorModel.hpp>

#include <cstdint>

namespace l2d_editor
{
    enum class AssetPickTarget : std::uint8_t
    {
        SpriteTexture,
        AnimatorClip,
        AnimatorInitialClip
    };

    // Selection-to-inspector boundary. It revalidates the selected browser entry
    // against the configured ResourceLocator roots immediately before applying it.
    class AssetPickingModel
    {
      public:
        AssetPickingModel(AssetBrowserModel& browser, ComponentInspectorModel& inspector) noexcept;

        [[nodiscard]] bool canApplySelected(AssetPickTarget target) const;
        [[nodiscard]] bool applySelected(AssetPickTarget target);

      private:
        AssetBrowserModel* m_browser = nullptr;
        ComponentInspectorModel* m_inspector = nullptr;
    };
}
