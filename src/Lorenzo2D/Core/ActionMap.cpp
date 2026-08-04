#include <Lorenzo2D/Core/ActionMap.hpp>

#include "ActionState.hpp"

#include <algorithm>

namespace l2d
{
    namespace
    {
        core_detail::ActionState aggregateActionState(const std::vector<Key>& keys)
        {
            core_detail::ActionState state;

            for (Key key : keys)
            {
                const bool currentPressed = Input::isKeyPressed(key);
                state.include(currentPressed, core_detail::previousButtonState(
                                                  currentPressed, Input::wasKeyPressed(key),
                                                  Input::wasKeyReleased(key)));
            }

            return state;
        }
    }

    void ActionMap::bindAction(const std::string& actionName, Key key)
    {
        std::vector<Key>& keys = m_actions[actionName];

        if (std::find(keys.begin(), keys.end(), key) == keys.end())
        {
            keys.push_back(key);
        }
    }

    void ActionMap::clearAction(const std::string& actionName)
    {
        m_actions.erase(actionName);
    }

    void ActionMap::clearAll()
    {
        m_actions.clear();
    }

    bool ActionMap::isActionPressed(const std::string& actionName) const
    {
        const std::vector<Key>* keys = findKeys(actionName);

        if (keys == nullptr) return false;

        return aggregateActionState(*keys).isPressed();
    }

    bool ActionMap::wasActionPressed(const std::string& actionName) const
    {
        const std::vector<Key>* keys = findKeys(actionName);

        if (keys == nullptr) return false;

        return aggregateActionState(*keys).wasPressed();
    }

    bool ActionMap::wasActionReleased(const std::string& actionName) const
    {
        const std::vector<Key>* keys = findKeys(actionName);

        if (keys == nullptr) return false;

        return aggregateActionState(*keys).wasReleased();
    }

    const std::vector<Key>* ActionMap::findKeys(const std::string& actionName) const
    {
        auto it = m_actions.find(actionName);

        if (it == m_actions.end()) return nullptr;

        return &it->second;
    }
}
