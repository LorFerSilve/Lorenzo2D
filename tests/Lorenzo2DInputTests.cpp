#include "ActionState.hpp"
#include "TestSupport.hpp"

namespace
{
    using l2d::test::runTest;

    void testActionStateUsesAggregateTransitions()
    {
        using l2d::core_detail::ActionState;

        ActionState idle;
        idle.include(false, false);
        L2D_REQUIRE(!idle.isPressed());
        L2D_REQUIRE(!idle.wasPressed());
        L2D_REQUIRE(!idle.wasReleased());

        ActionState firstKeyPressed;
        firstKeyPressed.include(true, false);
        firstKeyPressed.include(false, false);
        L2D_REQUIRE(firstKeyPressed.isPressed());
        L2D_REQUIRE(firstKeyPressed.wasPressed());
        L2D_REQUIRE(!firstKeyPressed.wasReleased());

        ActionState secondKeyPressedWhileFirstHeld;
        secondKeyPressedWhileFirstHeld.include(true, true);
        secondKeyPressedWhileFirstHeld.include(true, false);
        L2D_REQUIRE(secondKeyPressedWhileFirstHeld.isPressed());
        L2D_REQUIRE(!secondKeyPressedWhileFirstHeld.wasPressed());
        L2D_REQUIRE(!secondKeyPressedWhileFirstHeld.wasReleased());

        ActionState secondKeyReleasedWhileFirstHeld;
        secondKeyReleasedWhileFirstHeld.include(true, true);
        secondKeyReleasedWhileFirstHeld.include(false, true);
        L2D_REQUIRE(secondKeyReleasedWhileFirstHeld.isPressed());
        L2D_REQUIRE(!secondKeyReleasedWhileFirstHeld.wasPressed());
        L2D_REQUIRE(!secondKeyReleasedWhileFirstHeld.wasReleased());

        ActionState finalKeyReleased;
        finalKeyReleased.include(false, true);
        finalKeyReleased.include(false, false);
        L2D_REQUIRE(!finalKeyReleased.isPressed());
        L2D_REQUIRE(!finalKeyReleased.wasPressed());
        L2D_REQUIRE(finalKeyReleased.wasReleased());
    }

    void testPreviousButtonStateReconstructsSnapshots()
    {
        using l2d::core_detail::previousButtonState;

        L2D_REQUIRE(!previousButtonState(false, false, false));
        L2D_REQUIRE(!previousButtonState(true, true, false));
        L2D_REQUIRE(previousButtonState(true, false, false));
        L2D_REQUIRE(previousButtonState(false, false, true));
    }
}

int main()
{
    int failures = 0;

    runTest(
        "action state uses aggregate transitions",
        testActionStateUsesAggregateTransitions,
        failures
    );
    runTest(
        "previous button state reconstructs snapshots",
        testPreviousButtonStateReconstructsSnapshots,
        failures
    );

    if (failures != 0)
    {
        std::cerr << failures << " input test(s) failed.\n";
        return 1;
    }

    std::cout << "All Lorenzo2D input tests passed.\n";
    return 0;
}
