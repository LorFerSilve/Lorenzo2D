#pragma once

#include <Lorenzo2D/Scene/Prefab.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace l2d
{
    class GameObject;

    class ComponentCodecRegistry
    {
      public:
        using Decoder = std::function<bool(GameObject&, const std::string&)>;
        using Encoder = std::function<std::optional<std::string>(const GameObject&)>;

        bool registerCodec(std::string type, std::uint32_t version, Encoder encoder,
                           Decoder decoder);
        bool removeCodec(const std::string& type, std::uint32_t version);
        void clear();
        bool contains(const std::string& type, std::uint32_t version) const;
        std::size_t size() const;

        // Unknown optional components are skipped. Unknown required codecs and
        // decoder failures reject the object. A decoder receives a mutable
        // object; direct decode() cannot generically undo arbitrary decoder
        // side effects on failure. LevelSerializer::instantiate() removes the
        // newly-created level batch when decoding rejects it.
        bool decode(GameObject& object, const SerializedComponentPrefab& component) const;
        std::optional<SerializedComponentPrefab> encode(const GameObject& object,
                                                        const std::string& type,
                                                        std::uint32_t version,
                                                        bool required = true) const;

      private:
        static std::string key(const std::string& type, std::uint32_t version);
        struct Codec
        {
            Encoder encoder;
            Decoder decoder;
        };

        std::unordered_map<std::string, Codec> m_codecs;
    };
}
