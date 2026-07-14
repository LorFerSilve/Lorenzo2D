#include <Lorenzo2D/Assets/AssetHandle.hpp>
#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Renderer/DebugOverlay.hpp>
#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "TestSupport.hpp"

namespace
{
    using l2d::test::runTest;

    struct TrackedAsset
    {
        TrackedAsset(int assetValue, int& destructionCounter)
            : value(assetValue),
            destructionCount(&destructionCounter)
        {
        }

        ~TrackedAsset()
        {
            ++(*destructionCount);
        }

        int value = 0;
        int* destructionCount = nullptr;
    };

    l2d::TextureHandle makeTextureHandle()
    {
        return l2d::TextureHandle(std::make_shared<sf::Texture>());
    }

    l2d::FontHandle makeFontHandle()
    {
        return l2d::FontHandle(std::make_shared<sf::Font>());
    }

    void testGenericHandleOwnershipAndValueSemantics()
    {
        static_assert(std::is_copy_constructible<l2d::AssetHandle<int>>::value);
        static_assert(std::is_copy_assignable<l2d::AssetHandle<int>>::value);
        static_assert(std::is_nothrow_move_constructible<
            l2d::AssetHandle<int>>::value);
        static_assert(std::is_nothrow_move_assignable<
            l2d::AssetHandle<int>>::value);

        int destructionCount = 0;

        {
            std::shared_ptr<TrackedAsset> asset =
                std::make_shared<TrackedAsset>(42, destructionCount);
            const TrackedAsset* assetAddress = asset.get();

            l2d::AssetHandle<TrackedAsset> first(std::move(asset));
            L2D_REQUIRE(asset == nullptr);
            L2D_REQUIRE(first != nullptr);
            L2D_REQUIRE(first.get() == assetAddress);
            L2D_REQUIRE(first->value == 42);
            L2D_REQUIRE((*first).value == 42);

            l2d::AssetHandle<TrackedAsset> copy = first;
            L2D_REQUIRE(copy == first);

            l2d::AssetHandle<TrackedAsset> moved = std::move(copy);
            L2D_REQUIRE(copy == nullptr);
            L2D_REQUIRE(moved == first);

            l2d::AssetHandle<TrackedAsset> empty;
            L2D_REQUIRE(!empty);
            L2D_REQUIRE(nullptr == empty);
            L2D_REQUIRE(nullptr != first);

            swap(empty, moved);
            L2D_REQUIRE(empty == first);
            L2D_REQUIRE(moved == nullptr);

            first.reset();
            L2D_REQUIRE(first == nullptr);
            L2D_REQUIRE(destructionCount == 0);

            empty.reset();
            L2D_REQUIRE(destructionCount == 1);
        }

        L2D_REQUIRE(destructionCount == 1);
    }

    void testTextureRegistryGenerationsAndRemoval()
    {
        l2d::AssetManager assets;
        L2D_REQUIRE(assets.textureCount() == 0u);
        L2D_REQUIRE(!assets.hasTexture("hero"));
        L2D_REQUIRE(!assets.getTexture("hero"));
        L2D_REQUIRE(!assets.unloadTexture("hero"));
        L2D_REQUIRE(!assets.storeTexture("hero", {}));

        l2d::TextureHandle firstGeneration = makeTextureHandle();
        const sf::Texture* firstAddress = firstGeneration.get();

        L2D_REQUIRE(assets.storeTexture("hero", firstGeneration));
        L2D_REQUIRE(assets.textureCount() == 1u);
        L2D_REQUIRE(assets.hasTexture("hero"));
        L2D_REQUIRE(assets.getTexture("hero") == firstGeneration);

        l2d::TextureHandle secondGeneration = makeTextureHandle();
        const sf::Texture* secondAddress = secondGeneration.get();
        L2D_REQUIRE(secondAddress != firstAddress);

        L2D_REQUIRE(assets.storeTexture("hero", secondGeneration));
        L2D_REQUIRE(assets.textureCount() == 1u);
        L2D_REQUIRE(assets.getTexture("hero") == secondGeneration);
        L2D_REQUIRE(firstGeneration.get() == firstAddress);

        L2D_REQUIRE(!assets.storeTexture("hero", {}));
        L2D_REQUIRE(assets.getTexture("hero") == secondGeneration);

        L2D_REQUIRE(assets.unloadTexture("hero"));
        L2D_REQUIRE(!assets.unloadTexture("hero"));
        L2D_REQUIRE(assets.textureCount() == 0u);
        L2D_REQUIRE(!assets.getTexture("hero"));
        L2D_REQUIRE(firstGeneration.get() == firstAddress);
        L2D_REQUIRE(secondGeneration.get() == secondAddress);
    }

    void testFontRegistryGenerationsAndClearing()
    {
        l2d::AssetManager assets;
        L2D_REQUIRE(assets.fontCount() == 0u);
        L2D_REQUIRE(!assets.hasFont("ui"));
        L2D_REQUIRE(!assets.getFont("ui"));
        L2D_REQUIRE(!assets.unloadFont("ui"));
        L2D_REQUIRE(!assets.storeFont("ui", {}));

        l2d::FontHandle firstGeneration = makeFontHandle();
        const sf::Font* firstAddress = firstGeneration.get();
        L2D_REQUIRE(assets.storeFont("ui", firstGeneration));
        L2D_REQUIRE(assets.fontCount() == 1u);
        L2D_REQUIRE(assets.getFont("ui") == firstGeneration);

        l2d::FontHandle secondGeneration = makeFontHandle();
        const sf::Font* secondAddress = secondGeneration.get();
        L2D_REQUIRE(secondAddress != firstAddress);
        L2D_REQUIRE(assets.storeFont("ui", secondGeneration));
        L2D_REQUIRE(assets.fontCount() == 1u);
        L2D_REQUIRE(assets.getFont("ui") == secondGeneration);
        L2D_REQUIRE(firstGeneration.get() == firstAddress);

        L2D_REQUIRE(!assets.storeFont("ui", {}));
        L2D_REQUIRE(assets.getFont("ui") == secondGeneration);

        L2D_REQUIRE(assets.storeTexture("icon", makeTextureHandle()));
        assets.clearFonts();
        L2D_REQUIRE(assets.fontCount() == 0u);
        L2D_REQUIRE(assets.textureCount() == 1u);
        L2D_REQUIRE(firstGeneration.get() == firstAddress);
        L2D_REQUIRE(secondGeneration.get() == secondAddress);

        assets.clearAll();
        L2D_REQUIRE(assets.fontCount() == 0u);
        L2D_REQUIRE(assets.textureCount() == 0u);
    }

    void testFailedLoadsPreserveRegisteredGenerations()
    {
        const std::string missingTexturePath =
            "lorenzo2d_missing_asset_directory/missing_texture.png";
        const std::string missingFontPath =
            "lorenzo2d_missing_asset_directory/missing_font.ttf";

        l2d::AssetManager assets;
        const l2d::TextureHandle texture = makeTextureHandle();
        const l2d::FontHandle font = makeFontHandle();

        L2D_REQUIRE(assets.storeTexture("hero", texture));
        L2D_REQUIRE(assets.storeFont("ui", font));

        L2D_REQUIRE(!assets.loadTexture(
            "hero",
            missingTexturePath,
            false
        ));
        L2D_REQUIRE(!assets.loadFont("ui", missingFontPath));
        L2D_REQUIRE(assets.getTexture("hero") == texture);
        L2D_REQUIRE(assets.getFont("ui") == font);
        L2D_REQUIRE(assets.textureCount() == 1u);
        L2D_REQUIRE(assets.fontCount() == 1u);

        L2D_REQUIRE(!assets.loadTexture(
            "missing",
            missingTexturePath
        ));
        L2D_REQUIRE(!assets.loadFont("missing", missingFontPath));
        L2D_REQUIRE(!assets.hasTexture("missing"));
        L2D_REQUIRE(!assets.hasFont("missing"));
        L2D_REQUIRE(assets.textureCount() == 1u);
        L2D_REQUIRE(assets.fontCount() == 1u);
    }

    void testHandlesSurviveManagerDestruction()
    {
        l2d::TextureHandle textureLease;
        l2d::FontHandle fontLease;
        const sf::Texture* textureAddress = nullptr;
        const sf::Font* fontAddress = nullptr;

        {
            l2d::AssetManager assets;
            L2D_REQUIRE(assets.storeTexture("hero", makeTextureHandle()));
            L2D_REQUIRE(assets.storeFont("ui", makeFontHandle()));

            textureLease = assets.getTexture("hero");
            fontLease = assets.getFont("ui");
            textureAddress = textureLease.get();
            fontAddress = fontLease.get();
        }

        L2D_REQUIRE(textureLease);
        L2D_REQUIRE(fontLease);
        L2D_REQUIRE(textureLease.get() == textureAddress);
        L2D_REQUIRE(fontLease.get() == fontAddress);
    }

    void testSpriteRendererRetainsAndRebindsLeases()
    {
        bool threw = false;

        try
        {
            l2d::SpriteRenderer invalidRenderer({});
            (void)invalidRenderer;
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        L2D_REQUIRE(threw);

        std::weak_ptr<const sf::Texture> secondWeak;

        {
            l2d::AssetManager assets;
            std::shared_ptr<sf::Texture> firstAsset =
                std::make_shared<sf::Texture>();
            const std::weak_ptr<const sf::Texture> firstWeak = firstAsset;
            l2d::TextureHandle first(std::move(firstAsset));
            const sf::Texture* firstAddress = first.get();
            L2D_REQUIRE(assets.storeTexture("hero", first));

            l2d::SpriteRenderer renderer(assets.getTexture("hero"));
            first.reset();
            assets.clearTextures();
            L2D_REQUIRE(!firstWeak.expired());
            L2D_REQUIRE(renderer.textureHandle());
            L2D_REQUIRE(renderer.textureHandle().get() == firstAddress);

            l2d::TextureHandle bindingBeforeInvalidSet =
                renderer.textureHandle();
            L2D_REQUIRE(!renderer.setTexture({}));
            L2D_REQUIRE(renderer.textureHandle() == bindingBeforeInvalidSet);

            std::shared_ptr<sf::Texture> secondAsset =
                std::make_shared<sf::Texture>();
            secondWeak = secondAsset;
            l2d::TextureHandle second(std::move(secondAsset));
            const sf::Texture* secondAddress = second.get();

            bindingBeforeInvalidSet.reset();
            L2D_REQUIRE(renderer.setTexture(second, false));
            L2D_REQUIRE(firstWeak.expired());

            second.reset();
            L2D_REQUIRE(!secondWeak.expired());
            L2D_REQUIRE(renderer.textureHandle());
            L2D_REQUIRE(renderer.textureHandle().get() == secondAddress);
        }

        L2D_REQUIRE(secondWeak.expired());
    }

    void testDebugOverlayRetainsAndClearsLeases()
    {
        static_assert(!std::is_copy_constructible<l2d::DebugOverlay>::value);
        static_assert(!std::is_copy_assignable<l2d::DebugOverlay>::value);
        static_assert(!std::is_move_constructible<l2d::DebugOverlay>::value);
        static_assert(!std::is_move_assignable<l2d::DebugOverlay>::value);

        l2d::DebugOverlay overlay;
        L2D_REQUIRE(!overlay.hasFont());
        L2D_REQUIRE(!overlay.fontHandle());
        L2D_REQUIRE(!overlay.setFont({}));
        L2D_REQUIRE(!overlay.hasFont());

        l2d::AssetManager assets;
        std::shared_ptr<sf::Font> firstAsset =
            std::make_shared<sf::Font>();
        const std::weak_ptr<const sf::Font> firstWeak = firstAsset;
        l2d::FontHandle first(std::move(firstAsset));
        const sf::Font* firstAddress = first.get();
        L2D_REQUIRE(assets.storeFont("debug", first));
        L2D_REQUIRE(overlay.setFont(assets.getFont("debug")));

        first.reset();
        assets.clearFonts();
        L2D_REQUIRE(overlay.hasFont());
        L2D_REQUIRE(overlay.fontHandle());
        L2D_REQUIRE(overlay.fontHandle().get() == firstAddress);

        l2d::FontHandle bindingBeforeInvalidSet = overlay.fontHandle();
        L2D_REQUIRE(!overlay.setFont({}));
        L2D_REQUIRE(overlay.fontHandle() == bindingBeforeInvalidSet);
        L2D_REQUIRE(!overlay.loadFontFromFile(
            "lorenzo2d_missing_asset_directory/missing_overlay_font.ttf"
        ));
        L2D_REQUIRE(overlay.fontHandle() == bindingBeforeInvalidSet);

        std::shared_ptr<sf::Font> secondAsset =
            std::make_shared<sf::Font>();
        const std::weak_ptr<const sf::Font> secondWeak = secondAsset;
        l2d::FontHandle second(std::move(secondAsset));
        const sf::Font* secondAddress = second.get();

        bindingBeforeInvalidSet.reset();
        L2D_REQUIRE(overlay.setFont(second));
        L2D_REQUIRE(firstWeak.expired());

        second.reset();
        L2D_REQUIRE(!secondWeak.expired());
        L2D_REQUIRE(overlay.fontHandle().get() == secondAddress);

        overlay.clearFont();
        L2D_REQUIRE(secondWeak.expired());
        L2D_REQUIRE(!overlay.hasFont());
        L2D_REQUIRE(!overlay.fontHandle());
    }

    void testAssetManagerMoveContract()
    {
        static_assert(!std::is_copy_constructible<l2d::AssetManager>::value);
        static_assert(!std::is_copy_assignable<l2d::AssetManager>::value);
        static_assert(std::is_nothrow_move_constructible<
            l2d::AssetManager>::value);
        static_assert(std::is_nothrow_move_assignable<
            l2d::AssetManager>::value);

        l2d::AssetManager source;
        const l2d::TextureHandle texture = makeTextureHandle();
        const l2d::FontHandle font = makeFontHandle();
        L2D_REQUIRE(source.storeTexture("hero", texture));
        L2D_REQUIRE(source.storeFont("ui", font));

        l2d::AssetManager moved(std::move(source));
        L2D_REQUIRE(moved.getTexture("hero") == texture);
        L2D_REQUIRE(moved.getFont("ui") == font);
        L2D_REQUIRE(moved.textureCount() == 1u);
        L2D_REQUIRE(moved.fontCount() == 1u);

        l2d::AssetManager destination;
        const l2d::TextureHandle replaced = makeTextureHandle();
        L2D_REQUIRE(destination.storeTexture("old", replaced));
        destination = std::move(moved);

        L2D_REQUIRE(destination.getTexture("hero") == texture);
        L2D_REQUIRE(destination.getFont("ui") == font);
        L2D_REQUIRE(!destination.hasTexture("old"));
        L2D_REQUIRE(replaced);
    }

}

int main()
{
    int failures = 0;

    runTest("generic handle ownership and value semantics",
        testGenericHandleOwnershipAndValueSemantics, failures);
    runTest("texture registry generations and removal",
        testTextureRegistryGenerationsAndRemoval, failures);
    runTest("font registry generations and clearing",
        testFontRegistryGenerationsAndClearing, failures);
    runTest("failed loads preserve registered generations",
        testFailedLoadsPreserveRegisteredGenerations, failures);
    runTest("handles survive manager destruction",
        testHandlesSurviveManagerDestruction, failures);
    runTest("sprite renderer retains and rebinds leases",
        testSpriteRendererRetainsAndRebindsLeases, failures);
    runTest("debug overlay retains and clears leases",
        testDebugOverlayRetainsAndClearsLeases, failures);
    runTest("asset manager move contract",
        testAssetManagerMoveContract, failures);

    if (failures != 0)
    {
        std::cerr << failures << " asset test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D asset tests passed.\n";
    return 0;
}
