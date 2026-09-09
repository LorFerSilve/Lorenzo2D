#include <Lorenzo2D/Renderer/Shader2D.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

namespace l2d
{
    namespace
    {
        bool uniformSpecLess(const ShaderUniformSpec2D& left, const ShaderUniformSpec2D& right)
        {
            return left.name < right.name;
        }
    }

    bool Shader2D::isSupported() noexcept
    {
        return sf::Shader::isAvailable();
    }

    bool Shader2D::loadFragmentSource(std::string_view fragmentSource)
    {
        if (!isValidSource(fragmentSource) || !isSupported()) return false;

        sf::Shader candidate;
        if (!candidate.loadFromMemory(fragmentSource, sf::Shader::Type::Fragment)) return false;

        m_shader = std::move(candidate);
        m_loaded = true;
        return true;
    }

    bool Shader2D::loadVertexFragmentSource(std::string_view vertexSource,
                                            std::string_view fragmentSource)
    {
        if (!isValidSource(vertexSource) || !isValidSource(fragmentSource) || !isSupported())
            return false;

        sf::Shader candidate;
        if (!candidate.loadFromMemory(vertexSource, fragmentSource)) return false;

        m_shader = std::move(candidate);
        m_loaded = true;
        return true;
    }

    bool Shader2D::setUniformLayout(std::vector<ShaderUniformSpec2D> layout)
    {
        if (layout.size() > MaximumUniformCount) return false;

        for (const ShaderUniformSpec2D& spec : layout)
        {
            if (!isValidUniformName(spec.name) || !isValidUniformType(spec.type)) return false;
        }

        std::sort(layout.begin(), layout.end(), uniformSpecLess);

        for (std::size_t index = 1u; index < layout.size(); ++index)
            if (layout[index - 1u].name == layout[index].name) return false;

        m_uniformLayout = std::move(layout);
        return true;
    }

    void Shader2D::clearUniformLayout()
    {
        m_uniformLayout.clear();
    }

    bool Shader2D::loaded() const noexcept
    {
        return m_loaded;
    }

    unsigned int Shader2D::nativeHandle() const noexcept
    {
        return m_loaded ? m_shader.getNativeHandle() : 0u;
    }

    std::size_t Shader2D::uniformCount() const noexcept
    {
        return m_uniformLayout.size();
    }

    const ShaderUniformSpec2D* Shader2D::uniform(std::string_view name) const noexcept
    {
        const auto iterator =
            std::lower_bound(m_uniformLayout.begin(), m_uniformLayout.end(), name,
                             [](const ShaderUniformSpec2D& spec, std::string_view value)
                             { return std::string_view(spec.name) < value; });

        if (iterator == m_uniformLayout.end() || std::string_view(iterator->name) != name)
            return nullptr;
        return &*iterator;
    }

    const std::vector<ShaderUniformSpec2D>& Shader2D::uniformLayout() const noexcept
    {
        return m_uniformLayout;
    }

    bool Shader2D::isValidUniformName(std::string_view name) noexcept
    {
        if (name.empty() || name.size() > MaximumUniformNameBytes) return false;

        const auto isIdentifierStart = [](unsigned char character)
        { return std::isalpha(character) != 0 || character == '_'; };
        const auto isIdentifierBody = [](unsigned char character)
        {
            return std::isalnum(character) != 0 || character == '_' || character == '.' ||
                   character == '[' || character == ']';
        };

        if (!isIdentifierStart(static_cast<unsigned char>(name.front()))) return false;

        for (const char character : name)
            if (!isIdentifierBody(static_cast<unsigned char>(character))) return false;

        return true;
    }

    bool Shader2D::isValidUniformType(ShaderUniformType2D type) noexcept
    {
        switch (type)
        {
        case ShaderUniformType2D::Float:
        case ShaderUniformType2D::Integer:
        case ShaderUniformType2D::Boolean:
        case ShaderUniformType2D::Vector2:
        case ShaderUniformType2D::Vector3:
        case ShaderUniformType2D::Color:
        case ShaderUniformType2D::Texture:
            return true;
        }

        return false;
    }

    bool Shader2D::isValidSource(std::string_view source) noexcept
    {
        if (source.empty() || source.size() > MaximumSourceBytes) return false;
        return source.find('\0') == std::string_view::npos;
    }
}
