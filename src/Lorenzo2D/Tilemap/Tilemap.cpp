#include <Lorenzo2D/Tilemap/Tilemap.hpp>
#include <Lorenzo2D/Tilemap/AsciiTileMapImporter.hpp>
#include <Lorenzo2D/Tilemap/TileMapColliderBuilder2D.hpp>

#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Angle.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace l2d
{
    namespace
    {
        constexpr std::size_t VERTICES_PER_TILE = 6;
        constexpr float MIN_TILE_DIMENSION = 0.0001f;

        struct Bounds
        {
            float left = 0.f;
            float top = 0.f;
            float right = 0.f;
            float bottom = 0.f;
        };

        struct RenderChunk
        {
            struct AnimatedTileSpan
            {
                char tile = '\0';
                std::size_t firstVertex = 0;
                std::vector<TileAnimationFrame> frames;
                TileFlipFlags flipFlags = TileFlipFlags::None;
            };

            sf::VertexArray coloredVertices;
            sf::VertexArray texturedVertices;
            Bounds worldBounds;
            std::size_t chunkRow = 0;
            std::size_t chunkColumn = 0;
            std::size_t coloredTileCount = 0;
            std::size_t texturedTileCount = 0;
            std::vector<AnimatedTileSpan> animatedTiles;
            TextureHandle texture;

            RenderChunk(sf::VertexArray&& colored, sf::VertexArray&& textured, Bounds bounds,
                        std::size_t row, std::size_t column, std::size_t coloredCount,
                        std::size_t texturedCount, std::vector<AnimatedTileSpan>&& animated)
                : coloredVertices(std::move(colored)), texturedVertices(std::move(textured)),
                  worldBounds(bounds), chunkRow(row), chunkColumn(column),
                  coloredTileCount(coloredCount), texturedTileCount(texturedCount),
                  animatedTiles(std::move(animated))
            {
            }

            std::size_t tileCount() const
            {
                return coloredTileCount + texturedTileCount;
            }
        };

        void setTextureCoordinates(sf::VertexArray& vertices, std::size_t first,
                                   sf::IntRect rectangle, TileFlipFlags flags = TileFlipFlags::None)
        {
            std::array<sf::Vector2f, 4u> coordinates = {
                sf::Vector2f{0.f, 0.f}, sf::Vector2f{0.f, 1.f}, sf::Vector2f{1.f, 1.f},
                sf::Vector2f{1.f, 0.f}};

            for (sf::Vector2f& coordinate : coordinates)
            {
                if (hasFlag(flags, TileFlipFlags::Diagonal)) std::swap(coordinate.x, coordinate.y);
                if (hasFlag(flags, TileFlipFlags::Horizontal)) coordinate.x = 1.f - coordinate.x;
                if (hasFlag(flags, TileFlipFlags::Vertical)) coordinate.y = 1.f - coordinate.y;

                coordinate.x = static_cast<float>(rectangle.position.x) +
                               coordinate.x * static_cast<float>(rectangle.size.x);
                coordinate.y = static_cast<float>(rectangle.position.y) +
                               coordinate.y * static_cast<float>(rectangle.size.y);
            }

            vertices[first + 0u].texCoords = coordinates[0u];
            vertices[first + 1u].texCoords = coordinates[1u];
            vertices[first + 2u].texCoords = coordinates[2u];
            vertices[first + 3u].texCoords = coordinates[0u];
            vertices[first + 4u].texCoords = coordinates[2u];
            vertices[first + 5u].texCoords = coordinates[3u];
        }

        struct RenderChunkBuilder
        {
            sf::VertexArray coloredVertices{sf::PrimitiveType::Triangles};
            sf::VertexArray texturedVertices{sf::PrimitiveType::Triangles};
            Bounds worldBounds;
            std::size_t coloredTileCount = 0;
            std::size_t texturedTileCount = 0;
            std::vector<RenderChunk::AnimatedTileSpan> animatedTiles;

            std::size_t tileCount() const
            {
                return coloredTileCount + texturedTileCount;
            }
        };

        struct RenderBuild
        {
            std::vector<RenderChunk> chunks;
            std::size_t renderedTileCount = 0;
            std::size_t texturedTileCount = 0;
        };

        struct CollisionRectangle
        {
            std::size_t column = 0;
            std::size_t row = 0;
            std::size_t width = 0;
            std::size_t height = 0;
        };

        struct CollisionGeometry
        {
            sf::Vector2f position;
            sf::Vector2f size;
        };

        struct CullingArea
        {
            Bounds bounds;
            bool enabled = false;
        };

        bool isFinite(float value)
        {
            return std::isfinite(value);
        }

        bool isFinite(const Bounds& bounds)
        {
            return isFinite(bounds.left) && isFinite(bounds.top) && isFinite(bounds.right) &&
                   isFinite(bounds.bottom);
        }

        float checkedTileExtent(std::size_t tileCount, float tileDimension)
        {
            const long double extent =
                static_cast<long double>(tileCount) * static_cast<long double>(tileDimension);
            const long double maximum = static_cast<long double>(std::numeric_limits<float>::max());

            if (!std::isfinite(extent) || extent > maximum)
            {
                throw std::overflow_error("Tile map geometry exceeds the finite coordinate range.");
            }

            const float result = static_cast<float>(extent);

            if (!isFinite(result))
            {
                throw std::overflow_error("Tile map geometry exceeds the finite coordinate range.");
            }

            return result;
        }

        CullingArea makeCullingArea(const sf::View& view)
        {
            const sf::Vector2f center = view.getCenter();
            const sf::Vector2f size = view.getSize();
            const float rotation = view.getRotation().asRadians();

            if (!isFinite(center.x) || !isFinite(center.y) || !isFinite(size.x) ||
                !isFinite(size.y) || !isFinite(rotation) || size.x <= 0.f || size.y <= 0.f)
            {
                return {};
            }

            const float halfWidth = size.x * 0.5f;
            const float halfHeight = size.y * 0.5f;
            const float cosine = std::abs(std::cos(rotation));
            const float sine = std::abs(std::sin(rotation));
            const float horizontalExtent = cosine * halfWidth + sine * halfHeight;
            const float verticalExtent = sine * halfWidth + cosine * halfHeight;

            CullingArea area;
            area.bounds = {center.x - horizontalExtent, center.y - verticalExtent,
                           center.x + horizontalExtent, center.y + verticalExtent};
            area.enabled = isFinite(area.bounds);
            return area;
        }

        bool intersects(const Bounds& first, const Bounds& second)
        {
            // Treat touching edges as visible. The extra boundary draw is
            // preferable to a one-frame pop caused by floating-point drift.
            return first.left <= second.right && first.right >= second.left &&
                   first.top <= second.bottom && first.bottom >= second.top;
        }

        bool projectedBounds(const Bounds& worldBounds,
                             const CoordinateProjection2D& projection, Bounds& output)
        {
            sf::Vector2f renderMinimum;
            sf::Vector2f renderMaximum;
            if (!projection.projectBounds({worldBounds.left, worldBounds.top},
                                          {worldBounds.right, worldBounds.bottom},
                                          renderMinimum, renderMaximum))
            {
                return false;
            }

            output = {renderMinimum.x, renderMinimum.y, renderMaximum.x, renderMaximum.y};
            return isFinite(output) && output.left <= output.right && output.top <= output.bottom;
        }

        bool isVisible(const RenderChunk& chunk, const CullingArea& cullingArea,
                       const CoordinateProjection2D* projection = nullptr)
        {
            if (!cullingArea.enabled) return true;
            if (projection == nullptr || projection->isIdentity())
                return intersects(chunk.worldBounds, cullingArea.bounds);

            Bounds renderBounds;
            if (!projectedBounds(chunk.worldBounds, *projection, renderBounds))
            {
                // Unknown projection bounds stay visible. Culling must never
                // trade correctness for an optimization.
                return true;
            }

            return intersects(renderBounds, cullingArea.bounds);
        }

        void appendVertex(sf::VertexArray& vertices, sf::Vector2f position, sf::Color color,
                          sf::Vector2f textureCoordinate = {0.f, 0.f})
        {
            sf::Vertex vertex;
            vertex.position = position;
            vertex.color = color;
            vertex.texCoords = textureCoordinate;
            vertices.append(vertex);
        }

        void appendTile(RenderChunkBuilder& chunk, std::size_t column, std::size_t row, char tile,
                        sf::Vector2f tileSize, sf::Color color, const TileSet& tileSet,
                        const std::optional<sf::IntRect>& textureRect,
                        const std::vector<TileAnimationFrame>* animation = nullptr,
                        TileFlipFlags flipFlags = TileFlipFlags::None)
        {
            const float left = checkedTileExtent(column, tileSize.x);
            const float top = checkedTileExtent(row, tileSize.y);
            const float right = checkedTileExtent(column + 1u, tileSize.x);
            const float bottom = checkedTileExtent(row + 1u, tileSize.y);

            if (right <= left || bottom <= top)
            {
                throw std::overflow_error(
                    "Tile map cells are too small for their world coordinates.");
            }

            sf::VertexArray& vertices =
                textureRect ? chunk.texturedVertices : chunk.coloredVertices;

            if (textureRect)
            {
                if (tileSet.isAnimated(tile))
                {
                    chunk.animatedTiles.push_back({tile, vertices.getVertexCount(), {}, flipFlags});
                }
                else if (animation != nullptr && !animation->empty())
                {
                    chunk.animatedTiles.push_back(
                        {'\0', vertices.getVertexCount(), *animation, flipFlags});
                }

                const std::size_t firstVertex = vertices.getVertexCount();
                for (const sf::Vector2f position :
                     {sf::Vector2f{left, top}, sf::Vector2f{left, bottom},
                      sf::Vector2f{right, bottom}, sf::Vector2f{left, top},
                      sf::Vector2f{right, bottom}, sf::Vector2f{right, top}})
                    appendVertex(vertices, position, color);
                setTextureCoordinates(vertices, firstVertex, *textureRect, flipFlags);
                chunk.texturedTileCount++;
            }
            else
            {
                appendVertex(vertices, {left, top}, color);
                appendVertex(vertices, {left, bottom}, color);
                appendVertex(vertices, {right, bottom}, color);
                appendVertex(vertices, {left, top}, color);
                appendVertex(vertices, {right, bottom}, color);
                appendVertex(vertices, {right, top}, color);
                chunk.coloredTileCount++;
            }

            if (chunk.tileCount() == 1u)
            {
                chunk.worldBounds = {left, top, right, bottom};
            }
            else
            {
                chunk.worldBounds.left = std::min(chunk.worldBounds.left, left);
                chunk.worldBounds.top = std::min(chunk.worldBounds.top, top);
                chunk.worldBounds.right = std::max(chunk.worldBounds.right, right);
                chunk.worldBounds.bottom = std::max(chunk.worldBounds.bottom, bottom);
            }
        }

        std::optional<RenderChunk> buildRenderChunk(const TileMap::Layout& layout, char solidChar,
                                                    sf::Vector2f tileSize, sf::Vector2u chunkSize,
                                                    sf::Color color, const TileSet& tileSet,
                                                    std::size_t chunkRow, std::size_t chunkColumn)
        {
            RenderChunkBuilder builder;
            const std::size_t firstRow = chunkRow * static_cast<std::size_t>(chunkSize.y);
            const std::size_t firstColumn = chunkColumn * static_cast<std::size_t>(chunkSize.x);
            const std::size_t rowEnd =
                std::min(layout.size(), firstRow + static_cast<std::size_t>(chunkSize.y));

            for (std::size_t row = firstRow; row < rowEnd; ++row)
            {
                const std::size_t columnEnd = std::min(
                    layout[row].size(), firstColumn + static_cast<std::size_t>(chunkSize.x));

                for (std::size_t column = firstColumn; column < columnEnd; ++column)
                {
                    const char tile = layout[row][column];
                    const std::optional<sf::IntRect> textureRect = tileSet.textureRect(tile);

                    if (!textureRect && tile != solidChar) continue;

                    appendTile(builder, column, row, tile, tileSize,
                               textureRect ? sf::Color::White : color, tileSet, textureRect);
                }
            }

            if (builder.tileCount() == 0u) return std::nullopt;

            return RenderChunk(std::move(builder.coloredVertices),
                               std::move(builder.texturedVertices), builder.worldBounds, chunkRow,
                               chunkColumn, builder.coloredTileCount, builder.texturedTileCount,
                               std::move(builder.animatedTiles));
        }

        RenderBuild buildRenderChunks(const TileMap::Layout& layout, char solidChar,
                                      sf::Vector2f tileSize, sf::Vector2u chunkSize,
                                      sf::Color color, const TileSet& tileSet)
        {
            using ChunkCoordinate = std::pair<std::size_t, std::size_t>;
            std::map<ChunkCoordinate, RenderChunkBuilder> builders;

            for (std::size_t row = 0; row < layout.size(); ++row)
            {
                for (std::size_t column = 0; column < layout[row].size(); ++column)
                {
                    const char tile = layout[row][column];
                    const std::optional<sf::IntRect> textureRect = tileSet.textureRect(tile);

                    if (!textureRect && tile != solidChar) continue;

                    const ChunkCoordinate coordinate = {row / static_cast<std::size_t>(chunkSize.y),
                                                        column /
                                                            static_cast<std::size_t>(chunkSize.x)};

                    appendTile(builders[coordinate], column, row, tile, tileSize,
                               textureRect ? sf::Color::White : color, tileSet, textureRect);
                }
            }

            RenderBuild build;
            build.chunks.reserve(builders.size());

            for (auto& entry : builders)
            {
                RenderChunkBuilder& builder = entry.second;
                build.renderedTileCount += builder.tileCount();
                build.texturedTileCount += builder.texturedTileCount;
                build.chunks.emplace_back(std::move(builder.coloredVertices),
                                          std::move(builder.texturedVertices), builder.worldBounds,
                                          entry.first.first, entry.first.second,
                                          builder.coloredTileCount, builder.texturedTileCount,
                                          std::move(builder.animatedTiles));
            }

            return build;
        }

        std::optional<RenderBuild> buildDataRenderChunks(const TileMapData& data,
                                                         sf::Vector2u chunkSize, sf::Color color,
                                                         const AssetManager* assets)
        {
            using ChunkCoordinate = std::tuple<std::size_t, std::size_t, std::size_t, AssetId>;
            std::map<ChunkCoordinate, RenderChunkBuilder> builders;
            std::map<AssetId, TextureHandle> textures;
            const TileSet emptyTileSet;

            for (std::size_t layerIndex = 0u; layerIndex < data.layers().size(); ++layerIndex)
            {
                const TileMapLayer& layer = data.layers()[layerIndex];
                if (!layer.visible || layer.role == TileMapLayerRole::Collision ||
                    layer.role == TileMapLayerRole::Trigger ||
                    layer.role == TileMapLayerRole::Navigation ||
                    layer.role == TileMapLayerRole::Object)
                {
                    continue;
                }

                sf::Color layerColor = color;
                layerColor.a = static_cast<std::uint8_t>(
                    std::round(static_cast<float>(color.a) * layer.opacity));

                for (std::size_t index = 0; index < layer.tiles.size(); ++index)
                {
                    const TileId tile = layer.tiles[index];
                    if (tile == EmptyTile) continue;

                    const TileDefinition* definition = data.definition(tile);
                    if (definition == nullptr) return std::nullopt;

                    TextureHandle texture;
                    if (!definition->texture.empty())
                    {
                        if (assets == nullptr) return std::nullopt;
                        const auto cached = textures.find(definition->texture);
                        if (cached == textures.end())
                        {
                            texture = assets->getTexture(definition->texture);
                            if (!texture) return std::nullopt;
                            textures.emplace(definition->texture, texture);
                        }
                        else
                        {
                            texture = cached->second;
                        }
                    }

                    const std::size_t row = index / data.width();
                    const std::size_t column = index % data.width();
                    const ChunkCoordinate coordinate = {
                        layerIndex, row / static_cast<std::size_t>(chunkSize.y),
                        column / static_cast<std::size_t>(chunkSize.x), definition->texture};
                    const std::optional<sf::IntRect> textureRect =
                        definition->texture.empty()
                            ? std::nullopt
                            : std::optional<sf::IntRect>(definition->textureRect);
                    const TileFlipFlags flipFlags =
                        layer.flipFlags.empty() ? TileFlipFlags::None : layer.flipFlags[index];
                    appendTile(builders[coordinate], column, row, '\0', data.tileSize(),
                               texture ? sf::Color(255u, 255u, 255u, layerColor.a) : layerColor,
                               emptyTileSet, textureRect, &definition->animation, flipFlags);
                }
            }

            RenderBuild build;
            build.chunks.reserve(builders.size());

            for (auto& entry : builders)
            {
                RenderChunkBuilder& builder = entry.second;
                build.renderedTileCount += builder.tileCount();
                build.texturedTileCount += builder.texturedTileCount;
                build.chunks.emplace_back(std::move(builder.coloredVertices),
                                          std::move(builder.texturedVertices), builder.worldBounds,
                                          std::get<1u>(entry.first), std::get<2u>(entry.first),
                                          builder.coloredTileCount, builder.texturedTileCount,
                                          std::move(builder.animatedTiles));
                const auto texture = textures.find(std::get<3u>(entry.first));
                if (texture != textures.end()) build.chunks.back().texture = texture->second;
            }

            return std::optional<RenderBuild>(std::move(build));
        }

        std::vector<CollisionRectangle> buildCollisionRectangles(const TileMap::Layout& layout,
                                                                 char solidChar)
        {
            std::vector<std::vector<bool>> consumed;
            consumed.reserve(layout.size());

            for (const std::string& row : layout)
            {
                consumed.emplace_back(row.size(), false);
            }

            std::vector<CollisionRectangle> rectangles;

            for (std::size_t row = 0; row < layout.size(); ++row)
            {
                for (std::size_t column = 0; column < layout[row].size(); ++column)
                {
                    if (layout[row][column] != solidChar || consumed[row][column])
                    {
                        continue;
                    }

                    std::size_t width = 0;

                    while (column + width < layout[row].size() &&
                           layout[row][column + width] == solidChar &&
                           !consumed[row][column + width])
                    {
                        width++;
                    }

                    std::size_t height = 1;

                    while (row + height < layout.size())
                    {
                        bool canExtend = true;

                        for (std::size_t offset = 0; offset < width; ++offset)
                        {
                            const std::size_t candidateColumn = column + offset;

                            if (candidateColumn >= layout[row + height].size() ||
                                layout[row + height][candidateColumn] != solidChar ||
                                consumed[row + height][candidateColumn])
                            {
                                canExtend = false;
                                break;
                            }
                        }

                        if (!canExtend) break;

                        height++;
                    }

                    for (std::size_t consumedRow = row; consumedRow < row + height; ++consumedRow)
                    {
                        for (std::size_t consumedColumn = column; consumedColumn < column + width;
                             ++consumedColumn)
                        {
                            consumed[consumedRow][consumedColumn] = true;
                        }
                    }

                    rectangles.push_back({column, row, width, height});
                }
            }

            return rectangles;
        }

        std::vector<CollisionGeometry> buildCollisionGeometry(
            const std::vector<CollisionRectangle>& rectangles, sf::Vector2f tileSize)
        {
            std::vector<CollisionGeometry> geometry;
            geometry.reserve(rectangles.size());

            for (const CollisionRectangle& rectangle : rectangles)
            {
                CollisionGeometry item;
                const float right =
                    checkedTileExtent(rectangle.column + rectangle.width, tileSize.x);
                const float bottom =
                    checkedTileExtent(rectangle.row + rectangle.height, tileSize.y);
                item.position = {checkedTileExtent(rectangle.column, tileSize.x),
                                 checkedTileExtent(rectangle.row, tileSize.y)};
                item.size = {right - item.position.x, bottom - item.position.y};

                const sf::Vector2f maximum = item.position + item.size;

                if (!isFinite(item.size.x) || !isFinite(item.size.y) ||
                    item.size.x < MIN_TILE_DIMENSION || item.size.y < MIN_TILE_DIMENSION ||
                    maximum.x != right || maximum.y != bottom)
                {
                    throw std::overflow_error("Tile map collision geometry cannot be represented "
                                              "without gaps.");
                }

                geometry.push_back(item);
            }

            return geometry;
        }

        class TileMapRenderComponent final : public Component
        {
          public:
            TileMapRenderComponent(std::vector<RenderChunk>&& chunks, TextureHandle texture,
                                   TileSet tileSet, sf::Vector2u chunkSize,
                                   std::size_t solidTileCount)
                : m_chunks(std::move(chunks)), m_texture(std::move(texture)),
                  m_tileSet(std::move(tileSet)), m_chunkSize(chunkSize),
                  m_solidTileCount(solidTileCount)
            {
            }

            void replaceChunk(std::size_t chunkRow, std::size_t chunkColumn,
                              std::optional<RenderChunk> replacement)
            {
                const auto iterator = std::find_if(
                    m_chunks.begin(), m_chunks.end(),
                    [chunkRow, chunkColumn](const RenderChunk& chunk)
                    { return chunk.chunkRow == chunkRow && chunk.chunkColumn == chunkColumn; });

                if (iterator != m_chunks.end())
                {
                    if (replacement)
                        *iterator = std::move(*replacement);
                    else
                        m_chunks.erase(iterator);
                }
                else if (replacement)
                {
                    m_chunks.push_back(std::move(*replacement));
                }

                std::sort(m_chunks.begin(), m_chunks.end(),
                          [](const RenderChunk& left, const RenderChunk& right)
                          {
                              if (left.chunkRow != right.chunkRow)
                                  return left.chunkRow < right.chunkRow;
                              return left.chunkColumn < right.chunkColumn;
                          });
            }

            void setSolidTileCount(std::size_t count)
            {
                m_solidTileCount = count;
            }

            std::size_t chunkCount() const
            {
                return m_chunks.size();
            }

            void setStreamRegion(std::optional<TileMapRegion> region)
            {
                m_streamRegion = region;
            }

            std::optional<TileMapRegion> streamRegion() const
            {
                return m_streamRegion;
            }

            TileMapRenderStats statsForView(const sf::View& view) const
            {
                return statsForView(view, RenderContext2D{});
            }

            TileMapRenderStats statsForView(const sf::View& view,
                                            const RenderContext2D& context) const
            {
                const CullingArea cullingArea = makeCullingArea(view);
                const CoordinateProjection2D* projection =
                    context.pass == RenderPass2D::UI ? nullptr : context.projection;

                TileMapRenderStats stats = baseStats();

                for (const RenderChunk& chunk : m_chunks)
                {
                    if (!isResident(chunk))
                    {
                        stats.nonResidentChunkCount++;
                        stats.culledChunkCount++;
                        continue;
                    }

                    stats.residentChunkCount++;

                    if (isVisible(chunk, cullingArea, projection))
                    {
                        addVisibleChunk(stats, chunk);
                    }
                    else
                    {
                        stats.culledChunkCount++;
                    }
                }

                return stats;
            }

            TileMapRenderStats lastRenderStats() const
            {
                return m_lastRenderStats;
            }

            void onUpdate(float deltaTime) override
            {
                if (!std::isfinite(deltaTime) || deltaTime <= 0.f) return;

                const double nextAnimationTime =
                    static_cast<double>(m_animationTime) + static_cast<double>(deltaTime);
                m_animationTime = static_cast<float>(std::fmod(nextAnimationTime, 86400.0));

                for (RenderChunk& chunk : m_chunks)
                {
                    for (const RenderChunk::AnimatedTileSpan& animated : chunk.animatedTiles)
                    {
                        std::optional<sf::IntRect> rectangle;
                        if (!animated.frames.empty())
                        {
                            float duration = 0.f;
                            for (const TileAnimationFrame& frame : animated.frames)
                                duration += frame.duration;

                            float time = std::fmod(m_animationTime, duration);
                            for (const TileAnimationFrame& frame : animated.frames)
                            {
                                if (time < frame.duration)
                                {
                                    rectangle = frame.textureRect;
                                    break;
                                }
                                time -= frame.duration;
                            }
                            if (!rectangle) rectangle = animated.frames.back().textureRect;
                        }
                        else
                        {
                            rectangle = m_tileSet.textureRect(animated.tile, m_animationTime);
                        }

                        if (!rectangle || animated.firstVertex + VERTICES_PER_TILE >
                                              chunk.texturedVertices.getVertexCount())
                        {
                            continue;
                        }

                        setTextureCoordinates(chunk.texturedVertices, animated.firstVertex,
                                              *rectangle, animated.flipFlags);
                    }
                }
            }

            void onRender(sf::RenderWindow& window) override
            {
                onRender(window, 1.f);
            }

            void onRender(sf::RenderWindow& window, float interpolationAlpha) override
            {
                onRender(window, RenderContext2D{interpolationAlpha});
            }

            void onRender(sf::RenderWindow& window, const RenderContext2D& context) override
            {
                const CullingArea cullingArea = makeCullingArea(window.getView());
                const bool projectGeometry = context.pass != RenderPass2D::UI &&
                                             context.projection != nullptr &&
                                             !context.projection->isIdentity();

                TileMapRenderStats stats = baseStats();

                const auto drawVertices =
                    [&](const sf::VertexArray& source, const sf::Texture* texture)
                {
                    sf::RenderStates states;
                    states.texture = texture;

                    if (!projectGeometry)
                    {
                        window.draw(source, states);
                        return;
                    }

                    sf::VertexArray projected = source;

                    for (std::size_t index = 0; index < projected.getVertexCount(); ++index)
                    {
                        projected[index].position =
                            context.worldToRender(projected[index].position);
                    }

                    window.draw(projected, states);
                };

                for (const RenderChunk& chunk : m_chunks)
                {
                    if (!isResident(chunk))
                    {
                        stats.nonResidentChunkCount++;
                        stats.culledChunkCount++;
                        continue;
                    }

                    stats.residentChunkCount++;

                    const CoordinateProjection2D* cullingProjection =
                        projectGeometry ? context.projection : nullptr;
                    if (!isVisible(chunk, cullingArea, cullingProjection))
                    {
                        stats.culledChunkCount++;
                        continue;
                    }

                    if (chunk.coloredTileCount > 0u)
                    {
                        drawVertices(chunk.coloredVertices, nullptr);
                    }

                    if (chunk.texturedTileCount > 0u)
                    {
                        const sf::Texture* texture =
                            chunk.texture ? chunk.texture.get() : m_texture.get();
                        drawVertices(chunk.texturedVertices, texture);
                    }

                    addVisibleChunk(stats, chunk);
                }

                m_lastRenderStats = stats;
            }

          private:
            TileMapRenderStats baseStats() const
            {
                TileMapRenderStats stats;
                stats.chunkCount = m_chunks.size();
                stats.solidTileCount = m_solidTileCount;
                return stats;
            }

            bool isResident(const RenderChunk& chunk) const
            {
                if (!m_streamRegion) return true;

                const std::size_t chunkFirstColumn =
                    chunk.chunkColumn * static_cast<std::size_t>(m_chunkSize.x);
                const std::size_t chunkFirstRow =
                    chunk.chunkRow * static_cast<std::size_t>(m_chunkSize.y);
                const std::size_t chunkLastColumn =
                    chunkFirstColumn + static_cast<std::size_t>(m_chunkSize.x);
                const std::size_t chunkLastRow =
                    chunkFirstRow + static_cast<std::size_t>(m_chunkSize.y);
                const std::size_t regionLastColumn =
                    m_streamRegion->firstColumn + m_streamRegion->columnCount;
                const std::size_t regionLastRow =
                    m_streamRegion->firstRow + m_streamRegion->rowCount;

                return chunkFirstColumn < regionLastColumn &&
                       chunkLastColumn > m_streamRegion->firstColumn &&
                       chunkFirstRow < regionLastRow && chunkLastRow > m_streamRegion->firstRow;
            }

            static void addVisibleChunk(TileMapRenderStats& stats, const RenderChunk& chunk)
            {
                stats.visibleChunkCount++;
                stats.submittedTileCount += chunk.tileCount();
                stats.submittedVertexCount += chunk.tileCount() * VERTICES_PER_TILE;
                stats.drawCallCount += static_cast<std::size_t>(chunk.coloredTileCount > 0u);
                stats.drawCallCount += static_cast<std::size_t>(chunk.texturedTileCount > 0u);
            }

          private:
            std::vector<RenderChunk> m_chunks;
            TextureHandle m_texture;
            TileSet m_tileSet;
            sf::Vector2u m_chunkSize;
            std::size_t m_solidTileCount = 0;
            float m_animationTime = 0.f;
            std::optional<TileMapRegion> m_streamRegion;
            TileMapRenderStats m_lastRenderStats;
        };
    }

    TileMap::TileMap()
        : m_tileSize(40.f, 40.f), m_loadedTileSize(0.f, 0.f), m_renderChunkSize(16u, 16u),
          m_loadedRenderChunkSize(0u, 0u), m_solidTileColor(sf::Color::White),
          m_loadedSolidTileColor(sf::Color::White), m_worldSize(0.f, 0.f)
    {
    }

    TileMap::~TileMap()
    {
        unload();
    }

    TileMap::TileMap(TileMap&& other) noexcept
        : m_tileSize(other.m_tileSize), m_loadedTileSize(other.m_loadedTileSize),
          m_renderChunkSize(other.m_renderChunkSize),
          m_loadedRenderChunkSize(other.m_loadedRenderChunkSize),
          m_solidTileColor(other.m_solidTileColor),
          m_loadedSolidTileColor(other.m_loadedSolidTileColor),
          m_tileSet(std::move(other.m_tileSet)), m_loadedTileSet(std::move(other.m_loadedTileSet)),
          m_worldSize(other.m_worldSize), m_buildStats(other.m_buildStats),
          m_lastUpdateStats(other.m_lastUpdateStats), m_layout(std::move(other.m_layout)),
          m_data(std::move(other.m_data)), m_generatedObjects(std::move(other.m_generatedObjects)),
          m_collisionObjects(std::move(other.m_collisionObjects)),
          m_renderObject(std::move(other.m_renderObject)),
          m_sceneState(std::move(other.m_sceneState)), m_loadedSolidChar(other.m_loadedSolidChar),
          m_loadedObjectPrefix(std::move(other.m_loadedObjectPrefix)),
          m_streamRegion(other.m_streamRegion), m_legacyEditMode(other.m_legacyEditMode)
    {
        other.m_loadedTileSize = {0.f, 0.f};
        other.m_loadedRenderChunkSize = {0u, 0u};
        other.m_worldSize = {0.f, 0.f};
        other.m_buildStats = {};
        other.m_lastUpdateStats = {};
        other.m_layout.clear();
        other.m_data.clear();
        other.m_generatedObjects.clear();
        other.m_collisionObjects.clear();
        other.m_renderObject.reset();
        other.m_sceneState.reset();
        other.m_streamRegion.reset();
        other.m_legacyEditMode = false;
    }

    TileMap& TileMap::operator=(TileMap&& other) noexcept
    {
        if (this == &other) return *this;

        unload();

        m_tileSize = other.m_tileSize;
        m_loadedTileSize = other.m_loadedTileSize;
        m_renderChunkSize = other.m_renderChunkSize;
        m_loadedRenderChunkSize = other.m_loadedRenderChunkSize;
        m_solidTileColor = other.m_solidTileColor;
        m_loadedSolidTileColor = other.m_loadedSolidTileColor;
        m_tileSet = std::move(other.m_tileSet);
        m_loadedTileSet = std::move(other.m_loadedTileSet);
        m_worldSize = other.m_worldSize;
        m_buildStats = other.m_buildStats;
        m_lastUpdateStats = other.m_lastUpdateStats;
        m_layout = std::move(other.m_layout);
        m_data = std::move(other.m_data);
        m_generatedObjects = std::move(other.m_generatedObjects);
        m_collisionObjects = std::move(other.m_collisionObjects);
        m_renderObject = std::move(other.m_renderObject);
        m_sceneState = std::move(other.m_sceneState);
        m_loadedSolidChar = other.m_loadedSolidChar;
        m_loadedObjectPrefix = std::move(other.m_loadedObjectPrefix);
        m_streamRegion = other.m_streamRegion;
        m_legacyEditMode = other.m_legacyEditMode;

        other.m_loadedTileSize = {0.f, 0.f};
        other.m_loadedRenderChunkSize = {0u, 0u};
        other.m_worldSize = {0.f, 0.f};
        other.m_buildStats = {};
        other.m_lastUpdateStats = {};
        other.m_layout.clear();
        other.m_data.clear();
        other.m_generatedObjects.clear();
        other.m_collisionObjects.clear();
        other.m_renderObject.reset();
        other.m_sceneState.reset();
        other.m_streamRegion.reset();
        other.m_legacyEditMode = false;

        return *this;
    }

    void TileMap::setTileSize(sf::Vector2f tileSize)
    {
        if (!std::isfinite(tileSize.x) || tileSize.x <= 0.f)
            tileSize.x = 1.f;
        else if (tileSize.x < MIN_TILE_DIMENSION)
            tileSize.x = MIN_TILE_DIMENSION;

        if (!std::isfinite(tileSize.y) || tileSize.y <= 0.f)
            tileSize.y = 1.f;
        else if (tileSize.y < MIN_TILE_DIMENSION)
            tileSize.y = MIN_TILE_DIMENSION;

        m_tileSize = tileSize;
    }

    const sf::Vector2f& TileMap::tileSize() const
    {
        return m_tileSize;
    }

    const sf::Vector2f& TileMap::loadedTileSize() const
    {
        return m_loadedTileSize;
    }

    void TileMap::setRenderChunkSize(sf::Vector2u chunkSize)
    {
        if (chunkSize.x == 0u) chunkSize.x = 1u;

        if (chunkSize.y == 0u) chunkSize.y = 1u;

        m_renderChunkSize = chunkSize;
    }

    const sf::Vector2u& TileMap::renderChunkSize() const
    {
        return m_renderChunkSize;
    }

    const sf::Vector2u& TileMap::loadedRenderChunkSize() const
    {
        return m_loadedRenderChunkSize;
    }

    void TileMap::setSolidTileColor(sf::Color color)
    {
        m_solidTileColor = color;
    }

    sf::Color TileMap::solidTileColor() const
    {
        return m_solidTileColor;
    }

    void TileMap::setTileSet(TileSet tileSet)
    {
        m_tileSet = std::move(tileSet);
    }

    const TileSet& TileMap::tileSet() const
    {
        return m_tileSet;
    }

    const TileSet& TileMap::loadedTileSet() const
    {
        return m_loadedTileSet;
    }

    void TileMap::loadFromLayout(Scene& scene, const Layout& layout, char solidChar,
                                 const std::string& objectPrefix)
    {
        Layout newLayout = layout;
        const sf::Vector2f newLoadedTileSize = m_tileSize;
        TileMapData newData;
        const bool hasCells = std::any_of(newLayout.begin(), newLayout.end(),
                                          [](const std::string& row) { return !row.empty(); });
        if (hasCells &&
            !AsciiTileMapImporter::importLegacy(newLayout, newLoadedTileSize, solidChar, newData))
        {
            throw std::invalid_argument("Tile map layout cannot be represented as layered data.");
        }
        const sf::Vector2u newLoadedRenderChunkSize = m_renderChunkSize;
        const sf::Color newLoadedSolidTileColor = m_solidTileColor;
        TileSet newLoadedTileSet = m_tileSet;

        std::size_t maxColumns = 0;
        std::size_t solidTileCount = 0;
        std::size_t renderedTileCount = 0;

        for (const std::string& row : newLayout)
        {
            maxColumns = std::max(maxColumns, row.size());
            const std::size_t rowSolidTileCount =
                static_cast<std::size_t>(std::count(row.begin(), row.end(), solidChar));

            if (rowSolidTileCount > std::numeric_limits<std::size_t>::max() - solidTileCount)
            {
                throw std::length_error("Tile map contains too many solid tiles.");
            }

            solidTileCount += rowSolidTileCount;

            for (char tile : row)
            {
                if (tile != solidChar && !newLoadedTileSet.contains(tile)) continue;

                if (renderedTileCount == std::numeric_limits<std::size_t>::max())
                {
                    throw std::length_error("Tile map contains too many rendered tiles.");
                }

                ++renderedTileCount;
            }
        }

        if (renderedTileCount > std::numeric_limits<std::size_t>::max() / VERTICES_PER_TILE)
        {
            throw std::length_error("Tile map contains too many tiles to render.");
        }

        const sf::Vector2f newWorldSize = {
            checkedTileExtent(maxColumns, newLoadedTileSize.x),
            checkedTileExtent(newLayout.size(), newLoadedTileSize.y)};

        RenderBuild renderBuild =
            buildRenderChunks(newLayout, solidChar, newLoadedTileSize, newLoadedRenderChunkSize,
                              newLoadedSolidTileColor, newLoadedTileSet);
        const std::vector<CollisionRectangle> collisionRectangles =
            buildCollisionRectangles(newLayout, solidChar);
        const std::vector<CollisionGeometry> collisionGeometry =
            buildCollisionGeometry(collisionRectangles, newLoadedTileSize);

        TileMapBuildStats newBuildStats;
        newBuildStats.solidTileCount = solidTileCount;
        newBuildStats.renderedTileCount = renderBuild.renderedTileCount;
        newBuildStats.texturedTileCount = renderBuild.texturedTileCount;
        newBuildStats.renderChunkCount = renderBuild.chunks.size();
        newBuildStats.collisionRectangleCount = collisionRectangles.size();

        std::vector<GameObjectHandle> newGeneratedObjects;
        std::vector<GameObjectHandle> newCollisionObjects;
        newGeneratedObjects.reserve(collisionRectangles.size() +
                                    (renderBuild.chunks.empty() ? 0u : 1u));
        GameObjectHandle newRenderObject;

        try
        {
            if (!renderBuild.chunks.empty())
            {
                GameObject& renderObject = scene.createGameObject(objectPrefix + "_Render");
                newRenderObject = scene.createHandle(renderObject);
                newGeneratedObjects.push_back(newRenderObject);
                renderObject.addComponent<TileMapRenderComponent>(
                    std::move(renderBuild.chunks), newLoadedTileSet.texture(), newLoadedTileSet,
                    newLoadedRenderChunkSize, solidTileCount);
            }

            for (std::size_t index = 0; index < collisionRectangles.size(); ++index)
            {
                const CollisionGeometry& geometry = collisionGeometry[index];

                GameObject& collisionObject =
                    scene.createGameObject(objectPrefix + "_Collision_" + std::to_string(index));
                const GameObjectHandle collisionHandle = scene.createHandle(collisionObject);
                newGeneratedObjects.push_back(collisionHandle);
                newCollisionObjects.push_back(collisionHandle);
                collisionObject.transform.setPosition(geometry.position);
                BoxCollider2D& collider =
                    collisionObject.addComponent<BoxCollider2D>(geometry.size);
                collider.setOffset(geometry.size * 0.5f);
            }
        }
        catch (...)
        {
            queueGeneratedObjectsForDestruction(newGeneratedObjects);
            throw;
        }

        queueGeneratedObjectsForDestruction(m_generatedObjects);

        m_layout = std::move(newLayout);
        m_data = std::move(newData);
        m_loadedTileSize = newLoadedTileSize;
        m_loadedRenderChunkSize = newLoadedRenderChunkSize;
        m_loadedSolidTileColor = newLoadedSolidTileColor;
        m_loadedTileSet = std::move(newLoadedTileSet);
        m_worldSize = newWorldSize;
        m_buildStats = newBuildStats;
        m_lastUpdateStats = {};
        m_generatedObjects = std::move(newGeneratedObjects);
        m_collisionObjects = std::move(newCollisionObjects);
        m_renderObject = std::move(newRenderObject);
        m_sceneState = scene.m_handleState;
        m_loadedSolidChar = solidChar;
        m_loadedObjectPrefix = objectPrefix;
        m_streamRegion.reset();
        m_legacyEditMode = true;
    }

    bool TileMap::loadFromData(Scene& scene, const TileMapData& data,
                               const std::string& objectPrefix)
    {
        return loadFromData(scene, data, static_cast<const AssetManager*>(nullptr), objectPrefix);
    }

    bool TileMap::loadFromData(Scene& scene, const TileMapData& data, AssetManager& assets,
                               const std::string& objectPrefix)
    {
        return loadFromData(scene, data, &assets, objectPrefix);
    }

    bool TileMap::loadFromData(Scene& scene, const TileMapData& data, const AssetManager* assets,
                               const std::string& objectPrefix)
    {
        if (!data.isValid()) return false;

        Layout composited(data.height(), std::string(data.width(), '.'));
        constexpr char solidCell = '#';

        for (const TileMapLayer& layer : data.layers())
        {
            if (layer.role == TileMapLayerRole::Trigger ||
                layer.role == TileMapLayerRole::Navigation ||
                layer.role == TileMapLayerRole::Object)
                continue;

            for (std::size_t index = 0; index < layer.tiles.size(); ++index)
            {
                const TileId tile = layer.tiles[index];
                if (tile == EmptyTile) continue;

                const TileDefinition* definition = data.definition(tile);
                const bool solid =
                    layer.role == TileMapLayerRole::Collision ||
                    (definition != nullptr && definition->collision == TileCollisionKind::Solid);
                const std::size_t row = index / data.width();
                const std::size_t column = index % data.width();

                if (solid) composited[row][column] = solidCell;
            }
        }

        std::optional<RenderBuild> built =
            buildDataRenderChunks(data, m_renderChunkSize, m_solidTileColor, assets);
        if (!built) return false;
        RenderBuild renderBuild = std::move(*built);
        const std::size_t renderedTileCount = renderBuild.renderedTileCount;
        const std::size_t texturedTileCount = renderBuild.texturedTileCount;
        const std::size_t renderChunkCount = renderBuild.chunks.size();
        const std::vector<TileMapCollisionRectangle2D> collisionRectangles =
            TileMapColliderBuilder2D::build(data);

        std::size_t solidTileCount = 0u;
        for (const TileMapCollisionRectangle2D& rectangle : collisionRectangles)
        {
            if (rectangle.columnCount >
                    std::numeric_limits<std::size_t>::max() / rectangle.rowCount ||
                rectangle.columnCount * rectangle.rowCount >
                    std::numeric_limits<std::size_t>::max() - solidTileCount)
                return false;
            solidTileCount += rectangle.columnCount * rectangle.rowCount;
        }

        std::vector<GameObjectHandle> generatedObjects;
        std::vector<GameObjectHandle> collisionObjects;
        GameObjectHandle renderObject;

        try
        {
            if (!renderBuild.chunks.empty())
            {
                GameObject& object = scene.createGameObject(objectPrefix + "_Render");
                renderObject = scene.createHandle(object);
                generatedObjects.push_back(renderObject);
                object.addComponent<TileMapRenderComponent>(std::move(renderBuild.chunks),
                                                            TextureHandle{}, TileSet{},
                                                            m_renderChunkSize, solidTileCount);
            }

            for (std::size_t index = 0; index < collisionRectangles.size(); ++index)
            {
                const TileMapCollisionRectangle2D& rectangle = collisionRectangles[index];
                GameObject& object =
                    scene.createGameObject(objectPrefix + "_Collision_" + std::to_string(index));
                const GameObjectHandle handle = scene.createHandle(object);
                generatedObjects.push_back(handle);
                collisionObjects.push_back(handle);
                object.transform.setPosition(rectangle.position);
                BoxCollider2D& collider = object.addComponent<BoxCollider2D>(rectangle.size);
                collider.setOffset(rectangle.size * 0.5f);
            }
        }
        catch (...)
        {
            queueGeneratedObjectsForDestruction(generatedObjects);
            throw;
        }

        queueGeneratedObjectsForDestruction(m_generatedObjects);
        m_layout = std::move(composited);
        m_data = data;
        m_loadedTileSize = data.tileSize();
        m_loadedRenderChunkSize = m_renderChunkSize;
        m_loadedSolidTileColor = m_solidTileColor;
        m_loadedTileSet = {};
        m_worldSize = {checkedTileExtent(data.width(), data.tileSize().x),
                       checkedTileExtent(data.height(), data.tileSize().y)};
        m_buildStats = {solidTileCount, renderedTileCount, texturedTileCount, renderChunkCount,
                        collisionRectangles.size()};
        m_lastUpdateStats = {};
        m_generatedObjects = std::move(generatedObjects);
        m_collisionObjects = std::move(collisionObjects);
        m_renderObject = std::move(renderObject);
        m_sceneState = scene.m_handleState;
        m_loadedSolidChar = solidCell;
        m_loadedObjectPrefix = objectPrefix;
        m_streamRegion.reset();
        m_legacyEditMode = false;
        return true;
    }

    const TileMapData& TileMap::data() const
    {
        return m_data;
    }

    bool TileMap::loadFromFile(Scene& scene, const std::string& filepath, char solidChar,
                               const std::string& objectPrefix)
    {
        const Layout fileLayout = readLayoutFromFile(filepath);

        if (fileLayout.empty()) return false;

        loadFromLayout(scene, fileLayout, solidChar, objectPrefix);
        return true;
    }

    bool TileMap::setTile(std::size_t row, std::size_t column, char tile)
    {
        m_lastUpdateStats = {};

        if (!m_legacyEditMode) return false;
        if (row >= m_layout.size() || column >= m_layout[row].size()) return false;

        const std::shared_ptr<detail::SceneHandleState> sceneState = m_sceneState.lock();
        GameObject* renderObject = m_renderObject.get();
        Scene* scene = sceneState == nullptr ? nullptr : sceneState->scene;

        if (scene == nullptr) return false;

        TileMapRenderComponent* renderer =
            renderObject == nullptr ? nullptr
                                    : renderObject->getComponent<TileMapRenderComponent>();

        if (renderObject != nullptr && renderer == nullptr) return false;

        const char previousTile = m_layout[row][column];

        if (previousTile == tile) return true;

        const bool renderChunkChanged =
            previousTile == m_loadedSolidChar || tile == m_loadedSolidChar ||
            m_loadedTileSet.contains(previousTile) || m_loadedTileSet.contains(tile);

        Layout updatedLayout = m_layout;
        updatedLayout[row][column] = tile;

        const std::size_t chunkRow = row / static_cast<std::size_t>(m_loadedRenderChunkSize.y);
        const std::size_t chunkColumn =
            column / static_cast<std::size_t>(m_loadedRenderChunkSize.x);
        std::optional<RenderChunk> replacement;

        if (renderChunkChanged)
        {
            replacement = buildRenderChunk(updatedLayout, m_loadedSolidChar, m_loadedTileSize,
                                           m_loadedRenderChunkSize, m_loadedSolidTileColor,
                                           m_loadedTileSet, chunkRow, chunkColumn);
        }

        const bool solidityChanged =
            (previousTile == m_loadedSolidChar) != (tile == m_loadedSolidChar);
        std::vector<CollisionRectangle> collisionRectangles;
        std::vector<CollisionGeometry> collisionGeometry;
        std::vector<GameObjectHandle> newCollisionObjects;
        std::vector<GameObjectHandle> newGeneratedObjects;
        GameObjectHandle newRenderObject;
        bool rendererCreated = false;

        const bool previousSolid = previousTile == m_loadedSolidChar;
        const bool nextSolid = tile == m_loadedSolidChar;
        const bool previousTextured = m_loadedTileSet.contains(previousTile);
        const bool nextTextured = m_loadedTileSet.contains(tile);
        const bool previousRendered = previousSolid || previousTextured;
        const bool nextRendered = nextSolid || nextTextured;
        const auto updatedCount = [](std::size_t count, bool previous, bool next)
        {
            if (previous == next) return count;
            return next ? count + 1u : count - 1u;
        };
        const std::size_t solidTileCount =
            updatedCount(m_buildStats.solidTileCount, previousSolid, nextSolid);
        const std::size_t renderedTileCount =
            updatedCount(m_buildStats.renderedTileCount, previousRendered, nextRendered);
        const std::size_t texturedTileCount =
            updatedCount(m_buildStats.texturedTileCount, previousTextured, nextTextured);

        if (solidityChanged)
        {
            collisionRectangles = buildCollisionRectangles(updatedLayout, m_loadedSolidChar);
            collisionGeometry = buildCollisionGeometry(collisionRectangles, m_loadedTileSize);
            newCollisionObjects.reserve(collisionGeometry.size());
        }

        try
        {
            if (renderer == nullptr && replacement)
            {
                GameObject& newRender = scene->createGameObject(m_loadedObjectPrefix + "_Render");
                newRenderObject = scene->createHandle(newRender);
                newGeneratedObjects.push_back(newRenderObject);
                std::vector<RenderChunk> chunks;
                chunks.push_back(std::move(*replacement));
                renderer = &newRender.addComponent<TileMapRenderComponent>(
                    std::move(chunks), m_loadedTileSet.texture(), m_loadedTileSet,
                    m_loadedRenderChunkSize, solidTileCount);
                renderer->setStreamRegion(m_streamRegion);
                rendererCreated = true;
            }

            if (solidityChanged)
            {
                for (std::size_t index = 0; index < collisionGeometry.size(); ++index)
                {
                    const CollisionGeometry& geometry = collisionGeometry[index];
                    GameObject& collisionObject = scene->createGameObject(
                        m_loadedObjectPrefix + "_Collision_" + std::to_string(index));
                    const GameObjectHandle collisionHandle = scene->createHandle(collisionObject);
                    newCollisionObjects.push_back(collisionHandle);
                    newGeneratedObjects.push_back(collisionHandle);
                    collisionObject.transform.setPosition(geometry.position);
                    BoxCollider2D& collider =
                        collisionObject.addComponent<BoxCollider2D>(geometry.size);
                    collider.setOffset(geometry.size * 0.5f);
                }
            }

            if (!rendererCreated && renderer != nullptr && renderChunkChanged)
            {
                renderer->replaceChunk(chunkRow, chunkColumn, std::move(replacement));
            }
        }
        catch (...)
        {
            queueGeneratedObjectsForDestruction(newGeneratedObjects);
            throw;
        }

        if (rendererCreated)
        {
            m_renderObject = newRenderObject;
        }

        if (renderer != nullptr) renderer->setSolidTileCount(solidTileCount);

        m_layout = std::move(updatedLayout);

        if (m_data.isValid())
        {
            const TileId id = static_cast<TileId>(static_cast<unsigned char>(tile)) + 1u;
            if (m_data.definition(id) == nullptr)
            {
                TileDefinition definition;
                definition.id = id;
                definition.collision =
                    tile == m_loadedSolidChar ? TileCollisionKind::Solid : TileCollisionKind::None;
                (void)m_data.setDefinition(std::move(definition));
            }
            (void)m_data.setTile(0u, row, column, id);
        }
        m_buildStats.solidTileCount = solidTileCount;
        m_buildStats.renderedTileCount = renderedTileCount;
        m_buildStats.texturedTileCount = texturedTileCount;
        m_buildStats.renderChunkCount = renderer == nullptr ? 0u : renderer->chunkCount();
        m_lastUpdateStats.rebuiltRenderChunkCount = static_cast<std::size_t>(renderChunkChanged);

        if (solidityChanged)
        {
            queueGeneratedObjectsForDestruction(m_collisionObjects);
            m_collisionObjects = std::move(newCollisionObjects);
            m_buildStats.collisionRectangleCount = collisionRectangles.size();
            m_lastUpdateStats.rebuiltCollisionRectangleCount = collisionRectangles.size();
            m_lastUpdateStats.collisionGeometryRebuilt = true;
        }

        if (rendererCreated || solidityChanged)
        {
            m_generatedObjects.clear();

            if (m_renderObject.isValid()) m_generatedObjects.push_back(m_renderObject);

            m_generatedObjects.insert(m_generatedObjects.end(), m_collisionObjects.begin(),
                                      m_collisionObjects.end());
        }

        return true;
    }

    std::optional<char> TileMap::tileAt(std::size_t row, std::size_t column) const
    {
        if (row >= m_layout.size() || column >= m_layout[row].size()) return std::nullopt;
        return m_layout[row][column];
    }

    const TileMapUpdateStats& TileMap::lastUpdateStats() const
    {
        return m_lastUpdateStats;
    }

    bool TileMap::setStreamRegion(TileMapRegion region)
    {
        if (region.columnCount == 0u || region.rowCount == 0u ||
            region.firstColumn > std::numeric_limits<std::size_t>::max() - region.columnCount ||
            region.firstRow > std::numeric_limits<std::size_t>::max() - region.rowCount)
        {
            return false;
        }

        const std::shared_ptr<detail::SceneHandleState> sceneState = m_sceneState.lock();

        if (sceneState == nullptr || sceneState->scene == nullptr) return false;

        GameObject* renderObject = m_renderObject.get();
        TileMapRenderComponent* renderer =
            renderObject == nullptr ? nullptr
                                    : renderObject->getComponent<TileMapRenderComponent>();

        if (renderObject != nullptr && renderer == nullptr) return false;

        m_streamRegion = region;
        if (renderer != nullptr) renderer->setStreamRegion(region);
        return true;
    }

    void TileMap::clearStreamRegion()
    {
        m_streamRegion.reset();
        GameObject* renderObject = m_renderObject.get();

        if (renderObject == nullptr) return;

        TileMapRenderComponent* renderer = renderObject->getComponent<TileMapRenderComponent>();
        if (renderer != nullptr) renderer->setStreamRegion(std::nullopt);
    }

    std::optional<TileMapRegion> TileMap::streamRegion() const
    {
        return m_streamRegion;
    }

    void TileMap::unload()
    {
        queueGeneratedObjectsForDestruction(m_generatedObjects);

        m_generatedObjects.clear();
        m_collisionObjects.clear();
        m_renderObject.reset();
        m_sceneState.reset();
        m_layout.clear();
        m_data.clear();
        m_loadedTileSize = {0.f, 0.f};
        m_loadedRenderChunkSize = {0u, 0u};
        m_loadedSolidTileColor = sf::Color::White;
        m_loadedTileSet = {};
        m_worldSize = {0.f, 0.f};
        m_buildStats = {};
        m_lastUpdateStats = {};
        m_streamRegion.reset();
        m_legacyEditMode = false;
    }

    bool TileMap::findFirstTilePosition(char tileChar, sf::Vector2f& outPosition,
                                        bool centered) const
    {
        for (std::size_t row = 0; row < m_layout.size(); ++row)
        {
            for (std::size_t column = 0; column < m_layout[row].size(); ++column)
            {
                if (m_layout[row][column] != tileChar) continue;

                outPosition = {static_cast<float>(column) * m_loadedTileSize.x,
                               static_cast<float>(row) * m_loadedTileSize.y};

                if (centered)
                {
                    outPosition += m_loadedTileSize * 0.5f;
                }

                return true;
            }
        }

        return false;
    }

    std::vector<sf::Vector2f> TileMap::findTilePositions(char tileChar, bool centered) const
    {
        std::vector<sf::Vector2f> positions;

        for (std::size_t row = 0; row < m_layout.size(); ++row)
        {
            for (std::size_t column = 0; column < m_layout[row].size(); ++column)
            {
                if (m_layout[row][column] != tileChar) continue;

                sf::Vector2f position = {static_cast<float>(column) * m_loadedTileSize.x,
                                         static_cast<float>(row) * m_loadedTileSize.y};

                if (centered)
                {
                    position += m_loadedTileSize * 0.5f;
                }

                positions.push_back(position);
            }
        }

        return positions;
    }

    const TileMap::Layout& TileMap::layout() const
    {
        return m_layout;
    }

    const sf::Vector2f& TileMap::worldSize() const
    {
        return m_worldSize;
    }

    const TileMapBuildStats& TileMap::buildStats() const
    {
        return m_buildStats;
    }

    TileMapRenderStats TileMap::renderStatsForView(const sf::View& view) const
    {
        return renderStatsForView(view, RenderContext2D{});
    }

    TileMapRenderStats TileMap::renderStatsForView(const sf::View& view,
                                                   const RenderContext2D& context) const
    {
        GameObject* renderObject = m_renderObject.get();

        if (renderObject == nullptr) return {};

        const TileMapRenderComponent* renderer =
            renderObject->getComponent<TileMapRenderComponent>();

        if (renderer == nullptr) return {};

        return renderer->statsForView(view, context);
    }

    TileMapRenderStats TileMap::lastRenderStats() const
    {
        GameObject* renderObject = m_renderObject.get();

        if (renderObject == nullptr) return {};

        const TileMapRenderComponent* renderer =
            renderObject->getComponent<TileMapRenderComponent>();

        if (renderer == nullptr) return {};

        return renderer->lastRenderStats();
    }

    TileMap::Layout TileMap::readLayoutFromFile(const std::string& filepath) const
    {
        std::ifstream file(filepath);

        if (!file.is_open()) return {};

        Layout layout;
        std::string line;

        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            layout.push_back(line);
        }

        if (file.bad()) return {};

        return layout;
    }

    void TileMap::queueGeneratedObjectsForDestruction(
        const std::vector<GameObjectHandle>& generatedObjects) const
    {
        for (const GameObjectHandle& handle : generatedObjects)
        {
            Scene* scene = handle.scene();
            GameObject* gameObject = handle.get();

            if (scene != nullptr && gameObject != nullptr)
            {
                scene->destroyGameObject(*gameObject);
            }
        }
    }
}
