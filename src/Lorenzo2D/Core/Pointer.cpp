#include <Lorenzo2D/Core/Pointer.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <cmath>

namespace l2d
{
    PointerState Pointer::s_mouse;
    std::vector<PointerState> Pointer::s_touches;
    float Pointer::s_dragThreshold = 4.f;
    bool Pointer::s_mouseInitialized = false;

    const PointerState& Pointer::primary() noexcept
    {
        return s_mouse;
    }

    const PointerState* Pointer::find(InputDeviceType device, std::uint32_t pointerId) noexcept
    {
        if (device == InputDeviceType::Mouse) return pointerId == 0u ? &s_mouse : nullptr;
        if (device != InputDeviceType::Touch) return nullptr;

        const auto iterator = std::find_if(s_touches.begin(), s_touches.end(),
                                           [pointerId](const PointerState& pointer)
                                           { return pointer.id == pointerId; });
        return iterator == s_touches.end() ? nullptr : &*iterator;
    }

    std::vector<PointerState> Pointer::states()
    {
        std::vector<PointerState> result;
        result.reserve(s_touches.size() + 1u);
        result.push_back(s_mouse);
        result.insert(result.end(), s_touches.begin(), s_touches.end());
        return result;
    }

    sf::Vector2f Pointer::worldPosition(const sf::RenderWindow& window)
    {
        return window.mapPixelToCoords(s_mouse.screenPosition);
    }

    sf::Vector2f Pointer::worldPosition(const sf::RenderWindow& window, const sf::View& view)
    {
        return window.mapPixelToCoords(s_mouse.screenPosition, view);
    }

    sf::Vector2f Pointer::worldPosition(const PointerState& pointer, const sf::RenderWindow& window,
                                        const sf::View& view)
    {
        return window.mapPixelToCoords(pointer.screenPosition, view);
    }

    sf::Vector2f Pointer::worldPosition(const PointerState& pointer, sf::Vector2u targetSize,
                                        const sf::View& view)
    {
        if (targetSize.x == 0u || targetSize.y == 0u) return view.getCenter();

        const sf::FloatRect normalizedViewport = view.getViewport();
        const sf::Vector2f targetSizeFloat{static_cast<float>(targetSize.x),
                                           static_cast<float>(targetSize.y)};
        const sf::Vector2f viewportPosition =
            normalizedViewport.position.componentWiseMul(targetSizeFloat);
        const sf::Vector2f viewportSize = normalizedViewport.size.componentWiseMul(targetSizeFloat);
        if (viewportSize.x == 0.f || viewportSize.y == 0.f) return view.getCenter();

        const sf::Vector2f pixelPosition{static_cast<float>(pointer.screenPosition.x),
                                         static_cast<float>(pointer.screenPosition.y)};
        const sf::Vector2f normalized =
            sf::Vector2f{-1.f, 1.f} + sf::Vector2f{2.f, -2.f}
                                          .componentWiseMul(pixelPosition - viewportPosition)
                                          .componentWiseDiv(viewportSize);
        return view.getInverseTransform().transformPoint(normalized);
    }

    bool Pointer::setDragThreshold(float pixels) noexcept
    {
        if (!std::isfinite(pixels) || pixels < 0.f) return false;
        s_dragThreshold = pixels;
        return true;
    }

    float Pointer::dragThreshold() noexcept
    {
        return s_dragThreshold;
    }

    void Pointer::reset()
    {
        s_mouse = {};
        s_touches.clear();
        s_mouseInitialized = false;
    }

    void Pointer::beginFrame()
    {
        s_touches.erase(std::remove_if(s_touches.begin(), s_touches.end(),
                                       [](const PointerState& pointer)
                                       { return !pointer.down && pointer.released; }),
                        s_touches.end());

        const auto resetFrameState = [](PointerState& pointer)
        {
            pointer.delta = {0, 0};
            pointer.pressed = false;
            pointer.released = false;
            pointer.dragStarted = false;
            pointer.dragEnded = false;
            pointer.wheelDelta = 0.f;
            pointer.horizontalWheelDelta = 0.f;
        };

        resetFrameState(s_mouse);
        for (PointerState& pointer : s_touches)
            resetFrameState(pointer);
    }

    void Pointer::processEvent(const sf::Event& event)
    {
        if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
        {
            updateMousePosition(moved->position);
            return;
        }
        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            updateMousePosition(pressed->position);
            if (pressed->button == sf::Mouse::Button::Left) updateDown(s_mouse, true);
            return;
        }
        if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
        {
            updateMousePosition(released->position);
            if (released->button == sf::Mouse::Button::Left) updateDown(s_mouse, false);
            return;
        }
        if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>())
        {
            updateMousePosition(wheel->position);
            if (wheel->wheel == sf::Mouse::Wheel::Vertical)
                s_mouse.wheelDelta += wheel->delta;
            else
                s_mouse.horizontalWheelDelta += wheel->delta;
            return;
        }
        if (const auto* began = event.getIf<sf::Event::TouchBegan>())
        {
            PointerState& pointer = touchState(began->finger);
            pointer.screenPosition = began->position;
            pointer.delta = {0, 0};
            updateDown(pointer, true);
            return;
        }
        if (const auto* moved = event.getIf<sf::Event::TouchMoved>())
        {
            updatePosition(touchState(moved->finger), moved->position);
            return;
        }
        if (const auto* ended = event.getIf<sf::Event::TouchEnded>())
        {
            PointerState& pointer = touchState(ended->finger);
            updatePosition(pointer, ended->position);
            updateDown(pointer, false);
        }
    }

    void Pointer::update(const sf::RenderWindow& window)
    {
        updateMousePosition(sf::Mouse::getPosition(window));
        updateDown(s_mouse, sf::Mouse::isButtonPressed(sf::Mouse::Button::Left));
    }

    PointerState& Pointer::touchState(std::uint32_t pointerId)
    {
        const auto iterator = std::find_if(s_touches.begin(), s_touches.end(),
                                           [pointerId](const PointerState& pointer)
                                           { return pointer.id == pointerId; });
        if (iterator != s_touches.end()) return *iterator;

        s_touches.push_back({});
        PointerState& pointer = s_touches.back();
        pointer.id = pointerId;
        pointer.device = InputDeviceType::Touch;
        return pointer;
    }

    void Pointer::updateMousePosition(sf::Vector2i position)
    {
        if (!s_mouseInitialized)
        {
            s_mouse.screenPosition = position;
            s_mouseInitialized = true;
            return;
        }

        updatePosition(s_mouse, position);
    }

    void Pointer::updatePosition(PointerState& pointer, sf::Vector2i position)
    {
        pointer.delta += position - pointer.screenPosition;
        pointer.screenPosition = position;
        updateDrag(pointer);
    }

    void Pointer::updateDown(PointerState& pointer, bool isDown)
    {
        if (pointer.down == isDown) return;

        pointer.down = isDown;
        if (isDown)
        {
            pointer.pressed = true;
            pointer.dragOrigin = pointer.screenPosition;
            pointer.dragging = false;
            return;
        }

        pointer.released = true;
        pointer.dragEnded = pointer.dragging;
        pointer.dragging = false;
    }

    void Pointer::updateDrag(PointerState& pointer)
    {
        if (!pointer.down || pointer.dragging) return;

        const sf::Vector2i displacement = pointer.screenPosition - pointer.dragOrigin;
        const double horizontal = static_cast<double>(displacement.x);
        const double vertical = static_cast<double>(displacement.y);
        const double squaredDistance = horizontal * horizontal + vertical * vertical;
        const double threshold = static_cast<double>(s_dragThreshold);
        if (squaredDistance < threshold * threshold) return;

        pointer.dragging = true;
        pointer.dragStarted = true;
    }
}
