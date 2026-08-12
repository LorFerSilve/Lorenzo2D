#include <Lorenzo2D/Tilemap/TiledJsonImporter.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>

namespace l2d
{
    namespace
    {
        using Json = nlohmann::json;

        constexpr std::uint32_t HorizontalFlip = 0x80000000u;
        constexpr std::uint32_t VerticalFlip = 0x40000000u;
        constexpr std::uint32_t DiagonalFlip = 0x20000000u;
        constexpr std::uint32_t Rotation120 = 0x10000000u;
        constexpr std::uint32_t FlipMask =
            HorizontalFlip | VerticalFlip | DiagonalFlip | Rotation120;

        bool propertyValue(const Json& value, PropertyValue& output)
        {
            if (value.is_boolean())
                output = value.get<bool>();
            else if (value.is_number_integer())
                output = value.get<std::int64_t>();
            else if (value.is_number_unsigned())
            {
                const std::uint64_t number = value.get<std::uint64_t>();
                if (number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    return false;
                output = static_cast<std::int64_t>(number);
            }
            else if (value.is_number_float())
            {
                const double number = value.get<double>();
                if (!std::isfinite(number)) return false;
                output = number;
            }
            else if (value.is_string())
                output = value.get<std::string>();
            else
                return false;

            return true;
        }

        bool properties(const Json& owner, PropertyMap& output)
        {
            const auto iterator = owner.find("properties");
            if (iterator == owner.end()) return true;
            if (!iterator->is_array()) return false;

            PropertyMap parsed;
            for (const Json& property : *iterator)
            {
                if (!property.is_object() || !property.contains("name") ||
                    !property.at("name").is_string() || !property.contains("value"))
                    return false;

                const std::string name = property.at("name").get<std::string>();
                PropertyValue value;
                if (name.empty() || !propertyValue(property.at("value"), value)) return false;
                parsed.insert_or_assign(name, std::move(value));
            }
            output = std::move(parsed);
            return true;
        }

        template <typename T> const T* property(const PropertyMap& values, const std::string& key)
        {
            const auto iterator = values.find(key);
            return iterator == values.end() ? nullptr : std::get_if<T>(&iterator->second);
        }

        TileMapLayerRole layerRole(const std::string& name, const PropertyMap& values,
                                   bool objectLayer)
        {
            if (objectLayer) return TileMapLayerRole::Object;

            std::string role = name;
            if (const std::string* configured = property<std::string>(values, "role"))
                role = *configured;

            for (char& character : role)
                if (character >= 'A' && character <= 'Z') character = character - 'A' + 'a';

            if (role == "decoration") return TileMapLayerRole::Decoration;
            if (role == "collision") return TileMapLayerRole::Collision;
            if (role == "trigger" || role == "triggers") return TileMapLayerRole::Trigger;
            if (role == "navigation") return TileMapLayerRole::Navigation;
            return TileMapLayerRole::Ground;
        }

        bool parseTileDefinitions(const Json& root, TileMapData& data)
        {
            const auto tilesets = root.find("tilesets");
            if (tilesets == root.end()) return true;
            if (!tilesets->is_array()) return false;

            for (const Json& tilesetEntry : *tilesets)
            {
                if (!tilesetEntry.is_object() || tilesetEntry.contains("source") ||
                    !tilesetEntry.contains("firstgid") ||
                    !tilesetEntry.at("firstgid").is_number_unsigned())
                    return false;

                const std::uint64_t first64 = tilesetEntry.at("firstgid").get<std::uint64_t>();
                if (first64 == 0u || first64 > std::numeric_limits<TileId>::max()) return false;
                const Json& tileset =
                    tilesetEntry.contains("tileset") ? tilesetEntry.at("tileset") : tilesetEntry;
                if (!tileset.is_object()) return false;

                const std::uint64_t count = tileset.value("tilecount", std::uint64_t{0u});
                const std::uint64_t columns = tileset.value("columns", std::uint64_t{0u});
                const std::uint64_t tileWidth = tileset.value("tilewidth", std::uint64_t{0u});
                const std::uint64_t tileHeight = tileset.value("tileheight", std::uint64_t{0u});
                const std::string image = tileset.value("image", std::string{});

                if (count == 0u || count > TileMapData::MaximumCellCount || columns == 0u ||
                    tileWidth == 0u || tileHeight == 0u ||
                    tileWidth > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
                    tileHeight > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
                    image.empty() ||
                    first64 + count - 1u > std::numeric_limits<std::uint32_t>::max())
                    return false;

                for (std::uint64_t local = 0u; local < count; ++local)
                {
                    const std::uint64_t column = local % columns;
                    const std::uint64_t row = local / columns;
                    if (column * tileWidth >
                            static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
                        row * tileHeight >
                            static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
                        return false;

                    TileDefinition definition;
                    definition.id = static_cast<TileId>(first64 + local);
                    definition.texture = image;
                    definition.textureRect = {
                        {static_cast<int>(column * tileWidth), static_cast<int>(row * tileHeight)},
                        {static_cast<int>(tileWidth), static_cast<int>(tileHeight)}};
                    if (!data.setDefinition(std::move(definition))) return false;
                }

                const auto tiles = tileset.find("tiles");
                if (tiles == tileset.end()) continue;
                if (!tiles->is_array()) return false;

                for (const Json& tile : *tiles)
                {
                    if (!tile.is_object() || !tile.contains("id") ||
                        !tile.at("id").is_number_unsigned())
                        return false;
                    const std::uint64_t local = tile.at("id").get<std::uint64_t>();
                    if (local >= count) return false;

                    const TileId id = static_cast<TileId>(first64 + local);
                    const TileDefinition* existing = data.definition(id);
                    if (existing == nullptr) return false;
                    TileDefinition definition = *existing;
                    if (!properties(tile, definition.properties)) return false;

                    if (const bool* solid = property<bool>(definition.properties, "solid");
                        solid != nullptr && *solid)
                        definition.collision = TileCollisionKind::Solid;
                    if (const bool* navigable = property<bool>(definition.properties, "navigable"))
                        definition.navigable = *navigable;
                    if (const double* cost =
                            property<double>(definition.properties, "movementCost"))
                        definition.movementCost = static_cast<float>(*cost);
                    if (const std::int64_t* cost =
                            property<std::int64_t>(definition.properties, "movementCost"))
                        definition.movementCost = static_cast<float>(*cost);

                    const auto animation = tile.find("animation");
                    if (animation != tile.end())
                    {
                        if (!animation->is_array() || animation->empty()) return false;
                        definition.animation.clear();
                        for (const Json& frame : *animation)
                        {
                            if (!frame.is_object() || !frame.contains("tileid") ||
                                !frame.contains("duration") ||
                                !frame.at("tileid").is_number_unsigned() ||
                                !frame.at("duration").is_number_unsigned())
                                return false;
                            const std::uint64_t frameLocal =
                                frame.at("tileid").get<std::uint64_t>();
                            const std::uint64_t duration =
                                frame.at("duration").get<std::uint64_t>();
                            if (frameLocal >= count || duration == 0u) return false;
                            const TileDefinition* frameDefinition =
                                data.definition(static_cast<TileId>(first64 + frameLocal));
                            if (frameDefinition == nullptr) return false;
                            definition.animation.push_back({frameDefinition->textureRect,
                                                            static_cast<float>(duration) / 1000.f});
                        }
                    }

                    if (!data.setDefinition(std::move(definition))) return false;
                }
            }

            return true;
        }

        bool parseLayers(const Json& root, TileMapData& data)
        {
            const auto layers = root.find("layers");
            if (layers == root.end() || !layers->is_array() ||
                layers->size() > TileMapData::MaximumLayerCount)
                return false;

            for (const Json& source : *layers)
            {
                if (!source.is_object() || !source.contains("type") ||
                    !source.at("type").is_string())
                    return false;

                const std::string type = source.at("type").get<std::string>();
                const std::string name = source.value("name", std::string{});
                PropertyMap layerProperties;
                if (!properties(source, layerProperties)) return false;

                if (type == "tilelayer")
                {
                    if (source.value("infinite", false) || !source.contains("data") ||
                        !source.at("data").is_array() ||
                        source.at("data").size() != data.cellCount() ||
                        source.value("width", std::uint64_t{0u}) != data.width() ||
                        source.value("height", std::uint64_t{0u}) != data.height() ||
                        source.contains("compression") || source.contains("encoding"))
                        return false;

                    TileMapLayer layer;
                    layer.name = name;
                    layer.role = layerRole(name, layerProperties, false);
                    layer.visible = source.value("visible", true);
                    layer.opacity = source.value("opacity", 1.f);
                    layer.properties = std::move(layerProperties);
                    layer.tiles.resize(data.cellCount(), EmptyTile);

                    for (std::size_t index = 0; index < data.cellCount(); ++index)
                    {
                        const Json& value = source.at("data")[index];
                        if (!value.is_number_unsigned()) return false;
                        const std::uint64_t encoded64 = value.get<std::uint64_t>();
                        if (encoded64 > std::numeric_limits<std::uint32_t>::max()) return false;
                        const std::uint32_t encoded = static_cast<std::uint32_t>(encoded64);
                        if ((encoded & Rotation120) != 0u) return false;

                        const TileId tile = encoded & ~FlipMask;
                        if (tile != EmptyTile && data.definition(tile) == nullptr) return false;

                        TileFlipFlags flags = TileFlipFlags::None;
                        if ((encoded & HorizontalFlip) != 0u)
                            flags = flags | TileFlipFlags::Horizontal;
                        if ((encoded & VerticalFlip) != 0u) flags = flags | TileFlipFlags::Vertical;
                        if ((encoded & DiagonalFlip) != 0u) flags = flags | TileFlipFlags::Diagonal;

                        layer.tiles[index] = tile;
                        if (flags != TileFlipFlags::None)
                        {
                            if (layer.flipFlags.empty())
                                layer.flipFlags.resize(data.cellCount(), TileFlipFlags::None);
                            layer.flipFlags[index] = flags;
                        }
                    }

                    if (!data.addLayer(std::move(layer))) return false;
                }
                else if (type == "objectgroup")
                {
                    const auto objects = source.find("objects");
                    if (objects == source.end() || !objects->is_array()) return false;

                    TileMapLayer marker;
                    marker.name = name;
                    marker.role = TileMapLayerRole::Object;
                    marker.visible = source.value("visible", true);
                    marker.opacity = source.value("opacity", 1.f);
                    marker.properties = std::move(layerProperties);
                    marker.tiles.resize(data.cellCount(), EmptyTile);
                    if (!data.addLayer(std::move(marker))) return false;

                    for (const Json& sourceObject : *objects)
                    {
                        if (!sourceObject.is_object()) return false;
                        TileMapObject object;
                        object.id = sourceObject.value("id", std::uint64_t{0u});
                        object.name = sourceObject.value("name", std::string{});
                        object.type =
                            sourceObject.value("class", sourceObject.value("type", std::string{}));
                        object.position = {sourceObject.value("x", 0.f),
                                           sourceObject.value("y", 0.f)};
                        object.size = {sourceObject.value("width", 0.f),
                                       sourceObject.value("height", 0.f)};
                        object.rotation = sourceObject.value("rotation", 0.f);
                        if (!properties(sourceObject, object.properties) ||
                            !data.addObject(std::move(object)))
                            return false;
                    }
                }
                else
                {
                    return false;
                }
            }

            return true;
        }
        bool loadTiledJson(std::istream& input, TileMapData& output)
        {
            Json root = Json::parse(input, nullptr, false);
            if (root.is_discarded() || !root.is_object() || root.value("infinite", false))
                return false;

            const std::uint64_t width = root.value("width", std::uint64_t{0u});
            const std::uint64_t height = root.value("height", std::uint64_t{0u});
            const std::uint64_t tileWidth = root.value("tilewidth", std::uint64_t{0u});
            const std::uint64_t tileHeight = root.value("tileheight", std::uint64_t{0u});
            const std::string orientation = root.value("orientation", std::string{});
            if (width > std::numeric_limits<std::size_t>::max() ||
                height > std::numeric_limits<std::size_t>::max() || tileWidth == 0u ||
                tileHeight == 0u || tileWidth > std::numeric_limits<unsigned int>::max() ||
                tileHeight > std::numeric_limits<unsigned int>::max() ||
                (orientation != "orthogonal" && orientation != "isometric"))
                return false;

            TileMapData parsed;
            if (!parsed.setDimensions(static_cast<std::size_t>(width),
                                      static_cast<std::size_t>(height)) ||
                !parsed.setTileSize(
                    {static_cast<float>(tileWidth), static_cast<float>(tileHeight)}))
                return false;
            parsed.setOrientation(orientation == "isometric" ? TileMapOrientation::Isometric
                                                             : TileMapOrientation::Orthogonal);
            if (!properties(root, parsed.properties()) || !parseTileDefinitions(root, parsed) ||
                !parseLayers(root, parsed) || !parsed.isValid())
                return false;

            output = std::move(parsed);
            return true;
        }
    }

    bool TiledJsonImporter::load(std::istream& input, TileMapData& output)
    {
        try
        {
            return loadTiledJson(input, output);
        }
        catch (const Json::exception&)
        {
            return false;
        }
    }

    bool TiledJsonImporter::loadFromFile(const std::string& filepath, TileMapData& output)
    {
        std::ifstream input(filepath);
        return input.is_open() && load(input, output);
    }
}
