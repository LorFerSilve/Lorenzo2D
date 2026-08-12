#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/Mouse.hpp>
#include <Lorenzo2D/Core/Pointer.hpp>
#include <Lorenzo2D/Core/Time.hpp>
#include <Lorenzo2D/Core/WindowEvents.hpp>

#include <SFML/Graphics/RenderWindow.hpp>

#include <cmath>
#include <limits>

namespace l2d
{
    namespace
    {
        std::uint64_t addDroppedTicks(std::uint64_t droppedTicks, std::uint32_t abandonedTicks)
        {
            const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();

            if (droppedTicks >= maximum - abandonedTicks) return maximum;

            return droppedTicks + abandonedTicks;
        }

        double addDroppedSimulationTime(double droppedSimulationTime, std::uint32_t abandonedTicks,
                                        double fixedDeltaTime)
        {
            const double abandonedSimulationTime =
                static_cast<double>(abandonedTicks) * fixedDeltaTime;

            const double maximum = std::numeric_limits<double>::max();

            if (!std::isfinite(abandonedSimulationTime) ||
                droppedSimulationTime >= maximum - abandonedSimulationTime)
            {
                return maximum;
            }

            return droppedSimulationTime + abandonedSimulationTime;
        }
    }

    Application::Application(unsigned int width, unsigned int height, const std::string& title)
        : Application(width, height, title, ApplicationConfig{})
    {
    }

    Application::Application(unsigned int width, unsigned int height, const std::string& title,
                             const ApplicationConfig& config)
        : m_window(sf::VideoMode({width, height}), title), m_fixedStepScheduler(config.fixedStep)
    {
        m_window.setFramerateLimit(config.frameRateLimit);
    }

    void Application::run()
    {
        m_fixedStepScheduler.reset();
        Time::reset(m_fixedStepScheduler.config().fixedDeltaTime);
        Input::reset();
        Pointer::reset();

        // Exclude derived-constructor and asset-loading time from the first
        // measured frame.
        m_clock.restart();

        while (m_window.isOpen() && !m_closeRequested)
        {
            const double rawDeltaTime = static_cast<double>(m_clock.restart().asSeconds());

            const FixedStepFrame frame = m_fixedStepScheduler.advance(rawDeltaTime);

            Time::beginFrame(frame.rawDeltaTime, frame.frameDeltaTime);

            WindowEvents::beginFrame();
            Input::beginFrame();
            Pointer::beginFrame();
            processEvents();

            if (!shouldClose())
            {
                Input::update();
                Pointer::update(m_window);
                Mouse::update(m_window);

                onFrameStart(Time::frameDeltaTime());
            }

            if (!shouldClose())
            {
                const float fixedDeltaTime = Time::fixedDeltaTime();

                for (std::uint32_t tick = 0; tick < frame.ticksToRun; ++tick)
                {
                    // A close requested by one fixed phase takes effect only
                    // after the complete simulation tick remains consistent.
                    onFixedPreSimulation(fixedDeltaTime);
                    onFixedSimulation(fixedDeltaTime);
                    onFixedPostSimulation(fixedDeltaTime);

                    Time::completeFixedTick();

                    if (shouldClose()) break;
                }
            }

            const std::uint32_t abandonedTicks = frame.ticksToRun - Time::ticksThisFrame();

            Time::endFrame(frame.interpolationAlpha,
                           addDroppedTicks(frame.droppedTicks, abandonedTicks),
                           addDroppedSimulationTime(frame.droppedSimulationTime, abandonedTicks,
                                                    m_fixedStepScheduler.config().fixedDeltaTime),
                           frame.clampedFrameTime);

            if (shouldClose()) break;

            onUpdate(Time::frameDeltaTime());

            if (shouldClose()) break;

            m_window.clear(sf::Color::Black);

            onRender(m_window, Time::interpolationAlpha());

            if (shouldClose()) break;

            m_window.display();
        }

        if (m_closeRequested && m_window.isOpen()) m_window.close();
    }

    void Application::requestClose()
    {
        m_closeRequested = true;
    }

    void Application::processEvents()
    {
        while (const auto event = m_window.pollEvent())
        {
            WindowEvents::processEvent(*event);
            Input::processEvent(*event);
            Pointer::processEvent(*event);

            if (WindowEvents::closeRequested())
            {
                requestClose();
            }
        }
    }

    bool Application::shouldClose() const
    {
        return m_closeRequested || !m_window.isOpen();
    }

    void Application::onFrameStart(float deltaTime)
    {
        (void)deltaTime;
    }

    void Application::onFixedPreSimulation(float fixedDeltaTime)
    {
        (void)fixedDeltaTime;
    }

    void Application::onFixedSimulation(float fixedDeltaTime)
    {
        (void)fixedDeltaTime;
    }

    void Application::onFixedPostSimulation(float fixedDeltaTime)
    {
        (void)fixedDeltaTime;
    }

    void Application::onUpdate(float deltaTime)
    {
        (void)deltaTime;
    }

    void Application::onRender(sf::RenderWindow& window, float interpolationAlpha)
    {
        (void)interpolationAlpha;

        onRender(window);
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
