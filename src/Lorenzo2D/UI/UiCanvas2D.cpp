#include <Lorenzo2D/UI/UiCanvas2D.hpp>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/View.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace l2d
{
    namespace
    {
        constexpr std::size_t InvalidIndex = std::numeric_limits<std::size_t>::max();

        bool finite(sf::Vector2f value) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        bool validStyle(const UiButtonStyle2D& style) noexcept
        {
            return style.characterSize > 0u && finite(style.textOffset);
        }

        bool contains(const UiButton2D& button, sf::Vector2f point) noexcept
        {
            const double minimumX = static_cast<double>(button.position.x);
            const double minimumY = static_cast<double>(button.position.y);
            const double maximumX = minimumX + static_cast<double>(button.size.x);
            const double maximumY = minimumY + static_cast<double>(button.size.y);
            const double x = static_cast<double>(point.x);
            const double y = static_cast<double>(point.y);
            return x >= minimumX && x <= maximumX && y >= minimumY && y <= maximumY;
        }
    }

    bool UiCanvas2D::isValidButton(const UiButton2D& button) noexcept
    {
        return !button.id.empty() && finite(button.position) && finite(button.size) &&
               button.size.x > 0.f && button.size.y > 0.f && validStyle(button.style);
    }

    bool UiCanvas2D::addButton(UiButton2D button)
    {
        if (!isValidButton(button) || hasButton(button.id)) return false;

        m_buttons.push_back(std::move(button));
        return true;
    }

    bool UiCanvas2D::removeButton(std::string_view id)
    {
        const std::size_t index = indexOf(id);
        if (index == InvalidIndex) return false;

        m_buttons.erase(m_buttons.begin() + static_cast<std::ptrdiff_t>(index));
        cancelInteraction();
        m_activated.clear();
        return true;
    }

    void UiCanvas2D::clear() noexcept
    {
        m_buttons.clear();
        cancelInteraction();
        m_activated.clear();
    }

    bool UiCanvas2D::hasButton(std::string_view id) const noexcept
    {
        return indexOf(id) != InvalidIndex;
    }

    std::size_t UiCanvas2D::buttonCount() const noexcept
    {
        return m_buttons.size();
    }

    bool UiCanvas2D::setButtonEnabled(std::string_view id, bool enabled)
    {
        const std::size_t index = indexOf(id);
        if (index == InvalidIndex) return false;

        m_buttons[index].enabled = enabled;
        if (!enabled && ((m_hovered && *m_hovered == index) || (m_pressed && *m_pressed == index)))
            cancelInteraction();
        return true;
    }

    bool UiCanvas2D::setButtonVisible(std::string_view id, bool visible)
    {
        const std::size_t index = indexOf(id);
        if (index == InvalidIndex) return false;

        m_buttons[index].visible = visible;
        if (!visible && ((m_hovered && *m_hovered == index) || (m_pressed && *m_pressed == index)))
            cancelInteraction();
        return true;
    }

    bool UiCanvas2D::setButtonLabel(std::string_view id, std::string label)
    {
        const std::size_t index = indexOf(id);
        if (index == InvalidIndex) return false;

        m_buttons[index].label = std::move(label);
        return true;
    }

    bool UiCanvas2D::setButtonBounds(std::string_view id, sf::Vector2f position, sf::Vector2f size)
    {
        const std::size_t index = indexOf(id);
        if (index == InvalidIndex || !finite(position) || !finite(size) || size.x <= 0.f ||
            size.y <= 0.f)
            return false;

        m_buttons[index].position = position;
        m_buttons[index].size = size;
        return true;
    }

    void UiCanvas2D::update(const PointerState& pointer)
    {
        m_activated.clear();
        m_pointerDown = pointer.down;

        const sf::Vector2f screenPosition{static_cast<float>(pointer.screenPosition.x),
                                         static_cast<float>(pointer.screenPosition.y)};
        m_hovered = hitTest(screenPosition);

        if (pointer.pressed)
            m_pressed = m_hovered;

        if (pointer.released)
        {
            if (m_pressed && m_hovered && *m_pressed == *m_hovered)
                m_activated.push_back(m_buttons[*m_pressed].id);

            m_pressed.reset();
            m_pointerDown = false;
        }
        else if (!pointer.down && !pointer.pressed)
        {
            m_pressed.reset();
        }
    }

    void UiCanvas2D::cancelInteraction() noexcept
    {
        m_hovered.reset();
        m_pressed.reset();
        m_pointerDown = false;
    }

    bool UiCanvas2D::wasActivated(std::string_view id) const noexcept
    {
        return std::find(m_activated.begin(), m_activated.end(), id) != m_activated.end();
    }

    std::optional<std::string> UiCanvas2D::hoveredButton() const
    {
        return m_hovered ? std::optional<std::string>(m_buttons[*m_hovered].id) : std::nullopt;
    }

    std::optional<std::string> UiCanvas2D::pressedButton() const
    {
        return m_pressed ? std::optional<std::string>(m_buttons[*m_pressed].id) : std::nullopt;
    }

    void UiCanvas2D::render(sf::RenderWindow& window, FontHandle font) const
    {
        const sf::View previousView = window.getView();
        window.setView(window.getDefaultView());

        for (std::size_t index = 0u; index < m_buttons.size(); ++index)
        {
            const UiButton2D& button = m_buttons[index];
            if (!button.visible) continue;

            sf::Color color = button.style.idleColor;
            if (!button.enabled)
                color = button.style.disabledColor;
            else if (m_pressed && *m_pressed == index && m_pointerDown)
                color = button.style.pressedColor;
            else if (m_hovered && *m_hovered == index)
                color = button.style.hoveredColor;

            sf::RectangleShape rectangle(button.size);
            rectangle.setPosition(button.position);
            rectangle.setFillColor(color);
            window.draw(rectangle);

            if (font && !button.label.empty())
            {
                sf::Text text(*font, button.label, button.style.characterSize);
                text.setFillColor(button.style.textColor);
                text.setPosition(button.position + button.style.textOffset);
                window.draw(text);
            }
        }

        window.setView(previousView);
    }

    std::size_t UiCanvas2D::indexOf(std::string_view id) const noexcept
    {
        for (std::size_t index = 0u; index < m_buttons.size(); ++index)
            if (m_buttons[index].id == id) return index;

        return InvalidIndex;
    }

    std::optional<std::size_t> UiCanvas2D::hitTest(sf::Vector2f screenPosition) const noexcept
    {
        if (!finite(screenPosition)) return std::nullopt;

        for (std::size_t reverseIndex = m_buttons.size(); reverseIndex > 0u; --reverseIndex)
        {
            const std::size_t index = reverseIndex - 1u;
            const UiButton2D& button = m_buttons[index];
            if (button.visible && button.enabled && contains(button, screenPosition)) return index;
        }

        return std::nullopt;
    }
}
