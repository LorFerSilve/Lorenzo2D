#include <Lorenzo2D/Core/InputContextStack.hpp>

#include <algorithm>
#include <utility>

namespace l2d
{
    InputContextStack::InputContextStack(const InputSnapshot& snapshot) noexcept
        : m_snapshot(&snapshot)
    {
    }

    void InputContextStack::setSnapshot(const InputSnapshot& snapshot) noexcept
    {
        m_snapshot = &snapshot;
        for (auto& context : m_contexts)
            context.second.setSnapshot(snapshot);
    }

    void InputContextStack::clearSnapshot() noexcept
    {
        m_snapshot = nullptr;
        for (auto& context : m_contexts)
            context.second.clearSnapshot();
    }

    bool InputContextStack::hasSnapshot() const noexcept
    {
        return m_snapshot != nullptr;
    }

    InputMap& InputContextStack::createContext(const std::string& contextName)
    {
        auto result = m_contexts.try_emplace(contextName);
        if (result.second && m_snapshot != nullptr) result.first->second.setSnapshot(*m_snapshot);
        return result.first->second;
    }

    bool InputContextStack::removeContext(const std::string& contextName)
    {
        if (m_contexts.erase(contextName) == 0u) return false;

        m_activeContexts.erase(std::remove_if(m_activeContexts.begin(), m_activeContexts.end(),
                                              [&contextName](const ActiveContext& context)
                                              { return context.name == contextName; }),
                               m_activeContexts.end());
        return true;
    }

    InputMap* InputContextStack::findContext(const std::string& contextName)
    {
        const auto iterator = m_contexts.find(contextName);
        return iterator == m_contexts.end() ? nullptr : &iterator->second;
    }

    const InputMap* InputContextStack::findContext(const std::string& contextName) const
    {
        const auto iterator = m_contexts.find(contextName);
        return iterator == m_contexts.end() ? nullptr : &iterator->second;
    }

    bool InputContextStack::pushContext(const std::string& contextName, bool blocksLowerContexts)
    {
        if (contextName.empty() || findContext(contextName) == nullptr) return false;

        popContext(contextName);
        m_activeContexts.push_back({contextName, blocksLowerContexts});
        return true;
    }

    bool InputContextStack::popContext()
    {
        if (m_activeContexts.empty()) return false;
        m_activeContexts.pop_back();
        return true;
    }

    bool InputContextStack::popContext(const std::string& contextName)
    {
        const auto iterator = std::find_if(m_activeContexts.rbegin(), m_activeContexts.rend(),
                                           [&contextName](const ActiveContext& context)
                                           { return context.name == contextName; });
        if (iterator == m_activeContexts.rend()) return false;

        m_activeContexts.erase(std::next(iterator).base());
        return true;
    }

    void InputContextStack::clearActiveContexts()
    {
        m_activeContexts.clear();
    }

    std::size_t InputContextStack::activeContextCount() const noexcept
    {
        return m_activeContexts.size();
    }

    const std::string* InputContextStack::topContextName() const noexcept
    {
        return m_activeContexts.empty() ? nullptr : &m_activeContexts.back().name;
    }

    bool InputContextStack::down(const std::string& actionName) const
    {
        const InputMap* map = resolveMap(actionName);
        return map != nullptr && map->down(actionName);
    }

    bool InputContextStack::pressed(const std::string& actionName) const
    {
        const InputMap* map = resolveMap(actionName);
        return map != nullptr && map->pressed(actionName);
    }

    bool InputContextStack::released(const std::string& actionName) const
    {
        const InputMap* map = resolveMap(actionName);
        return map != nullptr && map->released(actionName);
    }

    bool InputContextStack::consumePressed(const std::string& actionName)
    {
        InputMap* map = resolveMap(actionName);
        return map != nullptr && map->consumePressed(actionName);
    }

    bool InputContextStack::consumeReleased(const std::string& actionName)
    {
        InputMap* map = resolveMap(actionName);
        return map != nullptr && map->consumeReleased(actionName);
    }

    float InputContextStack::axis1D(const std::string& actionName) const
    {
        const InputMap* map = resolveMap(actionName);
        return map == nullptr ? 0.f : map->axis1D(actionName);
    }

    sf::Vector2f InputContextStack::axis2D(const std::string& actionName) const
    {
        const InputMap* map = resolveMap(actionName);
        return map == nullptr ? sf::Vector2f{0.f, 0.f} : map->axis2D(actionName);
    }

    InputMap* InputContextStack::resolveMap(const std::string& actionName)
    {
        return const_cast<InputMap*>(std::as_const(*this).resolveMap(actionName));
    }

    const InputMap* InputContextStack::resolveMap(const std::string& actionName) const
    {
        for (auto iterator = m_activeContexts.rbegin(); iterator != m_activeContexts.rend();
             ++iterator)
        {
            const InputMap* map = findContext(iterator->name);
            if (map != nullptr && map->hasAction(actionName)) return map;
            if (iterator->blocksLowerContexts) return nullptr;
        }

        return nullptr;
    }
}
