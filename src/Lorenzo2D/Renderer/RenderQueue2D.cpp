#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Renderer/RenderContext2D.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <SFML/System/Angle.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace l2d
{
    namespace
    {
        sf::Vector2f transformedPoint(const TransformState& state, sf::Vector2f localPoint)
        {
            const double radians =
                static_cast<double>(state.rotation) * 3.14159265358979323846 / 180.0;
            const double cosine = std::cos(radians);
            const double sine = std::sin(radians);
            const double x = static_cast<double>(localPoint.x) * state.scale.x;
            const double y = static_cast<double>(localPoint.y) * state.scale.y;

            return {static_cast<float>(state.position.x + cosine * x - sine * y),
                    static_cast<float>(state.position.y + sine * x + cosine * y)};
        }

        sf::Vector2f footPointFor(const GameObject& gameObject, const RenderOrder2D& order,
                                  float interpolationAlpha)
        {
            const TransformState state = gameObject.transform.interpolated(interpolationAlpha);

            if (order.localFootPoint())
            {
                return transformedPoint(state, *order.localFootPoint());
            }

            if (const SpriteRenderer* sprite = gameObject.getComponent<SpriteRenderer>();
                sprite != nullptr && sprite->isActive())
            {
                return sprite->worldFootPoint(interpolationAlpha);
            }

            return state.position;
        }

        RenderSortKey2D makeSortKey(const GameObject& gameObject, const RenderOrder2D* order,
                                    const RenderContext2D& context, std::uint64_t insertionOrder)
        {
            RenderSortKey2D key;
            key.order = gameObject.zOrder();
            key.insertionOrder = insertionOrder;

            if (order == nullptr || !order->isActive()) return key;

            key.layer = order->layer();
            key.order = order->orderOr(gameObject.zOrder());

            switch (order->depthMode())
            {
            case RenderDepthMode2D::Explicit:
                key.depth = order->explicitDepth();
                break;
            case RenderDepthMode2D::WorldY:
                key.depth = footPointFor(gameObject, *order, context.interpolationAlpha).y;
                break;
            case RenderDepthMode2D::ProjectedY:
                key.depth =
                    context.depthFor(footPointFor(gameObject, *order, context.interpolationAlpha));
                break;
            case RenderDepthMode2D::Fixed:
                key.depth = order->explicitDepth();
                break;
            }

            return key;
        }
    }

    void RenderQueue2D::build(Scene& scene)
    {
        build(scene, RenderContext2D{});
    }

    void RenderQueue2D::build(Scene& scene, const RenderContext2D& context)
    {
        m_entries.clear();
        m_entries.reserve(scene.gameObjects().size());

        std::size_t insertionOrder = 0;

        for (const std::unique_ptr<GameObject>& gameObject : scene.gameObjects())
        {
            if (gameObject != nullptr && gameObject->isActive() && !gameObject->isDestroyQueued())
            {
                const RenderOrder2D* order = gameObject->getComponent<RenderOrder2D>();
                const RenderPass2D pass =
                    order != nullptr && order->isActive() ? order->pass() : RenderPass2D::World;

                if (pass == context.pass)
                {
                    const RenderSortKey2D sortKey =
                        makeSortKey(*gameObject, order, context, insertionOrder);
                    if (context.layers.contains(sortKey.layer))
                    {
                        m_entries.push_back({scene.createHandle(*gameObject), gameObject->zOrder(),
                                             insertionOrder, sortKey});
                    }
                }
            }

            ++insertionOrder;
        }

        std::stable_sort(m_entries.begin(), m_entries.end(),
                         [](const RenderQueueEntry2D& left, const RenderQueueEntry2D& right)
                         { return left.sortKey < right.sortKey; });
    }

    void RenderQueue2D::clear()
    {
        m_entries.clear();
    }

    std::size_t RenderQueue2D::size() const
    {
        return m_entries.size();
    }

    bool RenderQueue2D::empty() const
    {
        return m_entries.empty();
    }

    const std::vector<RenderQueueEntry2D>& RenderQueue2D::entries() const
    {
        return m_entries;
    }

    void RenderQueue2D::render(sf::RenderWindow& window, float interpolationAlpha) const
    {
        render(window, RenderContext2D{interpolationAlpha});
    }

    void RenderQueue2D::render(sf::RenderWindow& window, const RenderContext2D& context) const
    {
        for (const RenderQueueEntry2D& entry : m_entries)
        {
            if (GameObject* gameObject = entry.gameObject.get())
            {
                gameObject->render(window, context);
            }
        }
    }
}
