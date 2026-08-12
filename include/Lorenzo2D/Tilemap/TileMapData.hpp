#pragma once

#include <Lorenzo2D/Assets/AssetId.hpp>
#include <Lorenzo2D/Tilemap/TileSet.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace l2d
{
    using TileId = std::uint32_t;
    inline constexpr TileId EmptyTile = 0u;

    using PropertyValue = std::variant<bool, std::int64_t, double, std::string>;
    using PropertyMap = std::unordered_map<std::string, PropertyValue>;

    enum class TileCollisionKind
    {
        None,
        Solid
    };

    enum class TileMapOrientation
    {
        Orthogonal,
        Isometric
    };

    enum class TileMapLayerRole
    {
        Ground,
        Decoration,
        Collision,
        Trigger,
        Navigation,
        Object
    };

    enum class TileFlipFlags : std::uint8_t
    {
        None = 0u,
        Horizontal = 1u << 0u,
        Vertical = 1u << 1u,
        Diagonal = 1u << 2u
    };

    TileFlipFlags operator|(TileFlipFlags left, TileFlipFlags right);
    bool hasFlag(TileFlipFlags value, TileFlipFlags flag);

    struct TileDefinition
    {
        TileId id = EmptyTile;
        AssetId texture;
        sf::IntRect textureRect;
        std::vector<TileAnimationFrame> animation;
        TileCollisionKind collision = TileCollisionKind::None;
        bool navigable = true;
        float movementCost = 1.f;
        PropertyMap properties;
    };

    struct TileMapLayer
    {
        std::string name;
        TileMapLayerRole role = TileMapLayerRole::Ground;
        bool visible = true;
        float opacity = 1.f;
        std::vector<TileId> tiles;
        std::vector<TileFlipFlags> flipFlags;
        PropertyMap properties;
    };

    struct TileMapObject
    {
        std::uint64_t id = 0u;
        std::string name;
        std::string type;
        sf::Vector2f position;
        sf::Vector2f size;
        float rotation = 0.f;
        PropertyMap properties;
    };

    class TileMapData
    {
      public:
        static constexpr std::size_t MaximumCellCount = 16u * 1024u * 1024u;
        static constexpr std::size_t MaximumLayerCount = 1024u;
        static constexpr std::size_t MaximumObjectCount = 1000000u;

        bool setDimensions(std::size_t width, std::size_t height);
        std::size_t width() const;
        std::size_t height() const;
        std::size_t cellCount() const;

        bool setTileSize(sf::Vector2f tileSize);
        sf::Vector2f tileSize() const;

        void setOrientation(TileMapOrientation orientation);
        TileMapOrientation orientation() const;

        bool setDefinition(TileDefinition definition);
        bool removeDefinition(TileId id);
        const TileDefinition* definition(TileId id) const;
        const std::unordered_map<TileId, TileDefinition>& definitions() const;

        bool addLayer(TileMapLayer layer);
        bool replaceLayer(std::size_t index, TileMapLayer layer);
        TileMapLayer* layer(std::size_t index);
        const TileMapLayer* layer(std::size_t index) const;
        std::vector<TileMapLayer>& layers();
        const std::vector<TileMapLayer>& layers() const;

        bool addObject(TileMapObject object);
        const std::vector<TileMapObject>& objects() const;

        std::optional<TileId> tileAt(std::size_t layerIndex, std::size_t row,
                                     std::size_t column) const;
        bool setTile(std::size_t layerIndex, std::size_t row, std::size_t column, TileId tile,
                     TileFlipFlags flags = TileFlipFlags::None);

        PropertyMap& properties();
        const PropertyMap& properties() const;

        bool isValid() const;
        void clear();

      private:
        bool validLayer(const TileMapLayer& layer) const;
        std::optional<std::size_t> indexOf(std::size_t row, std::size_t column) const;

        std::size_t m_width = 0u;
        std::size_t m_height = 0u;
        sf::Vector2f m_tileSize = {1.f, 1.f};
        TileMapOrientation m_orientation = TileMapOrientation::Orthogonal;
        std::unordered_map<TileId, TileDefinition> m_definitions;
        std::vector<TileMapLayer> m_layers;
        std::vector<TileMapObject> m_objects;
        PropertyMap m_properties;
    };

    bool isValidTileDefinition(const TileDefinition& definition);
}
