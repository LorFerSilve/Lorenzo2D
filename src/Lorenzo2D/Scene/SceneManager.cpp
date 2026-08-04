#include <Lorenzo2D/Scene/SceneManager.hpp>

#include <stdexcept>

namespace l2d
{
    Scene& SceneManager::createScene(const std::string& name)
    {
        if (findSceneByName(name) != nullptr)
        {
            throw std::invalid_argument("SceneManager requires every scene name to be unique.");
        }

        std::unique_ptr<Scene> scene = std::make_unique<Scene>(name);
        scene->m_ownerManager = this;

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

        if (scene == nullptr) return false;

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
            if (scene->name() == name) return scene.get();
        }

        return nullptr;
    }

    const Scene* SceneManager::findSceneByName(const std::string& name) const
    {
        for (const std::unique_ptr<Scene>& scene : m_scenes)
        {
            if (scene->name() == name) return scene.get();
        }

        return nullptr;
    }

    std::size_t SceneManager::sceneCount() const
    {
        return m_scenes.size();
    }

    void SceneManager::fixedUpdate(float deltaTime)
    {
        if (m_fixedUpdateInProgress) return;

        Scene* activeScene = m_activeScene;

        if (activeScene == nullptr) return;

        m_fixedUpdateInProgress = true;
        beginDispatch();

        try
        {
            activeScene->fixedUpdate(deltaTime);
        }
        catch (...)
        {
            m_fixedUpdateInProgress = false;
            endDispatch();
            throw;
        }

        m_fixedUpdateInProgress = false;
        endDispatch();
    }

    void SceneManager::update(float deltaTime)
    {
        fixedUpdate(deltaTime);
    }

    void SceneManager::render(sf::RenderWindow& window)
    {
        render(window, 1.f);
    }

    void SceneManager::render(sf::RenderWindow& window, float interpolationAlpha)
    {
        Scene* activeScene = m_activeScene;

        if (activeScene == nullptr) return;

        beginDispatch();

        try
        {
            activeScene->render(window, interpolationAlpha);
        }
        catch (...)
        {
            endDispatch();
            throw;
        }

        endDispatch();
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

        if (m_dispatchDepth > 0)
        {
            for (const std::unique_ptr<Scene>& scene : m_scenes)
            {
                if (scene != nullptr) scene->clear();
            }

            m_clearDeferred = true;
            return;
        }

        m_clearDeferred = false;
        m_scenes.clear();
    }

    void SceneManager::beginDispatch()
    {
        ++m_dispatchDepth;
    }

    void SceneManager::endDispatch()
    {
        --m_dispatchDepth;

        if (m_dispatchDepth > 0 || !m_clearDeferred) return;

        m_clearDeferred = false;
        m_activeScene = nullptr;
        m_scenes.clear();
    }
}
