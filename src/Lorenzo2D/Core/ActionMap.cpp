#include <Lorenzo2D/Core/ActionMap.hpp>

namespace l2d
{
    void ActionMap::bindAction(const std::string& actionName, Key key)
    {
        m_inputMap.bindButton(actionName, Input::code(key));
    }

    void ActionMap::clearAction(const std::string& actionName)
    {
        m_inputMap.clearAction(actionName);
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
