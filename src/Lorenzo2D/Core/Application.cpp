#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/Mouse.hpp>
#include <Lorenzo2D/Core/Time.hpp>
#include <Lorenzo2D/Core/WindowEvents.hpp>

#include <SFML/Graphics/RenderWindow.hpp>

namespace l2d
{
    Application::Application(unsigned int width, unsigned int height, const std::string& title)
        : m_window(sf::VideoMode({ width, height }), title)
    {
        m_window.setFramerateLimit(60);
    }

    void Application::run()
    {
        while (m_window.isOpen())
        {
            const float deltaTime = m_clock.restart().asSeconds();

            Time::update(deltaTime);

            WindowEvents::beginFrame();
            processEvents();

            if (!m_window.isOpen())
                break;

            Input::update();
            Mouse::update(m_window);

            onUpdate(Time::deltaTime());

            if (!m_window.isOpen())
                break;

            m_window.clear(sf::Color::Black);

            onRender(m_window);

            m_window.display();
        }
    }

    void Application::processEvents()
    {
        while (const auto event = m_window.pollEvent())
        {
            WindowEvents::processEvent(*event);

            if (WindowEvents::closeRequested())
            {
                m_window.close();
            }
        }
    }

    void Application::onUpdate(float deltaTime)
    {
        (void)deltaTime;
    }

    void Application::onRender(sf::RenderWindow& window)
    {
        (void)window;
    }

    sf::RenderWindow& Application::getWindow()
    {
        return m_window;
    }
}
