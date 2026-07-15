#include "PhysicsWorld2DInternals.hpp"

#include <cstddef>
#include <tuple>

namespace l2d::detail
{
    namespace
    {
        using PhysicsContactKey2D = std::tuple<
            GameObjectId,
            GameObjectId,
            int,
            int
        >;

        PhysicsContactKey2D physicsContactKey(
            const PhysicsContact2D& contact
        )
        {
            return {
                contact.firstObjectId,
                contact.secondObjectId,
                static_cast<int>(contact.firstColliderType),
                static_cast<int>(contact.secondColliderType)
            };
        }

        bool samePhysicsContactKey(
            const PhysicsContact2D& left,
            const PhysicsContact2D& right
        )
        {
            return physicsContactKey(left) == physicsContactKey(right);
        }
    }

    bool physicsContactLess(
        const PhysicsContact2D& left,
        const PhysicsContact2D& right
    )
    {
        return physicsContactKey(left) < physicsContactKey(right);
    }

    void buildPhysicsContactEvents(
        const std::vector<PhysicsContact2D>& previousContacts,
        const std::vector<PhysicsContact2D>& currentContacts,
        std::vector<PhysicsContactEvent2D>& events
    )
    {
        events.clear();

        std::size_t previousIndex = 0;
        std::size_t currentIndex = 0;

        while (
            previousIndex < previousContacts.size() ||
            currentIndex < currentContacts.size()
        )
        {
            if (previousIndex >= previousContacts.size())
            {
                events.push_back({
                    PhysicsContactPhase2D::Begin,
                    currentContacts[currentIndex]
                });
                ++currentIndex;
                continue;
            }

            if (currentIndex >= currentContacts.size())
            {
                events.push_back({
                    PhysicsContactPhase2D::End,
                    previousContacts[previousIndex]
                });
                ++previousIndex;
                continue;
            }

            const PhysicsContact2D& previous =
                previousContacts[previousIndex];
            const PhysicsContact2D& current =
                currentContacts[currentIndex];

            if (samePhysicsContactKey(previous, current))
            {
                events.push_back({
                    PhysicsContactPhase2D::Stay,
                    current
                });
                ++previousIndex;
                ++currentIndex;
            }
            else if (physicsContactLess(previous, current))
            {
                events.push_back({
                    PhysicsContactPhase2D::End,
                    previous
                });
                ++previousIndex;
            }
            else
            {
                events.push_back({
                    PhysicsContactPhase2D::Begin,
                    current
                });
                ++currentIndex;
            }
        }
    }
}
