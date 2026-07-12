#include <Lorenzo2D/Scene/SceneManager.hpp>

namespace l2d
{
    Scene& SceneManager::createScene(const std::string& name)
    {
        std::unique_ptr<Scene> scene = std::make_unique<Scene>(name);

        Scene& reference = *scene;

        m_scenes.push_back(std::move(scene));

        if (m_activeScene == nullptr)
        {
            m_activeScene = &reference;
        }

        return reference;
    }

    bool SceneManager::setActiveScene(const std::string& name)
    {
        Scene* scene = findSceneByName(name);

        if (scene == nullptr)
            return false;

        m_activeScene = scene;
        return true;
    }

    Scene* SceneManager::activeScene()
    {
        return m_activeScene;
    }

    const Scene* SceneManager::activeScene() const
    {
        return m_activeScene;
    }

    Scene* SceneManager::findSceneByName(const std::string& name)
    {
        for (const std::unique_ptr<Scene>& scene : m_scenes)
        {
            if (scene->name() == name)
                return scene.get();
        }

        return nullptr;
    }

    const Scene* SceneManager::findSceneByName(const std::string& name) const
    {
        for (const std::unique_ptr<Scene>& scene : m_scenes)
        {
            if (scene->name() == name)
                return scene.get();
        }

        return nullptr;
    }

    std::size_t SceneManager::sceneCount() const
    {
        return m_scenes.size();
    }

    void SceneManager::update(float deltaTime)
    {
        if (m_activeScene == nullptr)
            return;

        m_activeScene->update(deltaTime);
    }

    void SceneManager::render(sf::RenderWindow& window)
    {
        if (m_activeScene == nullptr)
            return;

        m_activeScene->render(window);
    }

    void SceneManager::destroyQueuedGameObjects()
    {
        for (const std::unique_ptr<Scene>& scene : m_scenes)
        {
            scene->destroyQueuedGameObjects();
        }
    }

    std::size_t SceneManager::destroyQueuedGameObjectCount() const
    {
        std::size_t count = 0;

        for (const std::unique_ptr<Scene>& scene : m_scenes)
        {
            count += scene->destroyQueuedGameObjectCount();
        }

        return count;
    }

    void SceneManager::clear()
    {
        m_activeScene = nullptr;
        m_scenes.clear();
    }
}
