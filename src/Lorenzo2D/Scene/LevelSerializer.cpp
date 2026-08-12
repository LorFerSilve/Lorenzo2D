#include <Lorenzo2D/Scene/LevelSerializer.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>
#include <Lorenzo2D/Physics/ConvexPolygonCollider2D.hpp>
#include <Lorenzo2D/Scene/Scene.hpp>

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
        if (!isValidLevel(level)) return false;

        output << std::setprecision(std::numeric_limits<float>::max_digits10);
        output << "LORENZO2D_LEVEL " << CurrentVersion << '\n';
        output << "level " << std::quoted(level.name) << '\n';
        output << "objects " << level.objects.size() << '\n';

        for (const Prefab& prefab : level.objects)
        {
            writeObject(output, prefab);
        }

        return static_cast<bool>(output);
    }

    bool LevelSerializer::load(std::istream& input, LevelDocument& level)
    {
        LevelDocument parsed;
        std::uint32_t version = 0;
        std::size_t objectCount = 0;

        if (!parseLine(input, "LORENZO2D_LEVEL",
                       [&](std::istream& line)
                       {
                           return static_cast<bool>(line >> version) &&
                                  version >= MinimumSupportedVersion && version <= CurrentVersion;
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
}
