#pragma once

#include <SFML/Graphics.hpp>
#include <string>

namespace l2d
{
    class Application
    {
    public:
        Application(unsigned int width, unsigned int height, const std::string& title);
        virtual ~Application() = default;

        void run();

    protected:
        virtual void onUpdate(float deltaTime);
        virtual void onRender(sf::RenderWindow& window);

        sf::RenderWindow& getWindow();

    private:
        void processEvents();

    private:
        sf::RenderWindow m_window;
        sf::Clock m_clock;
    };
}