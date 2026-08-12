#pragma once

#include <Lorenzo2D/Core/InputCode.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <vector>

namespace sf
{
    class Event;
    class RenderWindow;
    class View;
}

namespace l2d
{
    class Application;

    struct PointerState
    {
        std::uint32_t id = 0u;
        InputDeviceType device = InputDeviceType::Mouse;
        sf::Vector2i screenPosition{0, 0};
        sf::Vector2i delta{0, 0};
        sf::Vector2i dragOrigin{0, 0};
        bool down = false;
        bool pressed = false;
        bool released = false;
        bool dragging = false;
        bool dragStarted = false;
        bool dragEnded = false;
        float wheelDelta = 0.f;
        float horizontalWheelDelta = 0.f;
    };

    class Pointer
    {
      public:
        [[nodiscard]] static const PointerState& primary() noexcept;
        [[nodiscard]] static const PointerState* find(InputDeviceType device,
                                                      std::uint32_t pointerId) noexcept;
        [[nodiscard]] static std::vector<PointerState> states();

        [[nodiscard]] static sf::Vector2f worldPosition(const sf::RenderWindow& window);
        [[nodiscard]] static sf::Vector2f worldPosition(const sf::RenderWindow& window,
                                                        const sf::View& view);
        [[nodiscard]] static sf::Vector2f worldPosition(const PointerState& pointer,
                                                        const sf::RenderWindow& window,
                                                        const sf::View& view);
        [[nodiscard]] static sf::Vector2f worldPosition(const PointerState& pointer,
                                                        sf::Vector2u targetSize,
                                                        const sf::View& view);

        static bool setDragThreshold(float pixels) noexcept;
        [[nodiscard]] static float dragThreshold() noexcept;

      private:
        static void reset();
        static void beginFrame();
        static void processEvent(const sf::Event& event);
        static void update(const sf::RenderWindow& window);

        [[nodiscard]] static PointerState& touchState(std::uint32_t pointerId);
        static void updateMousePosition(sf::Vector2i position);
        static void updatePosition(PointerState& pointer, sf::Vector2i position);
        static void updateDown(PointerState& pointer, bool down);
        static void updateDrag(PointerState& pointer);

      private:
        static PointerState s_mouse;
        static std::vector<PointerState> s_touches;
        static float s_dragThreshold;
        static bool s_mouseInitialized;

        friend class Application;
    };
}
