#pragma once

#include <Lorenzo2D/Core/FixedStepScheduler.hpp>

#include <SFML/Graphics.hpp>
#include <string>

namespace l2d
{
    struct ApplicationConfig
    {
        FixedStepConfig fixedStep;
        unsigned int frameRateLimit = 60;
    };

    class Application
    {
      public:
        Application(unsigned int width, unsigned int height, const std::string& title);

        Application(unsigned int width, unsigned int height, const std::string& title,
                    const ApplicationConfig& config);

        virtual ~Application() = default;

        void run();
        void requestClose();

      protected:
        virtual void onFrameStart(float deltaTime);

        virtual void onFixedPreSimulation(float fixedDeltaTime);
        virtual void onFixedSimulation(float fixedDeltaTime);
        virtual void onFixedPostSimulation(float fixedDeltaTime);

        virtual void onUpdate(float deltaTime);

        virtual void onRender(sf::RenderWindow& window, float interpolationAlpha);

        virtual void onRender(sf::RenderWindow& window);

        sf::RenderWindow& getWindow();

      private:
        void processEvents();
        bool shouldClose() const;

      private:
        sf::RenderWindow m_window;
        sf::Clock m_clock;
        FixedStepScheduler m_fixedStepScheduler;

        bool m_closeRequested = false;
    };
}
