#include <Lorenzo2DEditor/AssetBrowserModel.hpp>
#include <Lorenzo2DEditor/AssetPickingModel.hpp>
#include <Lorenzo2DEditor/ComponentInspectorModel.hpp>
#include <Lorenzo2DEditor/EditorCommandHistory.hpp>
#include <Lorenzo2DEditor/EditorDocument.hpp>

#include <Lorenzo2D/Scene/Prefab.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    class TempDirectory
    {
      public:
        TempDirectory()
        {
            static std::size_t counter = 0u;
            const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            m_path = std::filesystem::temp_directory_path() /
                     ("lorenzo2d-editor-assets-" + std::to_string(ticks) + "-" +
                      std::to_string(counter++));
            std::error_code error;
            const bool created = std::filesystem::create_directories(m_path, error);
            require(!error && created, "unable to create temporary asset-browser directory");
        }

        ~TempDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(m_path, error);
        }

        TempDirectory(const TempDirectory&) = delete;
        TempDirectory& operator=(const TempDirectory&) = delete;

        [[nodiscard]] const std::filesystem::path& path() const noexcept
        {
            return m_path;
        }

      private:
        std::filesystem::path m_path;
    };

    void writeFile(const std::filesystem::path& path)
    {
        std::error_code error;
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), error);
        require(!error, "unable to create asset test parent directory");
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        require(static_cast<bool>(output), "unable to create asset test file");
        output << "asset";
        require(static_cast<bool>(output), "unable to write asset test file");
    }

    bool hasAsset(const l2d_editor::AssetBrowserModel& browser, const std::string& id)
    {
        for (const l2d_editor::AssetBrowserEntry& entry : browser.entries())
            if (entry.id == id) return true;
        return false;
    }

    bool selectAsset(l2d_editor::AssetBrowserModel& browser, const std::string& id)
    {
        for (std::size_t index = 0u; index < browser.visibleCount(); ++index)
        {
            const l2d_editor::AssetBrowserEntry* entry = browser.visibleEntry(index);
            if (entry == nullptr || entry->id != id) continue;
            if (browser.selectedAssetId() && *browser.selectedAssetId() == id) return true;
            return browser.selectVisibleIndex(index);
        }
        return false;
    }

    l2d_editor::EditorDocument makeSelectedDocument()
    {
        l2d::LevelDocument level;
        level.name = "Asset Picking Test";
        l2d::Prefab prefab;
        prefab.name = "Actor";
        level.objects.push_back(std::move(prefab));

        l2d_editor::EditorDocument document;
        require(document.replace(std::move(level)), "asset picking document replacement failed");
        require(document.selectObject(1u), "asset picking document selection failed");
        return document;
    }

    void testDeterministicBrowsingFilteringAndRootPrecedence()
    {
        TempDirectory temporary;
        const std::filesystem::path rootA = temporary.path() / "root-a";
        const std::filesystem::path rootB = temporary.path() / "root-b";
        writeFile(rootA / "clips" / "idle.anim");
        writeFile(rootA / "textures" / "shared.png");
        writeFile(rootA / "textures" / "z.PNG");
        writeFile(rootB / "misc.txt");
        writeFile(rootB / "textures" / "a.jpg");
        writeFile(rootB / "textures" / "shared.png");
        writeFile(temporary.path() / "outside.png");

        l2d_editor::AssetBrowserModel browser;
        require(browser.setRoots({rootA, rootB}), "asset browser rejected valid roots");
        require(browser.refresh(), "asset browser failed to refresh valid roots");
        require(browser.lastError() == l2d_editor::AssetBrowserError::None,
                "successful asset refresh retained an error");
        require(browser.entries().size() == 5u,
                "duplicate root asset IDs were not collapsed by root precedence");

        const std::vector<std::string> expected = {"clips/idle.anim", "misc.txt", "textures/a.jpg",
                                                   "textures/shared.png", "textures/z.PNG"};
        for (std::size_t index = 0u; index < expected.size(); ++index)
            require(browser.entries()[index].id == expected[index],
                    "asset browser ordering is not deterministic by AssetId");

        require(browser.entries()[3].rootIndex == 0u,
                "duplicate AssetId did not retain first configured resource root");
        require(browser.entries()[2].kind == l2d_editor::AssetBrowserEntryKind::Texture &&
                    browser.entries()[3].kind == l2d_editor::AssetBrowserEntryKind::Texture &&
                    browser.entries()[4].kind == l2d_editor::AssetBrowserEntryKind::Texture,
                "texture extension classification is incorrect");
        require(browser.entries()[0].kind == l2d_editor::AssetBrowserEntryKind::Generic,
                "generic asset was misclassified as a texture");

        browser.setFilter("TEXTURES");
        require(browser.visibleCount() == 3u,
                "case-insensitive asset filter exposed the wrong result count");
        require(selectAsset(browser, "textures/shared.png"),
                "unable to select filtered asset by visible index");
        require(browser.selectedAssetId() && *browser.selectedAssetId() == "textures/shared.png",
                "asset selection did not preserve the selected AssetId");

        browser.setFilter("idle");
        require(browser.visibleCount() == 1u, "asset filter did not narrow deterministically");
        require(!browser.selectedAssetId(), "hidden asset selection was not cleared by filtering");
        browser.setFilter({});

        require(!browser.resolveAssetId("../outside.png"),
                "asset browser resolved a parent traversal outside configured roots");
        require(!browser.resolveAssetId((temporary.path() / "outside.png").generic_string()),
                "asset browser accepted an absolute asset ID");
        require(!l2d_editor::AssetBrowserModel::isPortableAssetId("a/../b.png"),
                "portable AssetId validation accepted traversal components");
        require(!l2d_editor::AssetBrowserModel::isPortableAssetId("broken\\path.png"),
                "portable AssetId validation accepted platform-specific separators");

        std::error_code symlinkError;
        std::filesystem::create_symlink(temporary.path() / "outside.png", rootA / "escape.png",
                                        symlinkError);
        if (!symlinkError)
        {
            require(browser.refresh(), "browser failed after optional symlink fixture was created");
            require(!hasAsset(browser, "escape.png"),
                    "asset browser published a symlink entry instead of a bounded resource file");
        }
    }

    void testRefreshLimitsAreTransactional()
    {
        TempDirectory temporary;
        const std::filesystem::path scanRoot = temporary.path() / "scan-root";
        writeFile(scanRoot / "a.txt");
        writeFile(scanRoot / "b.txt");

        l2d_editor::AssetBrowserLimits scanLimits;
        scanLimits.maxScannedEntries = 2u;
        scanLimits.maxAssets = 8u;
        scanLimits.maxDepth = 4u;
        l2d_editor::AssetBrowserModel scanBrowser(scanLimits);
        require(scanBrowser.setRoots({scanRoot}), "bounded browser rejected valid scan root");
        require(scanBrowser.refresh(), "bounded browser rejected scan at exact limit");
        require(scanBrowser.entries().size() == 2u, "exact-limit scan published wrong asset count");

        writeFile(scanRoot / "c.txt");
        require(!scanBrowser.refresh(), "over-limit scan unexpectedly succeeded");
        require(scanBrowser.lastError() == l2d_editor::AssetBrowserError::ScanLimitExceeded,
                "over-limit scan reported the wrong error");
        require(scanBrowser.entries().size() == 2u && hasAsset(scanBrowser, "a.txt") &&
                    hasAsset(scanBrowser, "b.txt"),
                "failed scan replaced the previous published asset index");

        const std::filesystem::path assetRoot = temporary.path() / "asset-root";
        writeFile(assetRoot / "one.txt");
        l2d_editor::AssetBrowserLimits assetLimits;
        assetLimits.maxScannedEntries = 16u;
        assetLimits.maxAssets = 1u;
        assetLimits.maxDepth = 4u;
        l2d_editor::AssetBrowserModel assetBrowser(assetLimits);
        require(assetBrowser.setRoots({assetRoot}), "asset-limit browser rejected valid root");
        require(assetBrowser.refresh(), "single-asset index failed at exact asset limit");
        writeFile(assetRoot / "two.txt");
        require(!assetBrowser.refresh(), "over-limit asset publication unexpectedly succeeded");
        require(assetBrowser.lastError() == l2d_editor::AssetBrowserError::AssetLimitExceeded,
                "asset publication limit reported the wrong error");
        require(assetBrowser.entries().size() == 1u && hasAsset(assetBrowser, "one.txt"),
                "failed asset-limit refresh replaced the previous index");

        const std::filesystem::path notDirectory = temporary.path() / "root-file.txt";
        writeFile(notDirectory);
        l2d_editor::AssetBrowserModel invalidRootBrowser;
        require(invalidRootBrowser.setRoots({notDirectory}),
                "runtime-compatible root normalization unexpectedly failed for file path");
        require(!invalidRootBrowser.refresh(), "file resource root unexpectedly browsed as directory");
        require(invalidRootBrowser.lastError() == l2d_editor::AssetBrowserError::RootIsNotDirectory,
                "file resource root reported the wrong browser error");

        l2d_editor::AssetBrowserLimits invalidLimits;
        invalidLimits.maxAssets = 0u;
        l2d_editor::AssetBrowserModel invalidConfiguration(invalidLimits);
        require(!invalidConfiguration.setRoots({scanRoot}),
                "invalid browser limits unexpectedly accepted roots");
        require(invalidConfiguration.lastError() ==
                    l2d_editor::AssetBrowserError::InvalidConfiguration,
                "invalid browser limits reported the wrong error");
    }

    void testPickingUsesValidatedUndoableInspectorMutations()
    {
        TempDirectory temporary;
        const std::filesystem::path root = temporary.path() / "assets";
        writeFile(root / "textures" / "player.png");
        writeFile(root / "clips" / "idle.anim");
        writeFile(root / "clips" / "run.anim");

        l2d_editor::AssetBrowserModel browser;
        require(browser.setRoots({root}) && browser.refresh(),
                "asset picker browser setup failed");

        l2d_editor::EditorDocument document = makeSelectedDocument();
        l2d_editor::EditorCommandHistory history;
        l2d_editor::ComponentInspectorModel inspector(document, history);
        l2d_editor::AssetPickingModel picker(browser, inspector);

        require(selectAsset(browser, "textures/player.png"), "texture asset selection failed");
        require(picker.canApplySelected(l2d_editor::AssetPickTarget::SpriteTexture),
                "valid texture selection was not pickable for SpriteRenderer");
        require(picker.applySelected(l2d_editor::AssetPickTarget::SpriteTexture),
                "SpriteRenderer texture pick failed");
        const l2d::Prefab& withSprite = document.findObject(1u)->prefab;
        require(withSprite.spriteRenderer.has_value(), "texture pick did not create SpriteRenderer");
        require(withSprite.spriteRenderer->texture == "textures/player.png",
                "texture pick published the wrong AssetId");
        require(withSprite.spriteRenderer->textureRect.position == sf::Vector2i{0, 0} &&
                    withSprite.spriteRenderer->textureRect.size == sf::Vector2i{1, 1},
                "new SpriteRenderer did not receive the minimal valid source rectangle");
        require(history.undoCount() == 1u, "texture pick did not create exactly one undo command");
        require(history.undo(document), "texture pick undo failed");
        require(!document.findObject(1u)->prefab.spriteRenderer,
                "texture pick undo did not remove newly-created SpriteRenderer");
        require(history.redo(document), "texture pick redo failed");
        require(document.findObject(1u)->prefab.spriteRenderer.has_value(),
                "texture pick redo did not restore SpriteRenderer");

        require(selectAsset(browser, "clips/idle.anim"), "Animator clip selection failed");
        require(!picker.canApplySelected(l2d_editor::AssetPickTarget::SpriteTexture),
                "generic asset was incorrectly pickable as a SpriteRenderer texture");
        const std::size_t commandsBeforeRejectedTexture = history.undoCount();
        require(!picker.applySelected(l2d_editor::AssetPickTarget::SpriteTexture),
                "generic asset unexpectedly applied as a SpriteRenderer texture");
        require(history.undoCount() == commandsBeforeRejectedTexture,
                "rejected texture pick entered command history");

        require(picker.applySelected(l2d_editor::AssetPickTarget::AnimatorClip),
                "Animator clip pick failed");
        const l2d::Prefab& withAnimator = document.findObject(1u)->prefab;
        require(withAnimator.animator.has_value() && withAnimator.animator->clips.size() == 1u &&
                    withAnimator.animator->clips[0] == "clips/idle.anim" &&
                    withAnimator.animator->initialClip.empty(),
                "Animator clip pick published incorrect component state");
        const std::size_t commandsBeforeDuplicate = history.undoCount();
        require(!picker.applySelected(l2d_editor::AssetPickTarget::AnimatorClip),
                "duplicate Animator clip pick unexpectedly succeeded");
        require(history.undoCount() == commandsBeforeDuplicate,
                "duplicate Animator clip pick entered history");

        require(selectAsset(browser, "clips/run.anim"), "Animator initial clip selection failed");
        require(picker.applySelected(l2d_editor::AssetPickTarget::AnimatorInitialClip),
                "Animator initial clip pick failed");
        const l2d::AnimatorPrefab& animator = *document.findObject(1u)->prefab.animator;
        require(animator.clips.size() == 2u && animator.clips[1] == "clips/run.anim" &&
                    animator.initialClip == "clips/run.anim",
                "Animator initial clip pick did not preserve runtime validity");
        require(history.undo(document), "Animator initial clip undo failed");
        require(document.findObject(1u)->prefab.animator->clips.size() == 1u &&
                    document.findObject(1u)->prefab.animator->initialClip.empty(),
                "Animator initial clip undo did not restore prior state");
        require(history.redo(document), "Animator initial clip redo failed");

        std::error_code removeError;
        const bool removed = std::filesystem::remove(root / "clips" / "run.anim", removeError);
        require(!removeError && removed, "unable to remove stale-selection fixture");
        require(!browser.isSelectedResolvable(),
                "browser considered a removed selected asset still resolvable");
        require(!picker.canApplySelected(l2d_editor::AssetPickTarget::AnimatorInitialClip),
                "picker accepted a stale filesystem selection");

        require(selectAsset(browser, "textures/player.png"),
                "unable to restore valid selection after stale asset test");
        document.clearSelection();
        require(!picker.canApplySelected(l2d_editor::AssetPickTarget::SpriteTexture),
                "asset picker remained enabled without an editor object selection");
    }
}

int main()
{
    try
    {
        testDeterministicBrowsingFilteringAndRootPrecedence();
        testRefreshLimitsAreTransactional();
        testPickingUsesValidatedUndoableInspectorMutations();
        std::cout << "Lorenzo2D asset browser/picking tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Lorenzo2D asset browser/picking test failure: " << error.what() << '\n';
        return 1;
    }
}
