#include <Lorenzo2D/Core/ActionMap.hpp>

namespace l2d
{
    bool ActionMap::tryBindAction(const std::string& actionName, Key key)
    {
        return m_inputMap.bindButton(actionName, Input::code(key));
    }

    bool ActionMap::tryClearAction(const std::string& actionName)
    {
        return m_inputMap.clearAction(actionName);
    }

    void ActionMap::bindAction(const std::string& actionName, Key key)
    {
        (void)tryBindAction(actionName, key);
    }

    void ActionMap::clearAction(const std::string& actionName)
    {
        (void)tryClearAction(actionName);
    }

    void ActionMap::clearAll()
    {
        m_inputMap.clearAll();
    }

    bool ActionMap::isActionPressed(const std::string& actionName) const
    {
        return m_inputMap.down(actionName);
    }

    bool ActionMap::wasActionPressed(const std::string& actionName) const
    {
        return m_inputMap.pressed(actionName);
    }

    bool ActionMap::wasActionReleased(const std::string& actionName) const
    {
        return m_inputMap.released(actionName);
    }
}
