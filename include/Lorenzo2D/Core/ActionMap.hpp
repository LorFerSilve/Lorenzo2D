#pragma once

#include <Lorenzo2D/Core/Input.hpp>

#include <string>
#include <unordered_map>
#include <vector>

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
        const std::vector<Key>* findKeys(const std::string& actionName) const;

      private:
        std::unordered_map<std::string, std::vector<Key>> m_actions;
    };
}
