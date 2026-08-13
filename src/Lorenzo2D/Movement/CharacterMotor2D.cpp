#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CapsuleCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace l2d
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;
        constexpr std::size_t MaximumIterations = 32u;

        enum class MotorShapeKind
        {
            Circle,
            Box,
            Capsule
        };

        struct MotorShape
        {
            MotorShapeKind kind = MotorShapeKind::Circle;
            sf::Vector2f center = {0.f, 0.f};
            sf::Vector2f size = {0.f, 0.f};
            float radius = 0.f;
            float height = 0.f;
            float rotationDegrees = 0.f;
        };

        bool finite(sf::Vector2f value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        double lengthSquared(sf::Vector2f value)
        {
            return static_cast<double>(value.x) * value.x + static_cast<double>(value.y) * value.y;
        }

        float length(sf::Vector2f value)
        {
            return static_cast<float>(std::sqrt(lengthSquared(value)));
        }

        double dot(sf::Vector2f first, sf::Vector2f second)
        {
            return static_cast<double>(first.x) * second.x +
                   static_cast<double>(first.y) * second.y;
        }

        std::optional<sf::Vector2f> normalized(sf::Vector2f value)
        {
            const double squared = lengthSquared(value);
            if (!finite(value) || squared <= std::numeric_limits<double>::epsilon())
                return std::nullopt;
            const double inverse = 1.0 / std::sqrt(squared);
            return sf::Vector2f{static_cast<float>(value.x * inverse),
                                static_cast<float>(value.y * inverse)};
        }

        bool supportedCollider(const Collider2D& collider)
        {
            return dynamic_cast<const CircleCollider2D*>(&collider) != nullptr ||
                   dynamic_cast<const BoxCollider2D*>(&collider) != nullptr ||
                   dynamic_cast<const CapsuleCollider2D*>(&collider) != nullptr;
        }

        const Collider2D* selectedCollider(const GameObject& object, ColliderId requested)
        {
            const Collider2D* selected = nullptr;
            for (const Collider2D* collider : object.getComponents<Collider2D>())
            {
                if (collider == nullptr || !collider->isActive() || !supportedCollider(*collider))
                    continue;
                if (requested != InvalidColliderId && collider->id() != requested) continue;
                if (selected == nullptr || collider->id() < selected->id()) selected = collider;
            }
            return selected;
        }

        std::optional<MotorShape> motorShape(const Collider2D& collider)
        {
            MotorShape shape;
            shape.center = collider.worldPosition();
            shape.rotationDegrees = collider.worldRotation();

            if (const auto* circle = dynamic_cast<const CircleCollider2D*>(&collider))
            {
                shape.kind = MotorShapeKind::Circle;
                shape.radius = circle->worldRadius();
            }
            else if (const auto* box = dynamic_cast<const BoxCollider2D*>(&collider))
            {
                shape.kind = MotorShapeKind::Box;
                shape.size = box->worldHalfExtents() * 2.f;
            }
            else if (const auto* capsule = dynamic_cast<const CapsuleCollider2D*>(&collider))
            {
                shape.kind = MotorShapeKind::Capsule;
                shape.radius = capsule->worldRadius();
                const auto segment = capsule->worldSegment();
                shape.height = length(segment[1] - segment[0]) + 2.f * shape.radius;
            }
            else
            {
                return std::nullopt;
            }

            if (!finite(shape.center) || !std::isfinite(shape.rotationDegrees) ||
                (shape.kind == MotorShapeKind::Circle &&
                 (!std::isfinite(shape.radius) || shape.radius <= 0.f)) ||
                (shape.kind == MotorShapeKind::Box &&
                 (!finite(shape.size) || shape.size.x <= 0.f || shape.size.y <= 0.f)) ||
                (shape.kind == MotorShapeKind::Capsule &&
                 (!std::isfinite(shape.radius) || !std::isfinite(shape.height) ||
                  shape.radius <= 0.f || shape.height < 2.f * shape.radius)))
                return std::nullopt;

            return shape;
        }

        std::vector<PhysicsQueryHit2D> overlaps(const PhysicsQueryContext2D& queries,
                                                const MotorShape& shape,
                                                const PhysicsQueryFilter2D& filter)
        {
            switch (shape.kind)
            {
            case MotorShapeKind::Circle:
                return queries.overlapCircle(shape.center, shape.radius, filter);
            case MotorShapeKind::Box:
                return queries.overlapBox(shape.center, shape.size, shape.rotationDegrees, filter);
            case MotorShapeKind::Capsule:
                return queries.overlapCapsule(shape.center, shape.radius, shape.height,
                                              shape.rotationDegrees, filter);
            }
            return {};
        }

        std::optional<PhysicsQueryHit2D> cast(const PhysicsQueryContext2D& queries,
                                              const MotorShape& shape, sf::Vector2f displacement,
                                              const PhysicsQueryFilter2D& filter)
        {
            const sf::Vector2f end = shape.center + displacement;
            switch (shape.kind)
            {
            case MotorShapeKind::Circle:
                return queries.castCircle(shape.center, end, shape.radius, filter);
            case MotorShapeKind::Box:
                return queries.castBox(shape.center, end, shape.size, shape.rotationDegrees,
                                       filter);
            case MotorShapeKind::Capsule:
                return queries.castCapsule(shape.center, end, shape.radius, shape.height,
                                           shape.rotationDegrees, filter);
            }
            return std::nullopt;
        }

        CharacterContactKind2D contactKind(sf::Vector2f normal, sf::Vector2f up,
                                           double groundedThreshold)
        {
            const double alignment = dot(normal, up);
            if (alignment >= groundedThreshold) return CharacterContactKind2D::Ground;
            if (alignment <= -groundedThreshold) return CharacterContactKind2D::Ceiling;
            return CharacterContactKind2D::Wall;
        }

        void recordContact(CharacterMoveResult2D& result, const PhysicsQueryHit2D& hit,
                           sf::Vector2f normal, CharacterContactKind2D kind, bool recovery,
                           bool probe)
        {
            CharacterMotorContact2D contact;
            contact.object = hit.object;
            contact.colliderId = hit.colliderId;
            contact.point = hit.point;
            contact.normal = normal;
            contact.distance = hit.distance;
            contact.penetration = hit.penetration;
            contact.kind = kind;
            contact.recoveredOverlap = recovery;
            contact.groundProbe = probe;
            result.contacts.push_back(std::move(contact));
        }

        void applyContactState(CharacterMotorState2D& state, CharacterContactKind2D kind,
                               sf::Vector2f normal, const PhysicsQueryHit2D& hit, sf::Vector2f up)
        {
            if (kind == CharacterContactKind2D::Ground)
            {
                if (!state.grounded || dot(normal, up) > dot(state.groundNormal, up) ||
                    (dot(normal, up) == dot(state.groundNormal, up) &&
                     hit.colliderId < state.supportColliderId))
                {
                    state.groundNormal = normal;
                    state.support = hit.object;
                    state.supportColliderId = hit.colliderId;
                }
                state.grounded = true;
            }
            else if (kind == CharacterContactKind2D::Ceiling)
            {
                state.touchingCeiling = true;
            }
            else
            {
                state.touchingWall = true;
            }
        }

        sf::Vector2f moveOwner(GameObject& owner, sf::Vector2f displacement)
        {
            const sf::Vector2f before = owner.transform.position();
            owner.transform.move(displacement);
            return owner.transform.position() - before;
        }
    }

    sf::Vector2f CharacterMoveResult2D::totalDisplacement() const
    {
        return inheritedDisplacement + recoveryDisplacement + movementDisplacement +
               snapDisplacement;
    }

    CharacterMotor2D::CharacterMotor2D() = default;

    CharacterMotor2D::CharacterMotor2D(CharacterMotorConfig2D config)
    {
        (void)setConfig(std::move(config));
    }

    bool CharacterMotor2D::isValidConfig(const CharacterMotorConfig2D& config)
    {
        return std::isfinite(config.skinWidth) && config.skinWidth >= 0.f &&
               std::isfinite(config.groundProbeDistance) && config.groundProbeDistance >= 0.f &&
               std::isfinite(config.maximumSlopeAngleDegrees) &&
               config.maximumSlopeAngleDegrees >= 0.f && config.maximumSlopeAngleDegrees < 90.f &&
               std::isfinite(config.minimumMoveDistance) && config.minimumMoveDistance >= 0.f &&
               std::isfinite(config.maximumMoveDistance) && config.maximumMoveDistance > 0.f &&
               config.minimumMoveDistance <= config.maximumMoveDistance &&
               std::isfinite(config.maximumPlatformDisplacement) &&
               config.maximumPlatformDisplacement >= 0.f && config.maximumSlideIterations > 0u &&
               config.maximumSlideIterations <= MaximumIterations &&
               config.maximumRecoveryIterations <= MaximumIterations &&
               normalized(config.upDirection).has_value() &&
               config.queryFilter.ignoredObject == InvalidGameObjectId;
    }

    bool CharacterMotor2D::setConfig(CharacterMotorConfig2D config)
    {
        if (!isValidConfig(config)) return false;
        config.upDirection = *normalized(config.upDirection);
        m_config = std::move(config);
        clearState();
        return true;
    }

    const CharacterMotorConfig2D& CharacterMotor2D::config() const
    {
        return m_config;
    }

    void CharacterMotor2D::setColliderId(ColliderId colliderId)
    {
        if (m_colliderId == colliderId) return;
        m_colliderId = colliderId;
        clearState();
    }

    ColliderId CharacterMotor2D::colliderId() const
    {
        return m_colliderId;
    }

    CharacterMoveResult2D CharacterMotor2D::move(const PhysicsQueryContext2D& queries,
                                                 sf::Vector2f displacement)
    {
        CharacterMoveResult2D result;
        result.requestedDisplacement = displacement;

        GameObject* character = owner();
        if (character == nullptr || !isValidConfig(m_config) || !finite(displacement) ||
            length(displacement) > m_config.maximumMoveDistance)
            return result;

        // A simulated dynamic body or an immovable static body conflicts with
        // the motor owning translation. An optional kinematic body is safe.
        if (const RigidBody2D* body = character->getComponent<RigidBody2D>();
            body != nullptr && body->bodyType() != BodyType2D::Kinematic)
            return result;

        const Collider2D* collider = selectedCollider(*character, m_colliderId);
        if (collider == nullptr) return result;
        std::optional<MotorShape> shape = motorShape(*collider);
        if (!shape) return result;

        PhysicsQueryFilter2D filter = m_config.queryFilter;
        filter.ignoredObject = character->id();

        const GameObjectHandle previousSupport = m_state.support;
        const bool hadSupportPosition = m_hasSupportPosition;
        const sf::Vector2f previousSupportPosition = m_supportPosition;
        m_state = {};
        m_hasSupportPosition = false;

        const double slopeRadians =
            static_cast<double>(m_config.maximumSlopeAngleDegrees) * Pi / 180.0;
        const double groundedThreshold = std::cos(slopeRadians);

        const auto moveAndSlide = [&](sf::Vector2f requested, sf::Vector2f& accumulator)
        {
            sf::Vector2f remaining = requested;
            for (std::size_t iteration = 0u; iteration < m_config.maximumSlideIterations;
                 ++iteration)
            {
                const float remainingLength = length(remaining);
                if (!std::isfinite(remainingLength) ||
                    remainingLength <= m_config.minimumMoveDistance)
                    break;

                const std::optional<PhysicsQueryHit2D> hit =
                    cast(queries, *shape, remaining, filter);
                if (!hit)
                {
                    const sf::Vector2f applied = moveOwner(*character, remaining);
                    shape->center += applied;
                    accumulator += applied;
                    remaining -= applied;
                    break;
                }

                const std::optional<sf::Vector2f> hitNormal = normalized(hit->normal);
                if (!hitNormal || dot(remaining, *hitNormal) >= 0.0)
                {
                    const sf::Vector2f applied = moveOwner(*character, remaining);
                    shape->center += applied;
                    accumulator += applied;
                    remaining -= applied;
                    break;
                }

                const sf::Vector2f direction = remaining / remainingLength;
                const float travel = std::max(0.f, hit->distance - m_config.skinWidth);
                const sf::Vector2f step = direction * std::min(travel, remainingLength);
                const sf::Vector2f applied = moveOwner(*character, step);
                shape->center += applied;
                accumulator += applied;
                remaining -= applied;

                const CharacterContactKind2D kind =
                    contactKind(*hitNormal, m_config.upDirection, groundedThreshold);
                recordContact(result, *hit, *hitNormal, kind, false, false);
                applyContactState(m_state, kind, *hitNormal, *hit, m_config.upDirection);

                const double inward = dot(remaining, *hitNormal);
                if (inward < 0.0) remaining -= *hitNormal * static_cast<float>(inward);

                if (lengthSquared(applied) <= 0.0 &&
                    length(remaining) <= m_config.minimumMoveDistance)
                    break;
            }
            return remaining;
        };

        if (m_config.inheritPlatformTranslation && hadSupportPosition)
        {
            if (GameObject* support = previousSupport.get())
            {
                const sf::Vector2f platformDelta =
                    support->transform.position() - previousSupportPosition;
                if (finite(platformDelta) &&
                    length(platformDelta) <= m_config.maximumPlatformDisplacement)
                {
                    (void)moveAndSlide(platformDelta, result.inheritedDisplacement);
                }
            }
        }

        for (std::size_t iteration = 0u; iteration < m_config.maximumRecoveryIterations;
             ++iteration)
        {
            const std::vector<PhysicsQueryHit2D> hits = overlaps(queries, *shape, filter);
            const PhysicsQueryHit2D* deepest = nullptr;
            for (const PhysicsQueryHit2D& hit : hits)
            {
                if (!std::isfinite(hit.penetration) || hit.penetration <= 0.f ||
                    !normalized(hit.normal))
                    continue;
                if (deepest == nullptr || hit.penetration > deepest->penetration ||
                    (hit.penetration == deepest->penetration &&
                     hit.colliderId < deepest->colliderId))
                    deepest = &hit;
            }
            if (deepest == nullptr) break;

            const sf::Vector2f normal = *normalized(deepest->normal);
            const sf::Vector2f recovery = normal * (deepest->penetration + m_config.skinWidth);
            const sf::Vector2f applied = moveOwner(*character, recovery);
            if (lengthSquared(applied) <= 0.0) break;
            shape->center += applied;
            result.recoveryDisplacement += applied;

            const CharacterContactKind2D kind =
                contactKind(normal, m_config.upDirection, groundedThreshold);
            recordContact(result, *deepest, normal, kind, true, false);
            applyContactState(m_state, kind, normal, *deepest, m_config.upDirection);
        }

        const sf::Vector2f remaining = moveAndSlide(displacement, result.movementDisplacement);

        const bool movingUp = dot(displacement, m_config.upDirection) > 0.0;
        if (!movingUp && m_config.groundProbeDistance > 0.f)
        {
            const sf::Vector2f down = -m_config.upDirection;
            const float probeDistance = m_config.skinWidth + m_config.groundProbeDistance;
            const std::optional<PhysicsQueryHit2D> groundHit =
                cast(queries, *shape, down * probeDistance, filter);
            if (groundHit)
            {
                const std::optional<sf::Vector2f> normal = normalized(groundHit->normal);
                if (normal && dot(*normal, m_config.upDirection) >= groundedThreshold)
                {
                    if (m_config.snapToGround && groundHit->distance > m_config.skinWidth)
                    {
                        const float snapDistance = std::min(
                            groundHit->distance - m_config.skinWidth, m_config.groundProbeDistance);
                        const sf::Vector2f applied = moveOwner(*character, down * snapDistance);
                        shape->center += applied;
                        result.snapDisplacement += applied;
                    }
                    const CharacterContactKind2D kind = CharacterContactKind2D::Ground;
                    recordContact(result, *groundHit, *normal, kind, false, true);
                    applyContactState(m_state, kind, *normal, *groundHit, m_config.upDirection);
                }
            }
        }

        if (m_state.grounded)
        {
            if (GameObject* support = m_state.support.get())
            {
                m_supportPosition = support->transform.position();
                m_hasSupportPosition = finite(m_supportPosition);
            }
        }

        result.succeeded = true;
        result.remainingDisplacement = remaining;
        result.state = m_state;
        return result;
    }

    CharacterMoveResult2D CharacterMotor2D::testMove(const PhysicsQueryContext2D& queries,
                                                     sf::Vector2f displacement)
    {
        GameObject* character = owner();
        if (character == nullptr) return {};

        const sf::Vector2f position = character->transform.position();
        const CharacterMotorState2D state = m_state;
        const sf::Vector2f supportPosition = m_supportPosition;
        const bool hasSupportPosition = m_hasSupportPosition;

        CharacterMoveResult2D result = move(queries, displacement);
        character->transform.setPosition(position);
        m_state = state;
        m_supportPosition = supportPosition;
        m_hasSupportPosition = hasSupportPosition;
        return result;
    }

    const CharacterMotorState2D& CharacterMotor2D::state() const
    {
        return m_state;
    }

    void CharacterMotor2D::clearState()
    {
        m_state = {};
        m_hasSupportPosition = false;
    }
}
