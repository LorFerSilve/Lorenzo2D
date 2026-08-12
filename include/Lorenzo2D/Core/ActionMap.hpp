#pragma once

#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/InputMap.hpp>

#include <string>

namespace l2d
{
    class ActionMap
    {
      public:
        void bindAction(const std::string& actionName, Key key);
        void clearAction(const std::string& actionName);
        void clearAll();

        bool isActionPressed(const std::string& actionName) const;
        bool wasActionPressed(const std::string& actionName) const;
        bool wasActionReleased(const std::string& actionName) const;

      private:
        InputMap m_inputMap{Input::snapshot()};
    };
}
