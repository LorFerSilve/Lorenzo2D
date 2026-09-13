#include <Lorenzo2DEditor/ComponentInspectorModel.hpp>

#include <algorithm>

namespace l2d_editor
{
    namespace
    {
        bool sameRenderOrder(const l2d::RenderOrderPrefab& lhs, const l2d::RenderOrderPrefab& rhs)
        {
            return lhs.layer == rhs.layer && lhs.depth == rhs.depth && lhs.order == rhs.order &&
                   lhs.depthMode == rhs.depthMode;
        }
    }

    bool ComponentInspectorModel::setRectangleSize(sf::Vector2f size)
    {
        return editSelected("Set RectangleRenderer size",
                            [size](l2d::Prefab& prefab)
                            {
                                if (!prefab.rectangleRenderer ||
                                    prefab.rectangleRenderer->size == size)
                                    return false;
                                prefab.rectangleRenderer->size = size;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setRectangleColor(sf::Color color)
    {
        return editSelected("Set RectangleRenderer color",
                            [color](l2d::Prefab& prefab)
                            {
                                if (!prefab.rectangleRenderer ||
                                    prefab.rectangleRenderer->color == color)
                                    return false;
                                prefab.rectangleRenderer->color = color;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setCircleRadius(float radius)
    {
        return editSelected("Set CircleRenderer radius",
                            [radius](l2d::Prefab& prefab)
                            {
                                if (!prefab.circleRenderer ||
                                    prefab.circleRenderer->radius == radius)
                                    return false;
                                prefab.circleRenderer->radius = radius;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setCircleColor(sf::Color color)
    {
        return editSelected("Set CircleRenderer color",
                            [color](l2d::Prefab& prefab)
                            {
                                if (!prefab.circleRenderer || prefab.circleRenderer->color == color)
                                    return false;
                                prefab.circleRenderer->color = color;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setSpriteSize(sf::Vector2f size)
    {
        return editSelected("Set SpriteRenderer size",
                            [size](l2d::Prefab& prefab)
                            {
                                if (!prefab.spriteRenderer || prefab.spriteRenderer->size == size)
                                    return false;
                                prefab.spriteRenderer->size = size;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setSpriteColor(sf::Color color)
    {
        return editSelected("Set SpriteRenderer color",
                            [color](l2d::Prefab& prefab)
                            {
                                if (!prefab.spriteRenderer || prefab.spriteRenderer->color == color)
                                    return false;
                                prefab.spriteRenderer->color = color;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setSpriteOrigin(sf::Vector2f origin)
    {
        return editSelected("Set SpriteRenderer origin",
                            [origin](l2d::Prefab& prefab)
                            {
                                if (!prefab.spriteRenderer ||
                                    prefab.spriteRenderer->origin == origin)
                                    return false;
                                prefab.spriteRenderer->origin = origin;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setSpriteFlipX(bool flipped)
    {
        return editSelected("Set SpriteRenderer flip X",
                            [flipped](l2d::Prefab& prefab)
                            {
                                if (!prefab.spriteRenderer ||
                                    prefab.spriteRenderer->flipX == flipped)
                                    return false;
                                prefab.spriteRenderer->flipX = flipped;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setSpriteFlipY(bool flipped)
    {
        return editSelected("Set SpriteRenderer flip Y",
                            [flipped](l2d::Prefab& prefab)
                            {
                                if (!prefab.spriteRenderer ||
                                    prefab.spriteRenderer->flipY == flipped)
                                    return false;
                                prefab.spriteRenderer->flipY = flipped;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setSpriteRenderOrder(l2d::RenderOrderPrefab order)
    {
        return editSelected("Set SpriteRenderer render order",
                            [order](l2d::Prefab& prefab)
                            {
                                if (!prefab.spriteRenderer ||
                                    sameRenderOrder(prefab.spriteRenderer->renderOrder, order))
                                    return false;
                                prefab.spriteRenderer->renderOrder = order;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setAnimatorPlaybackSpeed(float speed)
    {
        return editSelected("Set Animator playback speed",
                            [speed](l2d::Prefab& prefab)
                            {
                                if (!prefab.animator || prefab.animator->playbackSpeed == speed)
                                    return false;
                                prefab.animator->playbackSpeed = speed;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setAnimatorPlaying(bool playing)
    {
        return editSelected("Set Animator playing",
                            [playing](l2d::Prefab& prefab)
                            {
                                if (!prefab.animator || prefab.animator->playing == playing)
                                    return false;
                                prefab.animator->playing = playing;
                                return true;
                            });
    }

    bool ComponentInspectorModel::removeAnimatorClipAsset(const l2d::AssetId& clip)
    {
        return editSelected("Remove Animator clip",
                            [&clip](l2d::Prefab& prefab)
                            {
                                if (!prefab.animator) return false;
                                auto& animator = *prefab.animator;
                                const auto found =
                                    std::find(animator.clips.begin(), animator.clips.end(), clip);
                                if (found == animator.clips.end()) return false;
                                animator.clips.erase(found);
                                if (animator.initialClip == clip) animator.initialClip.clear();
                                return true;
                            });
    }

    bool ComponentInspectorModel::clearAnimatorInitialClip()
    {
        return editSelected("Clear Animator initial clip",
                            [](l2d::Prefab& prefab)
                            {
                                if (!prefab.animator || prefab.animator->initialClip.empty())
                                    return false;
                                prefab.animator->initialClip.clear();
                                return true;
                            });
    }

    bool ComponentInspectorModel::setRigidBodyType(l2d::BodyType2D type)
    {
        return editSelected("Set RigidBody type",
                            [type](l2d::Prefab& prefab)
                            {
                                if (!prefab.rigidBody || prefab.rigidBody->bodyType == type)
                                    return false;
                                prefab.rigidBody->bodyType = type;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setRigidBodyVelocity(sf::Vector2f velocity)
    {
        return editSelected("Set RigidBody velocity",
                            [velocity](l2d::Prefab& prefab)
                            {
                                if (!prefab.rigidBody || prefab.rigidBody->velocity == velocity)
                                    return false;
                                prefab.rigidBody->velocity = velocity;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setRigidBodyAcceleration(sf::Vector2f acceleration)
    {
        return editSelected("Set RigidBody acceleration",
                            [acceleration](l2d::Prefab& prefab)
                            {
                                if (!prefab.rigidBody ||
                                    prefab.rigidBody->acceleration == acceleration)
                                    return false;
                                prefab.rigidBody->acceleration = acceleration;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setRigidBodyMass(float mass)
    {
        return editSelected("Set RigidBody mass",
                            [mass](l2d::Prefab& prefab)
                            {
                                if (!prefab.rigidBody || prefab.rigidBody->mass == mass)
                                    return false;
                                prefab.rigidBody->mass = mass;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setRigidBodyUseGravity(bool enabled)
    {
        return editSelected("Set RigidBody gravity",
                            [enabled](l2d::Prefab& prefab)
                            {
                                if (!prefab.rigidBody || prefab.rigidBody->useGravity == enabled)
                                    return false;
                                prefab.rigidBody->useGravity = enabled;
                                return true;
                            });
    }

    bool ComponentInspectorModel::setRigidBodyGravityScale(float scale)
    {
        return editSelected("Set RigidBody gravity scale",
                            [scale](l2d::Prefab& prefab)
                            {
                                if (!prefab.rigidBody || prefab.rigidBody->gravityScale == scale)
                                    return false;
                                prefab.rigidBody->gravityScale = scale;
                                return true;
                            });
    }
}
