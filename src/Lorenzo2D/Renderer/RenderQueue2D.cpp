#include <Lorenzo2D/Renderer/RenderQueue2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <algorithm>
#include <memory>

namespace l2d
{
    void RenderQueue2D::build(Scene& scene)
    {
        m_entries.clear();
        m_entries.reserve(scene.gameObjects().size());

        std::size_t insertionOrder = 0;

        for (const std::unique_ptr<GameObject>& gameObject : scene.gameObjects())
        {
            if (gameObject != nullptr && gameObject->isActive() && !gameObject->isDestroyQueued())
            {
                m_entries.push_back(
                    {scene.createHandle(*gameObject), gameObject->zOrder(), insertionOrder});
            }

            ++insertionOrder;
        }

        std::stable_sort(m_entries.begin(), m_entries.end(),
                         [](const RenderQueueEntry2D& left, const RenderQueueEntry2D& right)
                         { return left.zOrder < right.zOrder; });
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
        for (const RenderQueueEntry2D& entry : m_entries)
        {
            if (GameObject* gameObject = entry.gameObject.get())
            {
                gameObject->render(window, interpolationAlpha);
            }
        }
    }
}
