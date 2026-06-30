#pragma once

#include <Lorenzo2D/Scene/Scene.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace sf
{
    class RenderWindow;
}

namespace l2d
{
    class SceneManager
    {
    public:
        Scene& createScene(const std::string& name = "Scene");

        bool setActiveScene(const std::string& name);

        Scene* activeScene();
        const Scene* activeScene() const;

        Scene* findSceneByName(const std::string& name);
        const Scene* findSceneByName(const std::string& name) const;

        std::size_t sceneCount() const;

        void update(float deltaTime);
        void render(sf::RenderWindow& window);

        void destroyQueuedGameObjects();
        std::size_t destroyQueuedGameObjectCount() const;

        void clear();

    private:
        std::vector<std::unique_ptr<Scene>> m_scenes;
        Scene* m_activeScene = nullptr;
    };
}