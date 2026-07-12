#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/Core/ActionMap.hpp>
#include <Lorenzo2D/Core/Application.hpp>
#include <Lorenzo2D/Core/Input.hpp>
#include <Lorenzo2D/Core/Mouse.hpp>
#include <Lorenzo2D/Core/Time.hpp>
#include <Lorenzo2D/Core/WindowEvents.hpp>
#include <Lorenzo2D/ECS/Component.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/BoxCollider2D.hpp>
#include <Lorenzo2D/Physics/CircleCollider2D.hpp>
#include <Lorenzo2D/Physics/PhysicsDebugRenderer2D.hpp>
#include <Lorenzo2D/Physics/PhysicsWorld2D.hpp>
#include <Lorenzo2D/Physics/RigidBody2D.hpp>
#include <Lorenzo2D/Renderer/Camera2D.hpp>
#include <Lorenzo2D/Renderer/CircleRenderer.hpp>
#include <Lorenzo2D/Renderer/DebugOverlay.hpp>
#include <Lorenzo2D/Renderer/OrthographicCameraController2D.hpp>
#include <Lorenzo2D/Renderer/RectangleRenderer.hpp>
#include <Lorenzo2D/Renderer/RenderLayerStack2D.hpp>
#include <Lorenzo2D/Renderer/SpriteRenderer.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>
#include <Lorenzo2D/Scene/SceneManager.hpp>
#include <Lorenzo2D/Scene/GameObjectHandle.hpp>
#include <Lorenzo2D/Tilemap/Tilemap.hpp>

#include <SFML/Graphics.hpp>    

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

#ifndef L2D_ASSET_ROOT
#define L2D_ASSET_ROOT "assets"
#endif

namespace GameActions
{
    constexpr const char* MoveLeft = "MoveLeft";
    constexpr const char* MoveRight = "MoveRight";
    constexpr const char* Jump = "Jump";
    constexpr const char* TogglePhysicsDebug = "TogglePhysicsDebug";
}

namespace GameTags
{
    constexpr const char* Player = "Player";
    constexpr const char* Coin = "Coin";
    constexpr const char* Enemy = "Enemy";
}

class PlayerController : public l2d::Component
{
public:
    explicit PlayerController(const l2d::ActionMap& actions)
        : m_actions(&actions)
    {
    }

    void onUpdate(float deltaTime) override
    {
        (void)deltaTime;

        l2d::GameObject* gameObject = owner();

        if (gameObject == nullptr)
            return;

        l2d::RigidBody2D* rigidBody =
            gameObject->getComponent<l2d::RigidBody2D>();

        if (rigidBody == nullptr)
            return;

        constexpr float moveSpeed = 350.f;
        constexpr float jumpSpeed = 650.f;

        sf::Vector2f velocity = rigidBody->velocity();

        velocity.x = 0.f;

        if (m_actions != nullptr && m_actions->isActionPressed(GameActions::MoveLeft))
            velocity.x -= moveSpeed;

        if (m_actions != nullptr && m_actions->isActionPressed(GameActions::MoveRight))
            velocity.x += moveSpeed;

        if (
            m_actions != nullptr &&
            m_actions->wasActionPressed(GameActions::Jump) &&
            rigidBody->isGrounded()
            )
        {
            velocity.y = -jumpSpeed;
        }

        rigidBody->setVelocity(velocity);
    }

private:
    const l2d::ActionMap* m_actions;
};

class CollisionColorDebug : public l2d::Component
{
public:
    void onRender(sf::RenderWindow& window) override
    {
        (void)window;

        l2d::GameObject* gameObject = owner();

        if (gameObject == nullptr)
            return;

        auto* collider = gameObject->getComponent<l2d::CircleCollider2D>();

        if (collider == nullptr)
            return;

        const bool colliding = collider->isColliding();

        if (auto* circleRenderer = gameObject->getComponent<l2d::CircleRenderer>())
        {
            circleRenderer->setFillColor(colliding ? sf::Color::Red : sf::Color::Green);
        }

        if (auto* spriteRenderer = gameObject->getComponent<l2d::SpriteRenderer>())
        {
            spriteRenderer->setColor(colliding ? sf::Color::Red : sf::Color::White);
        }
    }
};

class EnemyPatrol : public l2d::Component
{
public:
    EnemyPatrol(float speed = 80.f, float patrolDistance = 120.f)
        : m_speed(speed),
        m_patrolDistance(patrolDistance),
        m_direction(1.f),
        m_initialized(false),
        m_startPosition(0.f, 0.f)
    {
    }

    void onUpdate(float deltaTime) override
    {
        (void)deltaTime;

        l2d::GameObject* gameObject = owner();

        if (gameObject == nullptr)
            return;

        auto* rigidBody = gameObject->getComponent<l2d::RigidBody2D>();

        if (rigidBody == nullptr)
            return;

        if (!m_initialized)
        {
            m_startPosition = gameObject->transform.position();
            m_initialized = true;
        }

        const float distanceFromStart =
            gameObject->transform.position().x - m_startPosition.x;

        if (distanceFromStart > m_patrolDistance)
        {
            m_direction = -1.f;
        }
        else if (distanceFromStart < -m_patrolDistance)
        {
            m_direction = 1.f;
        }

        sf::Vector2f velocity = rigidBody->velocity();

        velocity.x = m_direction * m_speed;

        rigidBody->setVelocity(velocity);
    }

private:
    float m_speed;
    float m_patrolDistance;
    float m_direction;

    bool m_initialized;
    sf::Vector2f m_startPosition;
};

class SandboxApp : public l2d::Application
{
public:
    SandboxApp()
        : l2d::Application(1280, 720, "Lorenzo2D Engine"),
        m_camera({ 1280.f, 720.f }),
        m_cameraController(m_camera)
    {
        m_workingDirectory = std::filesystem::current_path().string();

        setupInputActions();
        setupAssets();

        m_levelScene = &m_sceneManager.createScene("Level 1");

        createLevel();
        createPlayer();
        createCoins();
        createEnemies();

        m_camera.setBounds({ 0.f, 0.f }, m_tileMap.worldSize());
        m_camera.setFollowSmoothness(6.f);

        m_cameraController.setZoomLimits(0.5f, 2.0f);
        m_cameraController.setZoomStepFactors(0.90f, 1.10f);

        snapCameraToPlayer();

        setupDebugOverlay();
    }

protected:
    void onUpdate(float deltaTime) override
    {
        (void)deltaTime;

        const float dt = l2d::Time::deltaTime();

        if (m_actions.wasActionPressed(GameActions::TogglePhysicsDebug))
        {
            const bool enabled =
                !m_renderLayers.isLayerEnabled(l2d::RenderLayer2D::PhysicsDebug);

            m_renderLayers.setLayerEnabled(
                l2d::RenderLayer2D::PhysicsDebug,
                enabled
            );

            m_physicsDebugRenderer.setEnabled(enabled);
        }

        m_sceneManager.update(dt);

        if (m_levelScene != nullptr)
        {
            m_physicsWorld.step(*m_levelScene, dt);
        }

        collectCoins();
        checkEnemyCollisions();

        m_sceneManager.destroyQueuedGameObjects();

        if (m_respawnCooldown > 0.f)
        {
            m_respawnCooldown -= dt;
            if (m_respawnCooldown < 0.f)
                m_respawnCooldown = 0.f;
        }

        updateCamera(dt);
        updateMouseDebug();
        updateWindowEventDebug();
        updateDebug();
    }

    void onRender(sf::RenderWindow& window) override
    {
        if (m_renderLayers.isLayerEnabled(l2d::RenderLayer2D::World))
        {
            m_camera.applyTo(window);

            m_sceneManager.render(window);
        }

        if (m_renderLayers.isLayerEnabled(l2d::RenderLayer2D::PhysicsDebug))
        {
            m_camera.applyTo(window);

            if (m_levelScene != nullptr)
            {
                m_physicsDebugRenderer.render(*m_levelScene, window);
            }
        }

        if (m_renderLayers.isLayerEnabled(l2d::RenderLayer2D::UI))
        {
            window.setView(window.getDefaultView());

            m_debugOverlay.render(window);
        }
    }

private:
    void createPlayer()
    {
        if (m_levelScene == nullptr)
            return;

        constexpr float playerRadius = 50.f;
        constexpr float playerDiameter = playerRadius * 2.f;

        sf::Vector2f spawnCenter = { 200.f, 120.f };

        const bool foundSpawn =
            m_tileMap.findFirstTilePosition('P', spawnCenter, true);

        if (!foundSpawn)
        {
            spawnCenter = { 200.f, 120.f };
        }

        const sf::Vector2f playerTopLeft =
        {
            spawnCenter.x - playerRadius,
            spawnCenter.y - playerRadius
        };

        m_playerSpawnPosition = playerTopLeft;

        l2d::GameObject& player =
            m_levelScene->createGameObject("Player");

        player.setTag(GameTags::Player);
        player.transform.setPosition(m_playerSpawnPosition);

        m_playerHandle = m_levelScene->createHandle(player);

        player.addComponent<PlayerController>(m_actions);

        l2d::RigidBody2D& rigidBody =
            player.addComponent<l2d::RigidBody2D>();

        rigidBody.setUseGravity(true);
        rigidBody.setGravityScale(1.f);

        player.addComponent<l2d::CircleCollider2D>(playerRadius);

        player.addComponent<CollisionColorDebug>();

        const sf::Texture* playerTexture =
            m_assets.getTexture("player");

        if (playerTexture != nullptr)
        {
            l2d::SpriteRenderer& spriteRenderer =
                player.addComponent<l2d::SpriteRenderer>(*playerTexture);

            spriteRenderer.setSize({ playerDiameter, playerDiameter });
        }
        else
        {
            player.addComponent<l2d::CircleRenderer>(
                playerRadius,
                sf::Color::Green
            );
        }
    }

    void createCoins()
    {
        if (m_levelScene == nullptr)
            return;

        constexpr float coinRadius = 12.f;
        constexpr float coinDiameter = coinRadius * 2.f;

        const std::vector<sf::Vector2f> coinCenters =
            m_tileMap.findTilePositions('C', true);

        int coinIndex = 0;

        for (const sf::Vector2f& coinCenter : coinCenters)
        {
            l2d::GameObject& coin = m_levelScene->createGameObject(
                "Coin_" + std::to_string(coinIndex)
            );

            coin.setTag(GameTags::Coin);

            const sf::Vector2f coinTopLeft =
            {
                coinCenter.x - coinRadius,
                coinCenter.y - coinRadius
            };

            coin.transform.setPosition(coinTopLeft);

            coin.addComponent<l2d::CircleCollider2D>(coinRadius);

            if (const sf::Texture* coinTexture = m_assets.getTexture("coin"))
            {
                l2d::SpriteRenderer& spriteRenderer =
                    coin.addComponent<l2d::SpriteRenderer>(*coinTexture);

                spriteRenderer.setSize({ coinDiameter, coinDiameter });
            }
            else
            {
                coin.addComponent<l2d::CircleRenderer>(
                    coinRadius,
                    sf::Color::Yellow
                );
            }

            coinIndex++;
        }

        m_totalCoins =
            static_cast<int>(m_levelScene->countGameObjectsByTag(GameTags::Coin));
    }

    void createEnemies()
    {
        if (m_levelScene == nullptr)
            return;

        constexpr float enemyRadius = 50.f;
        constexpr float enemyDiameter = enemyRadius * 2.f;

        const std::vector<sf::Vector2f> enemyTilePositions =
            m_tileMap.findTilePositions('E', false);

        int enemyIndex = 0;

        for (const sf::Vector2f& enemyTilePosition : enemyTilePositions)
        {
            l2d::GameObject& enemy = m_levelScene->createGameObject(
                "Enemy_" + std::to_string(enemyIndex)
            );

            enemy.setTag(GameTags::Enemy);

            const sf::Vector2f enemyTopLeft =
            {
                enemyTilePosition.x + (m_tileMap.tileSize().x * 0.5f) - enemyRadius,
                enemyTilePosition.y + m_tileMap.tileSize().y - enemyDiameter
            };

            enemy.transform.setPosition(enemyTopLeft);

            enemy.addComponent<EnemyPatrol>(80.f, 120.f);

            l2d::RigidBody2D& rigidBody = enemy.addComponent<l2d::RigidBody2D>();
            rigidBody.setUseGravity(true);
            rigidBody.setGravityScale(1.f);

            enemy.addComponent<l2d::CircleCollider2D>(enemyRadius);

            enemy.addComponent<l2d::CircleRenderer>(
                enemyRadius,
                sf::Color::Magenta
            );

            enemyIndex++;
        }
    }

    void checkEnemyCollisions()
    {
        if (m_levelScene == nullptr)
            return;

        if (m_respawnCooldown > 0.f)
            return;

        l2d::GameObject* player = m_playerHandle.get();

        if (player == nullptr)
            return;

        l2d::CircleCollider2D* playerCollider =
            player->getComponent<l2d::CircleCollider2D>();

        if (playerCollider == nullptr)
            return;

        const std::vector<l2d::GameObject*> activeEnemies =
            m_levelScene->findActiveGameObjectsByTag(GameTags::Enemy);

        for (l2d::GameObject* enemy : activeEnemies)
        {
            if (enemy == nullptr)
                continue;

            l2d::CircleCollider2D* enemyCollider =
                enemy->getComponent<l2d::CircleCollider2D>();

            if (enemyCollider == nullptr)
                continue;

            if (playerCollider->overlaps(*enemyCollider))
            {
                resetPlayerToSpawn();
                m_deaths++;
                m_respawnCooldown = 1.f;
                return;
            }
        }
    }

    void resetPlayerToSpawn()
    {
        l2d::GameObject* player = m_playerHandle.get();

        if (player == nullptr)
            return;

        player->transform.setPosition(m_playerSpawnPosition);

        l2d::RigidBody2D* rigidBody =
            player->getComponent<l2d::RigidBody2D>();

        if (rigidBody != nullptr)
            rigidBody->setVelocity({ 0.f, 0.f });

        snapCameraToPlayer();
    }

    void collectCoins()
    {
        if (m_levelScene == nullptr)
            return;

        l2d::GameObject* player = m_playerHandle.get();

        if (player == nullptr)
            return;

        l2d::CircleCollider2D* playerCollider =
            player->getComponent<l2d::CircleCollider2D>();

        if (playerCollider == nullptr)
            return;

        const std::vector<l2d::GameObject*> activeCoins =
            m_levelScene->findActiveGameObjectsByTag(GameTags::Coin);

        for (l2d::GameObject* coin : activeCoins)
        {
            if (coin == nullptr)
                continue;

            l2d::CircleCollider2D* coinCollider =
                coin->getComponent<l2d::CircleCollider2D>();

            if (coinCollider == nullptr)
                continue;

            if (playerCollider->overlaps(*coinCollider))
            {
                m_lastDestroyedCoinHandle = m_levelScene->createHandle(*coin);
                m_lastDestroyedCoinId = coin->id();

                m_levelScene->destroyGameObject(*coin);
                m_collectedCoins++;
            }
        }
    }

    void createLevel()
    {
        if (m_levelScene == nullptr)
            return;

        m_tileMap.setTileSize({ 40.f, 40.f });
        m_tileMap.setSolidTileColor(sf::Color::White);

        const std::filesystem::path levelPath =
            std::filesystem::path(L2D_ASSET_ROOT) / "levels" / "level1.txt";

        m_levelLoadedFromFile = m_tileMap.loadFromFile(
            *m_levelScene,
            levelPath.string()
        );

        if (m_levelLoadedFromFile)
        {
            m_levelSource = "FILE: " + levelPath.string();
            return;
        }

        m_levelSource = "FALLBACK: " + levelPath.string() + " not found";

        const l2d::TileMap::Layout fallbackLayout =
        {
            "............................................................",
            "............................................................",
            "............................................................",
            "...P........................................................",
            "............................................................",
            "...............................................C............",
            ".............................................######.........",
            "............................................................",
            "......................................C.....................",
            "....................................######..................",
            "............................................................",
            ".............................C..............................",
            "...........................######...........................",
            "............................................................",
            "....................C.......................................",
            "..................######....................................",
            ".......C.............E......................................",
            ".......######...............................................",
            "............................................................",
            "############################################################",
            "############################################################"
        };

        m_tileMap.loadFromLayout(*m_levelScene, fallbackLayout);
    }

    void setupInputActions()
    {
        m_actions.bindAction(GameActions::MoveLeft, l2d::Key::Q);
        m_actions.bindAction(GameActions::MoveLeft, l2d::Key::Left);

        m_actions.bindAction(GameActions::MoveRight, l2d::Key::D);
        m_actions.bindAction(GameActions::MoveRight, l2d::Key::Right);

        m_actions.bindAction(GameActions::Jump, l2d::Key::Space);
        m_actions.bindAction(GameActions::Jump, l2d::Key::Up);

        m_actions.bindAction(GameActions::TogglePhysicsDebug, l2d::Key::F1);
    }

    void setupAssets()
    {
        const std::filesystem::path assetRoot = L2D_ASSET_ROOT;

        std::vector<std::filesystem::path> fontCandidates =
        {
            assetRoot / "fonts" / "DejaVuSans.ttf"
        };

#if defined(_WIN32)
        fontCandidates.emplace_back("C:/Windows/Fonts/arial.ttf");
        fontCandidates.emplace_back("C:/Windows/Fonts/segoeui.ttf");
#elif defined(__APPLE__)
        fontCandidates.emplace_back(
            "/System/Library/Fonts/Supplemental/Arial.ttf"
        );
#else
        fontCandidates.emplace_back(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
        );
        fontCandidates.emplace_back(
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"
        );
#endif

        for (const std::filesystem::path& fontPath : fontCandidates)
        {
            if (m_assets.loadFont("debug", fontPath.string()))
                break;
        }

        m_assets.loadTexture(
            "player",
            (assetRoot / "textures" / "player.png").string(),
            true
        );

        // Optional. Shape renderers are used when these files are unavailable.
        m_assets.loadTexture(
            "coin",
            (assetRoot / "textures" / "coin.png").string(),
            true
        );
    }

    void setupDebugOverlay()
    {
        if (const sf::Font* font = m_assets.getFont("debug"))
        {
            m_debugOverlay.setFont(*font);
        }

        m_debugOverlay.setPosition({ 10.f, 10.f });
        m_debugOverlay.setCharacterSize(18);
        m_debugOverlay.setFillColor(sf::Color::White);
    }

    sf::Vector2f playerCenter() const
    {
        l2d::GameObject* player = m_playerHandle.get();

        if (player == nullptr)
            return { 0.f, 0.f };

        l2d::CircleCollider2D* playerCollider =
            player->getComponent<l2d::CircleCollider2D>();

        if (playerCollider != nullptr)
            return playerCollider->center();

        return player->transform.position();
    }

    bool isPointInsideCircleCollider(
        const sf::Vector2f& point,
        const l2d::CircleCollider2D& collider
    ) const
    {
        const sf::Vector2f center = collider.center();

        const float dx = point.x - center.x;
        const float dy = point.y - center.y;

        const float distanceSquared = dx * dx + dy * dy;
        const float radius = collider.radius();

        return distanceSquared <= radius * radius;
    }

    bool isPointInsideBoxCollider(
        const sf::Vector2f& point,
        const l2d::BoxCollider2D& collider
    ) const
    {
        const sf::Vector2f minimum = collider.min();
        const sf::Vector2f maximum = collider.max();

        return point.x >= minimum.x
            && point.x <= maximum.x
            && point.y >= minimum.y
            && point.y <= maximum.y;
    }

    bool isPointInsideGameObject(
        const sf::Vector2f& point,
        l2d::GameObject& gameObject
    ) const
    {
        l2d::CircleCollider2D* circleCollider =
            gameObject.getComponent<l2d::CircleCollider2D>();

        if (circleCollider != nullptr)
        {
            return isPointInsideCircleCollider(point, *circleCollider);
        }

        l2d::BoxCollider2D* boxCollider =
            gameObject.getComponent<l2d::BoxCollider2D>();

        if (boxCollider != nullptr)
        {
            return isPointInsideBoxCollider(point, *boxCollider);
        }

        return false;
    }

    void pickObjectAtWorldPosition(const sf::Vector2f& worldPosition)
    {
        if (m_levelScene == nullptr)
        {
            m_selectedObjectHandle.reset();
            return;
        }

        const std::vector<std::unique_ptr<l2d::GameObject>>& gameObjects =
            m_levelScene->gameObjects();

        for (std::size_t index = gameObjects.size(); index > 0; index--)
        {
            l2d::GameObject* gameObject =
                gameObjects[index - 1].get();

            if (gameObject == nullptr)
                continue;

            if (!gameObject->isActive())
                continue;

            if (gameObject->isDestroyQueued())
                continue;

            if (isPointInsideGameObject(worldPosition, *gameObject))
            {
                m_selectedObjectHandle =
                    m_levelScene->createHandle(*gameObject);

                return;
            }
        }

        m_selectedObjectHandle.reset();
    }

    void snapCameraToPlayer()
    {
        if (!m_playerHandle.isValid())
            return;

        m_camera.setCenter(playerCenter());
    }

    void updateCamera(float deltaTime)
    {
        if (m_playerHandle.isValid())
        {
            m_cameraController.setFollowTarget(playerCenter());
        }
        else
        {
            m_cameraController.clearFollowTarget();
        }

        m_cameraController.update(deltaTime);
    }

    void updateMouseDebug()
    {
        m_mouseScreenPosition =
            l2d::Mouse::screenPosition();

        m_mouseWorldPosition =
            l2d::Mouse::worldPosition(getWindow(), m_camera.view());

        if (l2d::Mouse::wasButtonPressed(l2d::MouseButton::Left))
        {
            m_leftMouseClicks++;
            pickObjectAtWorldPosition(m_mouseWorldPosition);
        }
    }

    void updateWindowEventDebug()
    {
        if (l2d::WindowEvents::wasResized())
        {
            m_resizeEvents++;
            m_lastWindowSize = l2d::WindowEvents::resizedSize();
        }

        if (l2d::WindowEvents::mouseWheelScrolled())
        {
            m_mouseWheelEvents++;
            m_lastMouseWheelDelta = l2d::WindowEvents::mouseWheelDelta();
        }
    }

    std::string buildDebugText() const
    {
        std::ostringstream text;

        const l2d::Scene* activeScene =
            m_sceneManager.activeScene();

        l2d::GameObject* player =
            m_playerHandle.get();

        const bool playerHandleValid =
            player != nullptr;

        const bool lastDestroyedCoinHandleValid =
            m_lastDestroyedCoinHandle.isValid();

        l2d::GameObject* selectedObject =
            m_selectedObjectHandle.get();

        const bool selectedObjectHandleValid =
            selectedObject != nullptr;

        const l2d::GameObjectId playerId =
            m_playerHandle.id();

        bool playerFoundById = false;

        if (activeScene != nullptr && playerId != l2d::InvalidGameObjectId)
        {
            const l2d::GameObject* foundPlayer =
                activeScene->findGameObjectById(playerId);

            playerFoundById =
                foundPlayer != nullptr && foundPlayer == player;
        }

        text << "Lorenzo2D Debug\n";
        text << "----------------\n";

        if (activeScene != nullptr)
        {
            text << "Scene: " << activeScene->name() << "\n";
            text << "Objects: "
                << activeScene->activeGameObjectCount()
                << " / "
                << activeScene->gameObjectCount()
                << "\n";

            text << "Destroy queued: "
                << activeScene->destroyQueuedGameObjectCount()
                << "\n";
        }
        else
        {
            text << "Scene: NONE\n";
            text << "Objects: 0 / 0\n";
            text << "Destroy queued: 0\n";
        }

        text << "FPS: " << l2d::Time::fps() << "\n";
        text << "DeltaTime: " << l2d::Time::deltaTime() << "\n";
        text << "Frame: " << l2d::Time::frameCount() << "\n";

        if (player != nullptr)
        {
            const sf::Vector2f playerPosition =
                player->transform.position();

            text << "Player position: "
                << playerPosition.x
                << ", "
                << playerPosition.y
                << "\n";

            text << "Player ID: "
                << playerId
                << "\n";

            text << "Player found by ID: "
                << (playerFoundById ? "YES" : "NO")
                << "\n";

            text << "Player handle valid: "
                << (playerHandleValid ? "YES" : "NO")
                << "\n";
        }
        else
        {
            text << "Player: NULL\n";

            text << "Player ID: "
                << playerId
                << "\n";

            text << "Player found by ID: "
                << (playerFoundById ? "YES" : "NO")
                << "\n";

            text << "Player handle valid: "
                << (playerHandleValid ? "YES" : "NO")
                << "\n";
        }

        text << "Last destroyed coin ID: "
            << m_lastDestroyedCoinId
            << "\n";

        text << "Last destroyed coin handle valid: "
            << (lastDestroyedCoinHandleValid ? "YES" : "NO")
            << "\n";

        if (selectedObject != nullptr)
        {
            text << "Selected object: "
                << selectedObject->name()
                << "\n";

            text << "Selected ID: "
                << selectedObject->id()
                << "\n";

            text << "Selected tag: "
                << selectedObject->tag()
                << "\n";

            text << "Selected handle valid: "
                << (selectedObjectHandleValid ? "YES" : "NO")
                << "\n";
        }
        else
        {
            text << "Selected object: NONE\n";

            text << "Selected ID: "
                << m_selectedObjectHandle.id()
                << "\n";

            text << "Selected tag: NONE\n";

            text << "Selected handle valid: "
                << (selectedObjectHandleValid ? "YES" : "NO")
                << "\n";
        }

        text << "Camera center: "
            << m_camera.center().x
            << ", "
            << m_camera.center().y
            << "\n";

        text << "Camera size: "
            << m_camera.size().x
            << ", "
            << m_camera.size().y
            << "\n";

        text << "Camera zoom: "
            << m_camera.zoom()
            << "\n";

        text << "Camera Controller: "
            << (m_cameraController.hasFollowTarget() ? "FOLLOWING" : "NO TARGET")
            << "\n";

        text << "Mouse screen: "
            << m_mouseScreenPosition.x
            << ", "
            << m_mouseScreenPosition.y
            << "\n";

        text << "Mouse world: "
            << m_mouseWorldPosition.x
            << ", "
            << m_mouseWorldPosition.y
            << "\n";

        text << "Left clicks: "
            << m_leftMouseClicks
            << "\n";

        text << "Resize events: "
            << m_resizeEvents
            << "\n";

        text << "Last window size: "
            << m_lastWindowSize.x
            << ", "
            << m_lastWindowSize.y
            << "\n";

        text << "Mouse wheel events: "
            << m_mouseWheelEvents
            << "\n";

        text << "Last wheel delta: "
            << m_lastMouseWheelDelta
            << "\n";

        text << "Smooth: "
            << m_camera.followSmoothness()
            << "\n";

        text << "Bounds: "
            << (m_camera.hasBounds() ? "ON" : "OFF")
            << "\n";

        text << "Coins: "
            << m_collectedCoins
            << " / "
            << m_totalCoins
            << "\n";

        text << "Deaths: "
            << m_deaths
            << "\n";

        text << "Respawn cooldown: "
            << m_respawnCooldown
            << "\n";

        text << "Render World: "
            << (m_renderLayers.isLayerEnabled(l2d::RenderLayer2D::World) ? "ON" : "OFF")
            << "\n";

        text << "Render PhysicsDebug: "
            << (m_renderLayers.isLayerEnabled(l2d::RenderLayer2D::PhysicsDebug) ? "ON" : "OFF")
            << "\n";

        text << "Render UI: "
            << (m_renderLayers.isLayerEnabled(l2d::RenderLayer2D::UI) ? "ON" : "OFF")
            << "\n";

        if (activeScene != nullptr)
        {
            const std::size_t taggedPlayers =
                activeScene->countGameObjectsByTag(GameTags::Player);

            const std::size_t activePlayers =
                activeScene->countActiveGameObjectsByTag(GameTags::Player);

            const std::size_t taggedCoins =
                activeScene->countGameObjectsByTag(GameTags::Coin);

            const std::size_t activeCoins =
                activeScene->countActiveGameObjectsByTag(GameTags::Coin);

            const std::size_t taggedEnemies =
                activeScene->countGameObjectsByTag(GameTags::Enemy);

            const std::size_t activeEnemies =
                activeScene->countActiveGameObjectsByTag(GameTags::Enemy);

            text << "Tagged Players: "
                << taggedPlayers
                << "\n";

            text << "Active Players: "
                << activePlayers
                << "\n";

            text << "Tagged Coins: "
                << taggedCoins
                << "\n";

            text << "Active Coins: "
                << activeCoins
                << "\n";

            text << "Tagged Enemies: "
                << taggedEnemies
                << "\n";

            text << "Active Enemies: "
                << activeEnemies
                << "\n";
        }

        text << "Level: "
            << (m_levelLoadedFromFile ? "FILE" : "FALLBACK")
            << "\n";

        text << "Source: "
            << m_levelSource
            << "\n";

        text << "Working dir: "
            << m_workingDirectory
            << "\n";

        return text.str();
    }

    void updateDebug()
    {
        const std::string debugText = buildDebugText();

        m_debugOverlay.setText(debugText);

        std::string title = "Lorenzo2D Engine";

        const l2d::Scene* activeScene = m_sceneManager.activeScene();

        if (activeScene != nullptr)
        {
            title += " | Scene: " + activeScene->name();
        }

        title += " | FPS: " + std::to_string(static_cast<int>(l2d::Time::fps()));

        getWindow().setTitle(title);
    }

private:
    l2d::AssetManager m_assets;
    l2d::ActionMap m_actions;

    l2d::SceneManager m_sceneManager;
    l2d::Scene* m_levelScene = nullptr;

    l2d::GameObjectHandle m_playerHandle;
    l2d::GameObjectHandle m_lastDestroyedCoinHandle;
    l2d::GameObjectId m_lastDestroyedCoinId = l2d::InvalidGameObjectId;
    l2d::GameObjectHandle m_selectedObjectHandle;

    int m_totalCoins = 0;
    int m_collectedCoins = 0;

    sf::Vector2f m_playerSpawnPosition = { 150.f, 100.f };

    int m_deaths = 0;
    float m_respawnCooldown = 0.f;

    bool m_levelLoadedFromFile = false;
    std::string m_levelSource = "none";
    std::string m_workingDirectory = "unknown";

    sf::Vector2i m_mouseScreenPosition = { 0, 0 };
    sf::Vector2f m_mouseWorldPosition = { 0.f, 0.f };

    int m_leftMouseClicks = 0;

    sf::Vector2u m_lastWindowSize = { 0, 0 };

    int m_resizeEvents = 0;
    int m_mouseWheelEvents = 0;
    float m_lastMouseWheelDelta = 0.f;

    l2d::PhysicsWorld2D m_physicsWorld;
    l2d::PhysicsDebugRenderer2D m_physicsDebugRenderer;

    l2d::RenderLayerStack2D m_renderLayers;

    l2d::Camera2D m_camera;
    l2d::OrthographicCameraController2D m_cameraController;

    l2d::TileMap m_tileMap;
    l2d::DebugOverlay m_debugOverlay;
};

int main()
{
    SandboxApp app;
    app.run();

    return 0;
}
