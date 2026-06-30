#include <Lorenzo2D/Core/ActionMap.hpp>

#include <algorithm>

namespace l2d
{
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

        if (keys == nullptr)
            return false;

        for (Key key : *keys)
        {
            if (Input::isKeyPressed(key))
                return true;
        }

        return false;
    }

    bool ActionMap::wasActionPressed(const std::string& actionName) const
    {
        const std::vector<Key>* keys = findKeys(actionName);

        if (keys == nullptr)
            return false;

        for (Key key : *keys)
        {
            if (Input::wasKeyPressed(key))
                return true;
        }

        return false;
    }

    bool ActionMap::wasActionReleased(const std::string& actionName) const
    {
        const std::vector<Key>* keys = findKeys(actionName);

        if (keys == nullptr)
            return false;

        for (Key key : *keys)
        {
            if (Input::wasKeyReleased(key))
                return true;
        }

        return false;
    }

    const std::vector<Key>* ActionMap::findKeys(const std::string& actionName) const
    {
        auto it = m_actions.find(actionName);

        if (it == m_actions.end())
            return nullptr;

        return &it->second;
    }
}