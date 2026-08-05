#include <Lorenzo2D/Scene/Prefab.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Renderer/CircleRenderer.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

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

        return true;
    }

    GameObject& instantiatePrefab(Scene& scene, const Prefab& prefab)
    {
        if (!isValidPrefab(prefab))
        {
            throw std::invalid_argument("Cannot instantiate an invalid prefab.");
        }

        GameObject& object = scene.createGameObject(prefab.name);
        object.setTag(prefab.tag);
        object.transform.setPosition(prefab.transform.position);
        object.transform.setRotation(prefab.transform.rotation);
        object.transform.setScale(prefab.transform.scale);

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

        object.setActive(prefab.active);
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
