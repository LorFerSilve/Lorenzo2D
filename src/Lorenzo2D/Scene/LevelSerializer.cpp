#include <Lorenzo2D/Scene/LevelSerializer.hpp>

#include <Lorenzo2D/Assets/AssetManager.hpp>
#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/ConvexPolygonCollider2D.hpp>
#include <Lorenzo2D/Scene/ComponentCodecRegistry.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace l2d
{
    namespace
    {
        using Json = nlohmann::ordered_json;

        Json vectorJson(sf::Vector2f value)
        {
            return Json::array({value.x, value.y});
        }

        bool readVector(const Json& value, sf::Vector2f& output)
        {
            if (!value.is_array() || value.size() != 2u || !value[0].is_number() ||
                !value[1].is_number())
                return false;
            output = {value[0].get<float>(), value[1].get<float>()};
            return std::isfinite(output.x) && std::isfinite(output.y);
        }

        Json colorJson(sf::Color color)
        {
            return Json::array({color.r, color.g, color.b, color.a});
        }

        bool readJsonColor(const Json& value, sf::Color& output)
        {
            if (!value.is_array() || value.size() != 4u) return false;
            unsigned int channels[4] = {};
            for (std::size_t index = 0; index < 4u; ++index)
            {
                if (!value[index].is_number_unsigned()) return false;
                channels[index] = value[index].get<unsigned int>();
                if (channels[index] > 255u) return false;
            }
            output = {
                static_cast<std::uint8_t>(channels[0]), static_cast<std::uint8_t>(channels[1]),
                static_cast<std::uint8_t>(channels[2]), static_cast<std::uint8_t>(channels[3])};
            return true;
        }

        Json prefabJson(const Prefab& prefab)
        {
            Json object;
            object["name"] = prefab.name;
            object["tag"] = prefab.tag;
            object["active"] = prefab.active;
            object["zOrder"] = prefab.zOrder;
            object["transform"] = {{"position", vectorJson(prefab.transform.position)},
                                   {"rotation", prefab.transform.rotation},
                                   {"scale", vectorJson(prefab.transform.scale)}};

            Json components = Json::array();
            const auto push =
                [&](const char* type, std::uint32_t version, Json data, bool required = true)
            {
                components.push_back({{"type", type},
                                      {"version", version},
                                      {"required", required},
                                      {"data", std::move(data)}});
            };

            if (prefab.rectangleRenderer)
                push("RectangleRenderer", 1u,
                     {{"size", vectorJson(prefab.rectangleRenderer->size)},
                      {"color", colorJson(prefab.rectangleRenderer->color)}});
            if (prefab.circleRenderer)
                push("CircleRenderer", 1u,
                     {{"radius", prefab.circleRenderer->radius},
                      {"color", colorJson(prefab.circleRenderer->color)}});
            if (prefab.spriteRenderer)
            {
                const SpriteRendererPrefab& sprite = *prefab.spriteRenderer;
                push("SpriteRenderer", 1u,
                     {{"texture", sprite.texture},
                      {"rect",
                       {sprite.textureRect.position.x, sprite.textureRect.position.y,
                        sprite.textureRect.size.x, sprite.textureRect.size.y}},
                      {"size", vectorJson(sprite.size)},
                      {"color", colorJson(sprite.color)},
                      {"origin", vectorJson(sprite.origin)},
                      {"flipX", sprite.flipX},
                      {"flipY", sprite.flipY},
                      {"renderOrder",
                       {{"layer", sprite.renderOrder.layer},
                        {"depth", sprite.renderOrder.depth},
                        {"order", sprite.renderOrder.order},
                        {"mode", sprite.renderOrder.depthMode}}}});
            }
            if (prefab.animator)
                push("Animator", 1u,
                     {{"clips", prefab.animator->clips},
                      {"initialClip", prefab.animator->initialClip},
                      {"speed", prefab.animator->playbackSpeed},
                      {"playing", prefab.animator->playing}});
            if (prefab.rigidBody)
                push("RigidBody2D", 1u,
                     {{"bodyType", static_cast<int>(prefab.rigidBody->bodyType)},
                      {"velocity", vectorJson(prefab.rigidBody->velocity)},
                      {"acceleration", vectorJson(prefab.rigidBody->acceleration)},
                      {"mass", prefab.rigidBody->mass},
                      {"useGravity", prefab.rigidBody->useGravity},
                      {"gravityScale", prefab.rigidBody->gravityScale}});
            if (prefab.characterMotor)
            {
                const CharacterMotorConfig2D& motor = prefab.characterMotor->config;
                push("CharacterMotor2D", 1u,
                     {{"skinWidth", motor.skinWidth},
                      {"groundProbeDistance", motor.groundProbeDistance},
                      {"maximumSlopeAngleDegrees", motor.maximumSlopeAngleDegrees},
                      {"minimumMoveDistance", motor.minimumMoveDistance},
                      {"maximumMoveDistance", motor.maximumMoveDistance},
                      {"maximumPlatformDisplacement", motor.maximumPlatformDisplacement},
                      {"maximumSlideIterations", motor.maximumSlideIterations},
                      {"maximumRecoveryIterations", motor.maximumRecoveryIterations},
                      {"upDirection", vectorJson(motor.upDirection)},
                      {"categoryMask", motor.queryFilter.categoryMask},
                      {"includeSensors", motor.queryFilter.includeSensors},
                      {"snapToGround", motor.snapToGround},
                      {"inheritPlatformTranslation", motor.inheritPlatformTranslation}});
            }

            const auto colliderProperties = [](const ColliderPrefabProperties& properties)
            {
                return Json{{"offset", vectorJson(properties.offset)},
                            {"restitution", properties.material.restitution},
                            {"staticFriction", properties.material.staticFriction},
                            {"dynamicFriction", properties.material.dynamicFriction},
                            {"category", properties.filter.categoryBits},
                            {"mask", properties.filter.maskBits},
                            {"sensor", properties.sensor}};
            };
            if (prefab.boxCollider)
                push("BoxCollider2D", 1u,
                     {{"size", vectorJson(prefab.boxCollider->size)},
                      {"properties", colliderProperties(prefab.boxCollider->properties)}});
            if (prefab.circleCollider)
                push("CircleCollider2D", 1u,
                     {{"radius", prefab.circleCollider->radius},
                      {"properties", colliderProperties(prefab.circleCollider->properties)}});
            if (prefab.capsuleCollider)
                push("CapsuleCollider2D", 1u,
                     {{"radius", prefab.capsuleCollider->radius},
                      {"height", prefab.capsuleCollider->height},
                      {"properties", colliderProperties(prefab.capsuleCollider->properties)}});
            if (prefab.convexPolygonCollider)
            {
                Json vertices = Json::array();
                for (sf::Vector2f vertex : prefab.convexPolygonCollider->vertices)
                    vertices.push_back(vectorJson(vertex));
                push(
                    "ConvexPolygonCollider2D", 1u,
                    {{"vertices", std::move(vertices)},
                     {"properties", colliderProperties(prefab.convexPolygonCollider->properties)}});
            }
            for (const SerializedComponentPrefab& component : prefab.customComponents)
            {
                Json data = Json::parse(component.data, nullptr, false);
                if (data.is_discarded()) data = component.data;
                push(component.type.c_str(), component.version, std::move(data),
                     component.required);
            }

            object["components"] = std::move(components);
            return object;
        }

        bool colliderPropertiesFromJson(const Json& value, ColliderPrefabProperties& output)
        {
            if (!value.is_object() || !value.contains("offset") ||
                !readVector(value.at("offset"), output.offset))
                return false;
            output.material.restitution = value.value("restitution", 0.f);
            output.material.staticFriction = value.value("staticFriction", 0.5f);
            output.material.dynamicFriction = value.value("dynamicFriction", 0.3f);
            output.filter.categoryBits = value.value("category", std::uint32_t{1u});
            output.filter.maskBits = value.value("mask", std::numeric_limits<std::uint32_t>::max());
            output.sensor = value.value("sensor", false);
            return true;
        }

        bool componentFromJson(const Json& component, Prefab& prefab)
        {
            if (!component.is_object() || !component.contains("type") ||
                !component.at("type").is_string() || !component.contains("version") ||
                !component.at("version").is_number_unsigned() || !component.contains("data"))
                return false;
            const std::string type = component.at("type").get<std::string>();
            const std::uint32_t version = component.at("version").get<std::uint32_t>();
            const bool required = component.value("required", true);
            const Json& data = component.at("data");
            if (version != 1u || !data.is_object())
            {
                prefab.customComponents.push_back({type, version, required, data.dump()});
                return true;
            }

            if (type == "RectangleRenderer")
            {
                if (prefab.rectangleRenderer) return false;
                RectangleRendererPrefab value;
                if (!data.contains("size") || !readVector(data.at("size"), value.size) ||
                    !data.contains("color") || !readJsonColor(data.at("color"), value.color))
                    return false;
                prefab.rectangleRenderer = value;
            }
            else if (type == "CircleRenderer")
            {
                if (prefab.circleRenderer) return false;
                CircleRendererPrefab value;
                value.radius = data.value("radius", -1.f);
                if (!data.contains("color") || !readJsonColor(data.at("color"), value.color))
                    return false;
                prefab.circleRenderer = value;
            }
            else if (type == "SpriteRenderer")
            {
                if (prefab.spriteRenderer) return false;
                SpriteRendererPrefab value;
                value.texture = data.value("texture", std::string{});
                if (!data.contains("rect") || !data.at("rect").is_array() ||
                    data.at("rect").size() != 4u ||
                    !std::all_of(data.at("rect").begin(), data.at("rect").end(),
                                 [](const Json& value) { return value.is_number_integer(); }) ||
                    !data.contains("size") || !readVector(data.at("size"), value.size) ||
                    !data.contains("color") || !readJsonColor(data.at("color"), value.color) ||
                    !data.contains("origin") || !readVector(data.at("origin"), value.origin))
                    return false;
                value.textureRect = {
                    {data.at("rect")[0].get<int>(), data.at("rect")[1].get<int>()},
                    {data.at("rect")[2].get<int>(), data.at("rect")[3].get<int>()}};
                value.flipX = data.value("flipX", false);
                value.flipY = data.value("flipY", false);
                if (data.contains("renderOrder"))
                {
                    const Json& order = data.at("renderOrder");
                    if (!order.is_object()) return false;
                    value.renderOrder.layer = order.value("layer", 0);
                    value.renderOrder.depth = order.value("depth", 0.f);
                    value.renderOrder.order = order.value("order", 0);
                    value.renderOrder.depthMode = order.value("mode", std::uint8_t{0u});
                }
                prefab.spriteRenderer = std::move(value);
            }
            else if (type == "Animator")
            {
                if (prefab.animator) return false;
                if (!data.contains("clips") || !data.at("clips").is_array() ||
                    !std::all_of(data.at("clips").begin(), data.at("clips").end(),
                                 [](const Json& value) { return value.is_string(); }))
                    return false;
                AnimatorPrefab value;
                value.clips = data.value("clips", std::vector<AssetId>{});
                value.initialClip = data.value("initialClip", std::string{});
                value.playbackSpeed = data.value("speed", 1.f);
                value.playing = data.value("playing", true);
                prefab.animator = std::move(value);
            }
            else if (type == "RigidBody2D")
            {
                if (prefab.rigidBody) return false;
                RigidBodyPrefab value;
                const int bodyType = data.value("bodyType", -1);
                if (bodyType < 0 || bodyType > 2 || !data.contains("velocity") ||
                    !readVector(data.at("velocity"), value.velocity) ||
                    !data.contains("acceleration") ||
                    !readVector(data.at("acceleration"), value.acceleration))
                    return false;
                value.bodyType = static_cast<BodyType2D>(bodyType);
                value.mass = data.value("mass", 1.f);
                value.useGravity = data.value("useGravity", false);
                value.gravityScale = data.value("gravityScale", 1.f);
                prefab.rigidBody = value;
            }
            else if (type == "CharacterMotor2D")
            {
                if (prefab.characterMotor) return false;
                CharacterMotorPrefab value;
                CharacterMotorConfig2D& motor = value.config;
                motor.skinWidth = data.value("skinWidth", motor.skinWidth);
                motor.groundProbeDistance =
                    data.value("groundProbeDistance", motor.groundProbeDistance);
                motor.maximumSlopeAngleDegrees =
                    data.value("maximumSlopeAngleDegrees", motor.maximumSlopeAngleDegrees);
                motor.minimumMoveDistance =
                    data.value("minimumMoveDistance", motor.minimumMoveDistance);
                motor.maximumMoveDistance =
                    data.value("maximumMoveDistance", motor.maximumMoveDistance);
                motor.maximumPlatformDisplacement =
                    data.value("maximumPlatformDisplacement", motor.maximumPlatformDisplacement);
                motor.maximumSlideIterations =
                    data.value("maximumSlideIterations", motor.maximumSlideIterations);
                motor.maximumRecoveryIterations =
                    data.value("maximumRecoveryIterations", motor.maximumRecoveryIterations);
                if (!data.contains("upDirection") ||
                    !readVector(data.at("upDirection"), motor.upDirection))
                    return false;
                motor.queryFilter.categoryMask =
                    data.value("categoryMask", motor.queryFilter.categoryMask);
                motor.queryFilter.includeSensors =
                    data.value("includeSensors", motor.queryFilter.includeSensors);
                motor.snapToGround = data.value("snapToGround", motor.snapToGround);
                motor.inheritPlatformTranslation =
                    data.value("inheritPlatformTranslation", motor.inheritPlatformTranslation);
                if (!CharacterMotor2D::isValidConfig(motor)) return false;
                prefab.characterMotor = std::move(value);
            }
            else if (type == "BoxCollider2D")
            {
                if (prefab.boxCollider) return false;
                BoxColliderPrefab value;
                if (!data.contains("size") || !readVector(data.at("size"), value.size) ||
                    !data.contains("properties") ||
                    !colliderPropertiesFromJson(data.at("properties"), value.properties))
                    return false;
                prefab.boxCollider = value;
            }
            else if (type == "CircleCollider2D")
            {
                if (prefab.circleCollider) return false;
                CircleColliderPrefab value;
                value.radius = data.value("radius", -1.f);
                if (!data.contains("properties") ||
                    !colliderPropertiesFromJson(data.at("properties"), value.properties))
                    return false;
                prefab.circleCollider = value;
            }
            else if (type == "CapsuleCollider2D")
            {
                if (prefab.capsuleCollider) return false;
                CapsuleColliderPrefab value;
                value.radius = data.value("radius", -1.f);
                value.height = data.value("height", -1.f);
                if (!data.contains("properties") ||
                    !colliderPropertiesFromJson(data.at("properties"), value.properties))
                    return false;
                prefab.capsuleCollider = value;
            }
            else if (type == "ConvexPolygonCollider2D")
            {
                if (prefab.convexPolygonCollider) return false;
                ConvexPolygonColliderPrefab value;
                if (!data.contains("vertices") || !data.at("vertices").is_array() ||
                    !data.contains("properties") ||
                    !colliderPropertiesFromJson(data.at("properties"), value.properties))
                    return false;
                value.vertices.clear();
                for (const Json& vertex : data.at("vertices"))
                {
                    sf::Vector2f point;
                    if (!readVector(vertex, point)) return false;
                    value.vertices.push_back(point);
                }
                prefab.convexPolygonCollider = std::move(value);
            }
            else
                prefab.customComponents.push_back({type, version, required, data.dump()});

            return true;
        }

        bool prefabFromJson(const Json& object, Prefab& prefab)
        {
            if (!object.is_object() || !object.contains("transform") ||
                !object.contains("components") || !object.at("components").is_array())
                return false;
            prefab.name = object.value("name", std::string{"GameObject"});
            prefab.tag = object.value("tag", std::string{});
            prefab.active = object.value("active", true);
            prefab.zOrder = object.value("zOrder", 0);
            const Json& transform = object.at("transform");
            if (!transform.is_object() || !transform.contains("position") ||
                !readVector(transform.at("position"), prefab.transform.position) ||
                !transform.contains("scale") ||
                !readVector(transform.at("scale"), prefab.transform.scale))
                return false;
            prefab.transform.rotation = transform.value("rotation", 0.f);
            for (const Json& component : object.at("components"))
                if (!componentFromJson(component, prefab)) return false;
            return isValidPrefab(prefab);
        }

        template <typename Parser>
        bool parseLine(std::istream& input, const char* keyword, Parser&& parser)
        {
            std::string line;

            if (!std::getline(input, line)) return false;

            std::istringstream stream(line);
            std::string actualKeyword;

            if (!(stream >> actualKeyword) || actualKeyword != keyword || !parser(stream))
            {
                return false;
            }

            stream >> std::ws;
            return stream.eof();
        }

        bool parseBoolean(std::istream& input, bool& value)
        {
            int parsed = 0;

            if (!(input >> parsed) || (parsed != 0 && parsed != 1)) return false;

            value = parsed == 1;
            return true;
        }

        bool parseColor(std::istream& input, sf::Color& color)
        {
            unsigned int red = 0;
            unsigned int green = 0;
            unsigned int blue = 0;
            unsigned int alpha = 0;

            if (!(input >> red >> green >> blue >> alpha) || red > 255u || green > 255u ||
                blue > 255u || alpha > 255u)
            {
                return false;
            }

            color = {static_cast<std::uint8_t>(red), static_cast<std::uint8_t>(green),
                     static_cast<std::uint8_t>(blue), static_cast<std::uint8_t>(alpha)};
            return true;
        }

        void writeColor(std::ostream& output, sf::Color color)
        {
            output << static_cast<unsigned int>(color.r) << ' '
                   << static_cast<unsigned int>(color.g) << ' '
                   << static_cast<unsigned int>(color.b) << ' '
                   << static_cast<unsigned int>(color.a);
        }

        bool parseColliderProperties(std::istream& input, ColliderPrefabProperties& properties)
        {
            return static_cast<bool>(
                       input >> properties.offset.x >> properties.offset.y >>
                       properties.material.restitution >> properties.material.staticFriction >>
                       properties.material.dynamicFriction >> properties.filter.categoryBits >>
                       properties.filter.maskBits) &&
                   parseBoolean(input, properties.sensor);
        }

        void writeColliderProperties(std::ostream& output,
                                     const ColliderPrefabProperties& properties)
        {
            output << properties.offset.x << ' ' << properties.offset.y << ' '
                   << properties.material.restitution << ' ' << properties.material.staticFriction
                   << ' ' << properties.material.dynamicFriction << ' '
                   << properties.filter.categoryBits << ' ' << properties.filter.maskBits << ' '
                   << (properties.sensor ? 1 : 0);
        }

        bool readObject(std::istream& input, Prefab& prefab, std::uint32_t version)
        {
            if (!parseLine(input, "object", [](std::istream&) { return true; }) ||
                !parseLine(input, "name", [&](std::istream& line)
                           { return static_cast<bool>(line >> std::quoted(prefab.name)); }) ||
                !parseLine(input, "tag", [&](std::istream& line)
                           { return static_cast<bool>(line >> std::quoted(prefab.tag)); }) ||
                !parseLine(input, "active",
                           [&](std::istream& line) { return parseBoolean(line, prefab.active); }))
            {
                return false;
            }

            if (version >= 2u && !parseLine(input, "z_order", [&](std::istream& line)
                                            { return static_cast<bool>(line >> prefab.zOrder); }))
            {
                return false;
            }

            if (!parseLine(input, "transform",
                           [&](std::istream& line)
                           {
                               return static_cast<bool>(
                                   line >> prefab.transform.position.x >>
                                   prefab.transform.position.y >> prefab.transform.rotation >>
                                   prefab.transform.scale.x >> prefab.transform.scale.y);
                           }))
            {
                return false;
            }

            bool present = false;

            if (!parseLine(input, "rectangle",
                           [&](std::istream& line)
                           {
                               if (!parseBoolean(line, present)) return false;
                               if (!present) return true;

                               RectangleRendererPrefab renderer;
                               if (!(line >> renderer.size.x >> renderer.size.y) ||
                                   !parseColor(line, renderer.color))
                               {
                                   return false;
                               }
                               prefab.rectangleRenderer = renderer;
                               return true;
                           }) ||
                !parseLine(input, "circle",
                           [&](std::istream& line)
                           {
                               if (!parseBoolean(line, present)) return false;
                               if (!present) return true;

                               CircleRendererPrefab renderer;
                               if (!(line >> renderer.radius) || !parseColor(line, renderer.color))
                               {
                                   return false;
                               }
                               prefab.circleRenderer = renderer;
                               return true;
                           }) ||
                !parseLine(input, "rigid_body",
                           [&](std::istream& line)
                           {
                               if (!parseBoolean(line, present)) return false;
                               if (!present) return true;

                               int bodyType = 0;
                               RigidBodyPrefab body;
                               if (!(line >> bodyType) || bodyType < 0 || bodyType > 2 ||
                                   !(line >> body.velocity.x >> body.velocity.y >>
                                     body.acceleration.x >> body.acceleration.y >> body.mass) ||
                                   !parseBoolean(line, body.useGravity) ||
                                   !(line >> body.gravityScale))
                               {
                                   return false;
                               }
                               body.bodyType = static_cast<BodyType2D>(bodyType);
                               prefab.rigidBody = body;
                               return true;
                           }) ||
                !parseLine(input, "box_collider",
                           [&](std::istream& line)
                           {
                               if (!parseBoolean(line, present)) return false;
                               if (!present) return true;

                               BoxColliderPrefab collider;
                               if (!(line >> collider.size.x >> collider.size.y) ||
                                   !parseColliderProperties(line, collider.properties))
                               {
                                   return false;
                               }
                               prefab.boxCollider = collider;
                               return true;
                           }) ||
                !parseLine(input, "circle_collider",
                           [&](std::istream& line)
                           {
                               if (!parseBoolean(line, present)) return false;
                               if (!present) return true;

                               CircleColliderPrefab collider;
                               if (!(line >> collider.radius) ||
                                   !parseColliderProperties(line, collider.properties))
                               {
                                   return false;
                               }
                               prefab.circleCollider = collider;
                               return true;
                           }))
            {
                return false;
            }

            if (version >= 3u)
            {
                if (!parseLine(input, "capsule_collider",
                               [&](std::istream& line)
                               {
                                   if (!parseBoolean(line, present)) return false;
                                   if (!present) return true;

                                   CapsuleColliderPrefab collider;
                                   if (!(line >> collider.radius >> collider.height) ||
                                       !parseColliderProperties(line, collider.properties))
                                   {
                                       return false;
                                   }
                                   prefab.capsuleCollider = collider;
                                   return true;
                               }) ||
                    !parseLine(input, "convex_polygon_collider",
                               [&](std::istream& line)
                               {
                                   if (!parseBoolean(line, present)) return false;
                                   if (!present) return true;

                                   std::size_t count = 0u;
                                   ConvexPolygonColliderPrefab collider;
                                   if (!(line >> count) || count < 3u ||
                                       count > ConvexPolygonCollider2D::MaximumVertexCount)
                                   {
                                       return false;
                                   }
                                   collider.vertices.resize(count);
                                   for (sf::Vector2f& vertex : collider.vertices)
                                   {
                                       if (!(line >> vertex.x >> vertex.y)) return false;
                                   }
                                   if (!parseColliderProperties(line, collider.properties))
                                       return false;
                                   prefab.convexPolygonCollider = std::move(collider);
                                   return true;
                               }))
                {
                    return false;
                }
            }

            if (!parseLine(input, "end", [](std::istream&) { return true; })) return false;

            return isValidPrefab(prefab);
        }

        void writeObject(std::ostream& output, const Prefab& prefab)
        {
            output << "object\n";
            output << "name " << std::quoted(prefab.name) << '\n';
            output << "tag " << std::quoted(prefab.tag) << '\n';
            output << "active " << (prefab.active ? 1 : 0) << '\n';
            output << "z_order " << prefab.zOrder << '\n';
            output << "transform " << prefab.transform.position.x << ' '
                   << prefab.transform.position.y << ' ' << prefab.transform.rotation << ' '
                   << prefab.transform.scale.x << ' ' << prefab.transform.scale.y << '\n';

            output << "rectangle " << (prefab.rectangleRenderer ? 1 : 0);
            if (prefab.rectangleRenderer)
            {
                output << ' ' << prefab.rectangleRenderer->size.x << ' '
                       << prefab.rectangleRenderer->size.y << ' ';
                writeColor(output, prefab.rectangleRenderer->color);
            }
            output << '\n';

            output << "circle " << (prefab.circleRenderer ? 1 : 0);
            if (prefab.circleRenderer)
            {
                output << ' ' << prefab.circleRenderer->radius << ' ';
                writeColor(output, prefab.circleRenderer->color);
            }
            output << '\n';

            output << "rigid_body " << (prefab.rigidBody ? 1 : 0);
            if (prefab.rigidBody)
            {
                output << ' ' << static_cast<int>(prefab.rigidBody->bodyType) << ' '
                       << prefab.rigidBody->velocity.x << ' ' << prefab.rigidBody->velocity.y << ' '
                       << prefab.rigidBody->acceleration.x << ' '
                       << prefab.rigidBody->acceleration.y << ' ' << prefab.rigidBody->mass << ' '
                       << (prefab.rigidBody->useGravity ? 1 : 0) << ' '
                       << prefab.rigidBody->gravityScale;
            }
            output << '\n';

            output << "box_collider " << (prefab.boxCollider ? 1 : 0);
            if (prefab.boxCollider)
            {
                output << ' ' << prefab.boxCollider->size.x << ' ' << prefab.boxCollider->size.y
                       << ' ';
                writeColliderProperties(output, prefab.boxCollider->properties);
            }
            output << '\n';

            output << "circle_collider " << (prefab.circleCollider ? 1 : 0);
            if (prefab.circleCollider)
            {
                output << ' ' << prefab.circleCollider->radius << ' ';
                writeColliderProperties(output, prefab.circleCollider->properties);
            }
            output << '\n';

            output << "capsule_collider " << (prefab.capsuleCollider ? 1 : 0);
            if (prefab.capsuleCollider)
            {
                output << ' ' << prefab.capsuleCollider->radius << ' '
                       << prefab.capsuleCollider->height << ' ';
                writeColliderProperties(output, prefab.capsuleCollider->properties);
            }
            output << '\n';

            output << "convex_polygon_collider " << (prefab.convexPolygonCollider ? 1 : 0);
            if (prefab.convexPolygonCollider)
            {
                output << ' ' << prefab.convexPolygonCollider->vertices.size();
                for (const sf::Vector2f vertex : prefab.convexPolygonCollider->vertices)
                    output << ' ' << vertex.x << ' ' << vertex.y;
                output << ' ';
                writeColliderProperties(output, prefab.convexPolygonCollider->properties);
            }
            output << "\nend\n";
        }

        bool isValidLevel(const LevelDocument& level)
        {
            if (level.name.find('\n') != std::string::npos ||
                level.name.find('\r') != std::string::npos ||
                level.objects.size() > LevelSerializer::MaximumObjectCount)
            {
                return false;
            }

            for (const Prefab& prefab : level.objects)
            {
                if (!isValidPrefab(prefab)) return false;
            }

            return true;
        }
    }

    bool LevelSerializer::save(std::ostream& output, const LevelDocument& level)
    {
        return saveJson(output, level);
    }

    bool LevelSerializer::load(std::istream& input, LevelDocument& level)
    {
        input >> std::ws;
        if (input.peek() == '{') return loadJson(input, level);

        LevelDocument parsed;
        std::uint32_t version = 0;
        std::size_t objectCount = 0;

        if (!parseLine(input, "LORENZO2D_LEVEL",
                       [&](std::istream& line)
                       {
                           return static_cast<bool>(line >> version) &&
                                  version >= MinimumSupportedVersion && version <= 3u;
                       }) ||
            !parseLine(input, "level", [&](std::istream& line)
                       { return static_cast<bool>(line >> std::quoted(parsed.name)); }) ||
            !parseLine(input, "objects",
                       [&](std::istream& line) { return static_cast<bool>(line >> objectCount); }))
        {
            return false;
        }

        if (objectCount > MaximumObjectCount) return false;

        parsed.objects.reserve(objectCount);

        for (std::size_t index = 0; index < objectCount; ++index)
        {
            Prefab prefab;

            if (!readObject(input, prefab, version)) return false;

            parsed.objects.push_back(std::move(prefab));
        }

        input >> std::ws;
        if (!input.eof()) return false;

        level = std::move(parsed);
        return true;
    }

    bool LevelSerializer::saveJson(std::ostream& output, const LevelDocument& level)
    {
        if (!isValidLevel(level)) return false;

        Json root;
        root["format"] = "Lorenzo2DLevel";
        root["version"] = CurrentVersion;
        root["name"] = level.name;
        root["objects"] = Json::array();
        for (const Prefab& prefab : level.objects)
            root["objects"].push_back(prefabJson(prefab));

        output << root.dump(2) << '\n';
        return static_cast<bool>(output);
    }

    bool LevelSerializer::loadJson(std::istream& input, LevelDocument& level)
    {
        try
        {
            Json root = Json::parse(input, nullptr, false);
            if (root.is_discarded() || !root.is_object() ||
                root.value("format", std::string{}) != "Lorenzo2DLevel" ||
                root.value("version", std::uint32_t{0u}) < 4u ||
                root.value("version", std::uint32_t{0u}) > CurrentVersion ||
                !root.contains("objects") || !root.at("objects").is_array() ||
                root.at("objects").size() > MaximumObjectCount)
                return false;

            LevelDocument parsed;
            parsed.name = root.value("name", std::string{"Level"});
            parsed.objects.reserve(root.at("objects").size());
            for (const Json& object : root.at("objects"))
            {
                Prefab prefab;
                if (!prefabFromJson(object, prefab)) return false;
                parsed.objects.push_back(std::move(prefab));
            }
            if (!isValidLevel(parsed)) return false;
            level = std::move(parsed);
            return true;
        }
        catch (const Json::exception&)
        {
            return false;
        }
    }

    bool LevelSerializer::saveToFile(const std::string& filepath, const LevelDocument& level)
    {
        if (!isValidLevel(level)) return false;

        std::ofstream output(filepath, std::ios::trunc);
        return output.is_open() && save(output, level);
    }

    bool LevelSerializer::loadFromFile(const std::string& filepath, LevelDocument& level)
    {
        std::ifstream input(filepath);
        return input.is_open() && load(input, level);
    }

    std::vector<GameObjectHandle> LevelSerializer::instantiate(Scene& scene,
                                                               const LevelDocument& level)
    {
        if (!isValidLevel(level))
        {
            throw std::invalid_argument("Cannot instantiate an invalid level document.");
        }

        std::vector<GameObjectHandle> handles;
        handles.reserve(level.objects.size());

        for (const Prefab& prefab : level.objects)
        {
            GameObject& object = instantiatePrefab(scene, prefab);
            handles.push_back(scene.createHandle(object));
        }

        return handles;
    }

    std::vector<GameObjectHandle> LevelSerializer::instantiate(Scene& scene,
                                                               const LevelDocument& level,
                                                               AssetManager& assets,
                                                               const ComponentCodecRegistry* codecs)
    {
        if (!isValidLevel(level))
            throw std::invalid_argument("Cannot instantiate an invalid level document.");

        std::vector<GameObjectHandle> handles;
        handles.reserve(level.objects.size());

        try
        {
            for (const Prefab& prefab : level.objects)
            {
                GameObject& object = instantiatePrefab(scene, prefab, assets);
                handles.push_back(scene.createHandle(object));
                if (codecs != nullptr)
                {
                    for (const SerializedComponentPrefab& component : prefab.customComponents)
                    {
                        if (!codecs->decode(object, component))
                            throw std::invalid_argument("A required component codec failed.");
                    }
                }
                else
                {
                    for (const SerializedComponentPrefab& component : prefab.customComponents)
                        if (component.required)
                            throw std::invalid_argument("A required component codec is missing.");
                }
            }
        }
        catch (...)
        {
            for (GameObjectHandle handle : handles)
                if (GameObject* object = handle.get()) object->destroy();
            scene.destroyQueuedGameObjects();
            throw;
        }

        return handles;
    }
}
