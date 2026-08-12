#include <Lorenzo2D/Scene/ComponentCodecRegistry.hpp>

#include <Lorenzo2D/ECS/GameObject.hpp>

#include <utility>

namespace l2d
{
    bool ComponentCodecRegistry::registerCodec(std::string type, std::uint32_t version,
                                               Encoder encoder, Decoder decoder)
    {
        if (type.empty() || version == 0u || !encoder || !decoder) return false;

        return m_codecs.emplace(key(type, version), Codec{std::move(encoder), std::move(decoder)})
            .second;
    }

    bool ComponentCodecRegistry::removeCodec(const std::string& type, std::uint32_t version)
    {
        return m_codecs.erase(key(type, version)) != 0u;
    }

    void ComponentCodecRegistry::clear()
    {
        m_codecs.clear();
    }

    bool ComponentCodecRegistry::contains(const std::string& type, std::uint32_t version) const
    {
        return m_codecs.find(key(type, version)) != m_codecs.end();
    }

    std::size_t ComponentCodecRegistry::size() const
    {
        return m_codecs.size();
    }

    bool ComponentCodecRegistry::decode(GameObject& object,
                                        const SerializedComponentPrefab& component) const
    {
        const auto iterator = m_codecs.find(key(component.type, component.version));
        if (iterator == m_codecs.end()) return !component.required;
        return iterator->second.decoder(object, component.data);
    }

    std::optional<SerializedComponentPrefab> ComponentCodecRegistry::encode(
        const GameObject& object, const std::string& type, std::uint32_t version,
        bool required) const
    {
        const auto iterator = m_codecs.find(key(type, version));
        if (iterator == m_codecs.end()) return std::nullopt;

        std::optional<std::string> data = iterator->second.encoder(object);
        if (!data) return std::nullopt;

        return SerializedComponentPrefab{type, version, required, std::move(*data)};
    }

    std::string ComponentCodecRegistry::key(const std::string& type, std::uint32_t version)
    {
        std::string result = type;
        result.push_back('\0');
        result += std::to_string(version);
        return result;
    }
}
