#pragma once

#include <Lorenzo2D/Core/InputMap.hpp>

#include <SFML/System/Vector2.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace l2d
{
    class InputContextStack
    {
      public:
        InputContextStack() = default;
        explicit InputContextStack(const InputSnapshot& snapshot) noexcept;

        void setSnapshot(const InputSnapshot& snapshot) noexcept;

        InputMap& createContext(const std::string& contextName);
        bool removeContext(const std::string& contextName);
        [[nodiscard]] InputMap* findContext(const std::string& contextName);
        [[nodiscard]] const InputMap* findContext(const std::string& contextName) const;

        bool pushContext(const std::string& contextName, bool blocksLowerContexts = true);
        bool popContext();
        bool popContext(const std::string& contextName);
        void clearActiveContexts();

        [[nodiscard]] std::size_t activeContextCount() const noexcept;
        [[nodiscard]] const std::string* topContextName() const noexcept;

        [[nodiscard]] bool down(const std::string& actionName) const;
        [[nodiscard]] bool pressed(const std::string& actionName) const;
        [[nodiscard]] bool released(const std::string& actionName) const;
        bool consumePressed(const std::string& actionName);
        bool consumeReleased(const std::string& actionName);
        [[nodiscard]] float axis1D(const std::string& actionName) const;
        [[nodiscard]] sf::Vector2f axis2D(const std::string& actionName) const;

      private:
        struct ActiveContext
        {
            std::string name;
            bool blocksLowerContexts = true;
        };

        [[nodiscard]] InputMap* resolveMap(const std::string& actionName);
        [[nodiscard]] const InputMap* resolveMap(const std::string& actionName) const;

      private:
        const InputSnapshot* m_snapshot = nullptr;
        std::unordered_map<std::string, InputMap> m_contexts;
        std::vector<ActiveContext> m_activeContexts;
    };
}
