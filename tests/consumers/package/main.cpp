#include <Lorenzo2D/ECS/Transform.hpp>

int main()
{
    l2d::Transform transform({2.f, 3.f});
    transform.move({4.f, 5.f});

    const sf::Vector2f position = transform.position();

    return position == sf::Vector2f{6.f, 8.f} ? 0 : 1;
}
