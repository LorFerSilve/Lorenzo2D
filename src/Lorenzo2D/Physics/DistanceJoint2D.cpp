#include <Lorenzo2D/Physics/DistanceJoint2D.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace l2d
{
    namespace
    {
        JointId allocateJointId()
        {
            static std::atomic<JointId> nextId{1};
            JointId id = nextId.fetch_add(1, std::memory_order_relaxed);

            while (id == InvalidJointId)
            {
                id = nextId.fetch_add(1, std::memory_order_relaxed);
            }

            return id;
        }

        sf::Vector2f sanitizeVector(sf::Vector2f value)
        {
            if (!std::isfinite(value.x) || !std::isfinite(value.y)) return {0.f, 0.f};

            return value;
        }
    }

    DistanceJoint2D::DistanceJoint2D(GameObjectId connectedObjectId, float restLength)
        : m_id(allocateJointId()), m_connectedObjectId(connectedObjectId), m_localAnchor(0.f, 0.f),
          m_connectedLocalAnchor(0.f, 0.f), m_restLength(0.f), m_stiffness(1.f), m_damping(0.f),
          m_collideConnected(false)
    {
        setRestLength(restLength);
    }

    JointId DistanceJoint2D::id() const
    {
        return m_id;
    }

    GameObjectId DistanceJoint2D::connectedObjectId() const
    {
        return m_connectedObjectId;
    }

    void DistanceJoint2D::setConnectedObjectId(GameObjectId objectId)
    {
        m_connectedObjectId = objectId;
    }

    const sf::Vector2f& DistanceJoint2D::localAnchor() const
    {
        return m_localAnchor;
    }

    void DistanceJoint2D::setLocalAnchor(sf::Vector2f anchor)
    {
        m_localAnchor = sanitizeVector(anchor);
    }

    const sf::Vector2f& DistanceJoint2D::connectedLocalAnchor() const
    {
        return m_connectedLocalAnchor;
    }

    void DistanceJoint2D::setConnectedLocalAnchor(sf::Vector2f anchor)
    {
        m_connectedLocalAnchor = sanitizeVector(anchor);
    }

    float DistanceJoint2D::restLength() const
    {
        return m_restLength;
    }

    void DistanceJoint2D::setRestLength(float length)
    {
        m_restLength = std::isfinite(length) && length >= 0.f ? length : 0.f;
    }

    float DistanceJoint2D::stiffness() const
    {
        return m_stiffness;
    }

    void DistanceJoint2D::setStiffness(float stiffness)
    {
        m_stiffness = std::isfinite(stiffness) ? std::clamp(stiffness, 0.f, 1.f) : 1.f;
    }

    float DistanceJoint2D::damping() const
    {
        return m_damping;
    }

    void DistanceJoint2D::setDamping(float damping)
    {
        m_damping = std::isfinite(damping) && damping >= 0.f ? damping : 0.f;
    }

    bool DistanceJoint2D::collideConnected() const
    {
        return m_collideConnected;
    }

    void DistanceJoint2D::setCollideConnected(bool collideConnected)
    {
        m_collideConnected = collideConnected;
    }
}
