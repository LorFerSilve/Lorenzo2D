#include <Lorenzo2D/Scene/Prefab.hpp>

#include <Lorenzo2D/Animation/Animator.hpp>
#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Movement/CharacterMotor2D.hpp>
#include <Lorenzo2D/Movement/GridStepController2D.hpp>
#include <Lorenzo2D/Movement/PlatformerController2D.hpp>
#include <Lorenzo2D/Movement/TopDownController2D.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CapsuleCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/ConvexPolygonCollider2D.hpp>
#include <Lorenzo2D/Renderer/CircleRenderer.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Renderer/RenderOrder2D.hpp>
#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace l2d
{
    namespace
    {
        bool isFinite(float value)
        {
            return std::isfinite(value);
        }

        bool isFinite(sf::Vector2f value)
        {
            return isFinite(value.x) && isFinite(value.y);
        }

        bool hasLineBreak(const std::string& value)
        {
            return value.find('\n') != std::string::npos || value.find('\r') != std::string::npos;
        }

        bool isValidMaterial(const PhysicsMaterial2D& material)
        {
            return isFinite(material.restitution) && material.restitution >= 0.f &&
                   material.restitution <= 1.f && isFinite(material.staticFriction) &&
                   material.staticFriction >= 0.f && material.staticFriction <= 1.f &&
                   isFinite(material.dynamicFriction) && material.dynamicFriction >= 0.f &&
                   material.dynamicFriction <= material.staticFriction;
        }

        bool isValidColliderProperties(const ColliderPrefabProperties& properties)
        {
            return isFinite(properties.offset) && isValidMaterial(properties.material);
        }

        bool validAssetId(const AssetId& id)
        {
            return !id.empty() && !hasLineBreak(id);
        }

        bool validTextureRect(sf::IntRect rectangle)
        {
            return rectangle.position.x >= 0 && rectangle.position.y >= 0 && rectangle.size.x > 0 &&
                   rectangle.size.y > 0 &&
                   rectangle.position.x <= std::numeric_limits<int>::max() - rectangle.size.x &&
                   rectangle.position.y <= std::numeric_limits<int>::max() - rectangle.size.y;
        }

        bool validSprite(const SpriteRendererPrefab& sprite)
        {
            return validAssetId(sprite.texture) && validTextureRect(sprite.textureRect) &&
                   isFinite(sprite.size) && sprite.size.x >= 0.f && sprite.size.y >= 0.f &&
                   isFinite(sprite.origin) && isFinite(sprite.renderOrder.depth) &&
                   sprite.renderOrder.depthMode <=
                       static_cast<std::uint8_t>(RenderDepthMode2D::Fixed);
        }

        bool validAnimator(const AnimatorPrefab& animator)
        {
            if (animator.clips.empty() || !isFinite(animator.playbackSpeed) ||
                animator.playbackSpeed < 0.f)
                return false;

            for (const AssetId& clip : animator.clips)
                if (!validAssetId(clip)) return false;
            return animator.initialClip.empty() ||
                   (validAssetId(animator.initialClip) &&
                    std::find(animator.clips.begin(), animator.clips.end(), animator.initialClip) !=
                        animator.clips.end());
        }

        void configureRenderOrder(GameObject& object, const RenderOrderPrefab& prefab)
        {
            RenderOrder2D& order = object.addComponent<RenderOrder2D>(
                static_cast<RenderDepthMode2D>(prefab.depthMode));
            order.setLayer(prefab.layer);
            order.setExplicitDepth(prefab.depth);
            order.setOrder(prefab.order);
        }

        void applyColliderProperties(Collider2D& collider,
                                     const ColliderPrefabProperties& properties)
        {
            collider.setOffset(properties.offset);
            collider.setMaterial(properties.material);
            collider.setFilter(properties.filter);
            collider.setSensor(properties.sensor);
        }
    }

    bool isValidPrefab(const Prefab& prefab)
    {
        if (hasLineBreak(prefab.name) || hasLineBreak(prefab.tag) ||
            !isFinite(prefab.transform.position) || !isFinite(prefab.transform.rotation) ||
            !isFinite(prefab.transform.scale))
        {
            return false;
        }

        if (prefab.rectangleRenderer &&
            (!isFinite(prefab.rectangleRenderer->size) || prefab.rectangleRenderer->size.x < 0.f ||
             prefab.rectangleRenderer->size.y < 0.f))
        {
            return false;
        }

        if (prefab.circleRenderer &&
            (!isFinite(prefab.circleRenderer->radius) || prefab.circleRenderer->radius < 0.f))
        {
            return false;
        }

        if (prefab.spriteRenderer && !validSprite(*prefab.spriteRenderer)) return false;
        if (prefab.animator && !validAnimator(*prefab.animator)) return false;

        for (const SerializedComponentPrefab& component : prefab.customComponents)
            if (component.type.empty() || component.version == 0u || hasLineBreak(component.type))
                return false;

        const bool validBodyType = !prefab.rigidBody ||
                                   prefab.rigidBody->bodyType == BodyType2D::Static ||
                                   prefab.rigidBody->bodyType == BodyType2D::Kinematic ||
                                   prefab.rigidBody->bodyType == BodyType2D::Dynamic;

        if (!validBodyType ||
            (prefab.rigidBody &&
             (!isFinite(prefab.rigidBody->velocity) || !isFinite(prefab.rigidBody->acceleration) ||
              !isFinite(prefab.rigidBody->mass) || prefab.rigidBody->mass <= 0.f ||
              !isFinite(prefab.rigidBody->gravityScale))))
        {
            return false;
        }

        if (prefab.characterMotor &&
            (!CharacterMotor2D::isValidConfig(prefab.characterMotor->config) ||
             (prefab.rigidBody && prefab.rigidBody->bodyType != BodyType2D::Kinematic) ||
             (!prefab.boxCollider && !prefab.circleCollider && !prefab.capsuleCollider)))
        {
            return false;
        }

        if ((prefab.topDownController || prefab.gridStepController ||
             prefab.platformerController) &&
            !prefab.characterMotor)
            return false;
        if (prefab.topDownController &&
            !TopDownController2D::isValidConfig(prefab.topDownController->config))
            return false;
        if (prefab.gridStepController &&
            !GridStepController2D::isValidConfig(prefab.gridStepController->config))
            return false;
        if (prefab.platformerController &&
            !PlatformerController2D::isValidConfig(prefab.platformerController->config))
            return false;

        if (prefab.boxCollider &&
            (!isFinite(prefab.boxCollider->size) || prefab.boxCollider->size.x < 0.f ||
             prefab.boxCollider->size.y < 0.f ||
             !isValidColliderProperties(prefab.boxCollider->properties)))
        {
            return false;
        }

        if (prefab.circleCollider &&
            (!isFinite(prefab.circleCollider->radius) || prefab.circleCollider->radius < 0.f ||
             !isValidColliderProperties(prefab.circleCollider->properties)))
        {
            return false;
        }

        if (prefab.capsuleCollider &&
            (!isFinite(prefab.capsuleCollider->radius) || prefab.capsuleCollider->radius <= 0.f ||
             !isFinite(prefab.capsuleCollider->height) ||
             prefab.capsuleCollider->height < 2.f * prefab.capsuleCollider->radius ||
             !isValidColliderProperties(prefab.capsuleCollider->properties)))
        {
            return false;
        }

        if (prefab.convexPolygonCollider &&
            (!ConvexPolygonCollider2D::isValidVertices(prefab.convexPolygonCollider->vertices) ||
             !isValidColliderProperties(prefab.convexPolygonCollider->properties)))
        {
            return false;
        }

        return true;
    }

    GameObject& instantiatePrefab(Scene& scene, const Prefab& prefab)
    {
        if (!isValidPrefab(prefab))
        {
            throw std::invalid_argument("Cannot instantiate an invalid prefab.");
        }

        if (prefab.spriteRenderer || prefab.animator)
            throw std::invalid_argument("Prefab assets require an AssetManager.");
        for (const SerializedComponentPrefab& component : prefab.customComponents)
            if (component.required)
                throw std::invalid_argument("A required component codec is missing.");

        GameObject& object = scene.createGameObject(prefab.name);
        object.setTag(prefab.tag);
        object.transform.setPosition(prefab.transform.position);
        object.transform.setRotation(prefab.transform.rotation);
        object.transform.setScale(prefab.transform.scale);
        object.setZOrder(prefab.zOrder);

        if (prefab.rectangleRenderer)
        {
            object.addComponent<RectangleRenderer>(prefab.rectangleRenderer->size,
                                                   prefab.rectangleRenderer->color);
        }

        if (prefab.circleRenderer)
        {
            object.addComponent<CircleRenderer>(prefab.circleRenderer->radius,
                                                prefab.circleRenderer->color);
        }

        if (prefab.rigidBody)
        {
            RigidBody2D& body = object.addComponent<RigidBody2D>();
            body.setBodyType(prefab.rigidBody->bodyType);
            body.setVelocity(prefab.rigidBody->velocity);
            body.setAcceleration(prefab.rigidBody->acceleration);
            body.setMass(prefab.rigidBody->mass);
            body.setUseGravity(prefab.rigidBody->useGravity);
            body.setGravityScale(prefab.rigidBody->gravityScale);
        }

        if (prefab.boxCollider)
        {
            BoxCollider2D& collider = object.addComponent<BoxCollider2D>(prefab.boxCollider->size);
            applyColliderProperties(collider, prefab.boxCollider->properties);
        }

        if (prefab.circleCollider)
        {
            CircleCollider2D& collider =
                object.addComponent<CircleCollider2D>(prefab.circleCollider->radius);
            applyColliderProperties(collider, prefab.circleCollider->properties);
        }

        if (prefab.capsuleCollider)
        {
            CapsuleCollider2D& collider = object.addComponent<CapsuleCollider2D>(
                prefab.capsuleCollider->radius, prefab.capsuleCollider->height);
            applyColliderProperties(collider, prefab.capsuleCollider->properties);
        }

        if (prefab.convexPolygonCollider)
        {
            ConvexPolygonCollider2D& collider = object.addComponent<ConvexPolygonCollider2D>(
                prefab.convexPolygonCollider->vertices);
            applyColliderProperties(collider, prefab.convexPolygonCollider->properties);
        }

        if (prefab.characterMotor)
            object.addComponent<CharacterMotor2D>(prefab.characterMotor->config);
        if (prefab.topDownController)
            object.addComponent<TopDownController2D>(prefab.topDownController->config);
        if (prefab.gridStepController)
            object.addComponent<GridStepController2D>(prefab.gridStepController->config);
        if (prefab.platformerController)
            object.addComponent<PlatformerController2D>(prefab.platformerController->config);

        object.setActive(prefab.active);
        return object;
    }

    GameObject& instantiatePrefab(Scene& scene, const Prefab& prefab, AssetManager& assets)
    {
        if (!isValidPrefab(prefab))
            throw std::invalid_argument("Cannot instantiate an invalid prefab.");

        GameObject& object = scene.createGameObject(prefab.name);

        try
        {
            object.setTag(prefab.tag);
            object.transform.setPosition(prefab.transform.position);
            object.transform.setRotation(prefab.transform.rotation);
            object.transform.setScale(prefab.transform.scale);
            object.setZOrder(prefab.zOrder);

            if (prefab.rectangleRenderer)
                object.addComponent<RectangleRenderer>(prefab.rectangleRenderer->size,
                                                       prefab.rectangleRenderer->color);
            if (prefab.circleRenderer)
                object.addComponent<CircleRenderer>(prefab.circleRenderer->radius,
                                                    prefab.circleRenderer->color);
            if (prefab.rigidBody)
            {
                RigidBody2D& body = object.addComponent<RigidBody2D>();
                body.setBodyType(prefab.rigidBody->bodyType);
                body.setVelocity(prefab.rigidBody->velocity);
                body.setAcceleration(prefab.rigidBody->acceleration);
                body.setMass(prefab.rigidBody->mass);
                body.setUseGravity(prefab.rigidBody->useGravity);
                body.setGravityScale(prefab.rigidBody->gravityScale);
            }
            if (prefab.boxCollider)
            {
                BoxCollider2D& collider =
                    object.addComponent<BoxCollider2D>(prefab.boxCollider->size);
                applyColliderProperties(collider, prefab.boxCollider->properties);
            }
            if (prefab.circleCollider)
            {
                CircleCollider2D& collider =
                    object.addComponent<CircleCollider2D>(prefab.circleCollider->radius);
                applyColliderProperties(collider, prefab.circleCollider->properties);
            }
            if (prefab.capsuleCollider)
            {
                CapsuleCollider2D& collider = object.addComponent<CapsuleCollider2D>(
                    prefab.capsuleCollider->radius, prefab.capsuleCollider->height);
                applyColliderProperties(collider, prefab.capsuleCollider->properties);
            }
            if (prefab.convexPolygonCollider)
            {
                ConvexPolygonCollider2D& collider = object.addComponent<ConvexPolygonCollider2D>(
                    prefab.convexPolygonCollider->vertices);
                applyColliderProperties(collider, prefab.convexPolygonCollider->properties);
            }
            if (prefab.characterMotor)
                object.addComponent<CharacterMotor2D>(prefab.characterMotor->config);
            if (prefab.topDownController)
                object.addComponent<TopDownController2D>(prefab.topDownController->config);
            if (prefab.gridStepController)
                object.addComponent<GridStepController2D>(prefab.gridStepController->config);
            if (prefab.platformerController)
                object.addComponent<PlatformerController2D>(prefab.platformerController->config);

            if (prefab.spriteRenderer)
            {
                const SpriteRendererPrefab& source = *prefab.spriteRenderer;
                const TextureHandle texture = assets.getTexture(source.texture);
                if (!texture) throw std::invalid_argument("Prefab texture asset is unavailable.");

                SpriteRenderer& sprite = object.addComponent<SpriteRenderer>(texture);
                sprite.setTextureRect(source.textureRect);
                if (source.size.x > 0.f || source.size.y > 0.f) sprite.setSize(source.size);
                sprite.setColor(source.color);
                if (!sprite.setOrigin(source.origin))
                    throw std::invalid_argument("Prefab sprite origin is invalid.");
                sprite.setFlippedX(source.flipX);
                sprite.setFlippedY(source.flipY);
                configureRenderOrder(object, source.renderOrder);
            }

            if (prefab.animator)
            {
                const AnimatorPrefab& source = *prefab.animator;
                Animator& animator = object.addComponent<Animator>();
                for (const AssetId& clipId : source.clips)
                {
                    const AnimationClipHandle clip = assets.getAnimationClip(clipId);
                    if (!clip || !animator.addClip(*clip))
                        throw std::invalid_argument("Prefab animation clip is unavailable.");
                }
                if (!animator.setPlaybackSpeed(source.playbackSpeed))
                    throw std::invalid_argument("Prefab playback speed is invalid.");
                if (!source.initialClip.empty())
                {
                    const AnimationClipHandle initial = assets.getAnimationClip(source.initialClip);
                    if (!initial || !animator.play(initial->name()))
                        throw std::invalid_argument(
                            "Prefab initial animation clip is unavailable.");
                }
                if (!source.playing) animator.pause();
            }

            object.setActive(prefab.active);
        }
        catch (...)
        {
            object.destroy();
            throw;
        }

        return object;
    }

    bool PrefabLibrary::store(std::string key, Prefab prefab)
    {
        if (key.empty() || !isValidPrefab(prefab)) return false;

        m_prefabs.insert_or_assign(std::move(key), std::move(prefab));
        return true;
    }

    bool PrefabLibrary::remove(const std::string& key)
    {
        return m_prefabs.erase(key) > 0u;
    }

    void PrefabLibrary::clear()
    {
        m_prefabs.clear();
    }

    bool PrefabLibrary::contains(const std::string& key) const
    {
        return m_prefabs.find(key) != m_prefabs.end();
    }

    const Prefab* PrefabLibrary::find(const std::string& key) const
    {
        const auto iterator = m_prefabs.find(key);
        return iterator == m_prefabs.end() ? nullptr : &iterator->second;
    }

    std::size_t PrefabLibrary::size() const
    {
        return m_prefabs.size();
    }

    GameObject* PrefabLibrary::instantiate(Scene& scene, const std::string& key) const
    {
        const Prefab* prefab = find(key);
        return prefab == nullptr ? nullptr : &instantiatePrefab(scene, *prefab);
    }
}
