#include <Lorenzo2D/Renderer/Material2D.hpp>

#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/Glsl.hpp>
#include <SFML/Graphics/RenderStates.hpp>

#include <cmath>
#include <type_traits>
#include <utility>

namespace l2d
{
    namespace
    {
        sf::BlendMode toSfmlBlendMode(MaterialBlendMode2D blendMode)
        {
            switch (blendMode)
            {
            case MaterialBlendMode2D::Alpha:
                return sf::BlendAlpha;
            case MaterialBlendMode2D::Add:
                return sf::BlendAdd;
            case MaterialBlendMode2D::Multiply:
                return sf::BlendMultiply;
            case MaterialBlendMode2D::None:
                return sf::BlendNone;
            }

            return sf::BlendAlpha;
        }
    }

    bool Material2D::setShader(Shader2DHandle shader)
    {
        if (!shader || !shader->loaded()) return false;

        for (const auto& [name, value] : m_uniforms)
        {
            const auto type = typeOf(value);
            const ShaderUniformSpec2D* spec = type ? shader->uniform(name) : nullptr;
            if (spec == nullptr || spec->type != *type) return false;
        }

        m_shader = std::move(shader);
        return true;
    }

    void Material2D::clearShader()
    {
        m_shader.reset();
        m_uniforms.clear();
    }

    Shader2DHandle Material2D::shader() const noexcept
    {
        return m_shader;
    }

    bool Material2D::setBlendMode(MaterialBlendMode2D blendMode) noexcept
    {
        switch (blendMode)
        {
        case MaterialBlendMode2D::Alpha:
        case MaterialBlendMode2D::Add:
        case MaterialBlendMode2D::Multiply:
        case MaterialBlendMode2D::None:
            m_blendMode = blendMode;
            return true;
        }

        return false;
    }

    MaterialBlendMode2D Material2D::blendMode() const noexcept
    {
        return m_blendMode;
    }

    bool Material2D::setFloat(std::string name, float value)
    {
        if (!std::isfinite(value)) return false;
        return setUniform(std::move(name), ShaderUniformType2D::Float, value);
    }

    bool Material2D::setInteger(std::string name, int value)
    {
        return setUniform(std::move(name), ShaderUniformType2D::Integer, value);
    }

    bool Material2D::setBoolean(std::string name, bool value)
    {
        return setUniform(std::move(name), ShaderUniformType2D::Boolean, value);
    }

    bool Material2D::setVector2(std::string name, sf::Vector2f value)
    {
        if (!isFinite(value)) return false;
        return setUniform(std::move(name), ShaderUniformType2D::Vector2, value);
    }

    bool Material2D::setVector3(std::string name, sf::Vector3f value)
    {
        if (!isFinite(value)) return false;
        return setUniform(std::move(name), ShaderUniformType2D::Vector3, value);
    }

    bool Material2D::setColor(std::string name, sf::Color value)
    {
        return setUniform(std::move(name), ShaderUniformType2D::Color, value);
    }

    bool Material2D::setTexture(std::string name, TextureHandle texture)
    {
        if (!texture) return false;
        return setUniform(std::move(name), ShaderUniformType2D::Texture, std::move(texture));
    }

    bool Material2D::setCurrentTexture(std::string name)
    {
        return setUniform(std::move(name), ShaderUniformType2D::Texture,
                          MaterialCurrentTexture2D{});
    }

    bool Material2D::removeUniform(std::string_view name)
    {
        const auto iterator = m_uniforms.find(name);
        if (iterator == m_uniforms.end()) return false;
        m_uniforms.erase(iterator);
        return true;
    }

    void Material2D::clearUniforms()
    {
        m_uniforms.clear();
    }

    std::size_t Material2D::uniformCount() const noexcept
    {
        return m_uniforms.size();
    }

    const MaterialUniformValue2D* Material2D::uniform(std::string_view name) const noexcept
    {
        const auto iterator = m_uniforms.find(name);
        return iterator == m_uniforms.end() ? nullptr : &iterator->second;
    }

    std::optional<ShaderUniformType2D> Material2D::uniformType(std::string_view name) const noexcept
    {
        const MaterialUniformValue2D* value = uniform(name);
        return value == nullptr ? std::nullopt : typeOf(*value);
    }

    bool Material2D::isComplete() const noexcept
    {
        if (!m_shader) return m_uniforms.empty();

        if (!m_shader->loaded()) return false;

        for (const auto& [name, value] : m_uniforms)
        {
            const auto type = typeOf(value);
            if (!type || !uniformMatchesLayout(name, *type)) return false;
        }

        for (const ShaderUniformSpec2D& spec : m_shader->uniformLayout())
            if (spec.required && m_uniforms.find(spec.name) == m_uniforms.end()) return false;

        return true;
    }

    bool Material2D::apply(sf::RenderStates& states) const
    {
        if (!isComplete()) return false;

        sf::RenderStates next = states;
        next.blendMode = toSfmlBlendMode(m_blendMode);

        if (!m_shader)
        {
            next.shader = nullptr;
            states = next;
            return true;
        }

        const auto applyValue = [this](const std::string& name, const MaterialUniformValue2D& value)
        {
            std::visit(
                [this, &name](const auto& typedValue)
                {
                    using Value = std::decay_t<decltype(typedValue)>;

                    if constexpr (std::is_same_v<Value, float> || std::is_same_v<Value, int> ||
                                  std::is_same_v<Value, bool> ||
                                  std::is_same_v<Value, sf::Vector2f> ||
                                  std::is_same_v<Value, sf::Vector3f>)
                    {
                        m_shader->m_shader->setUniform(name, typedValue);
                    }
                    else if constexpr (std::is_same_v<Value, sf::Color>)
                    {
                        const sf::Glsl::Vec4 color = typedValue;
                        m_shader->m_shader->setUniform(name, color);
                    }
                    else if constexpr (std::is_same_v<Value, TextureHandle>)
                    {
                        m_shader->m_shader->setUniform(name, *typedValue);
                    }
                    else if constexpr (std::is_same_v<Value, MaterialCurrentTexture2D>)
                    {
                        m_shader->m_shader->setUniform(name, sf::Shader::CurrentTexture);
                    }
                },
                value);
        };

        const auto applyDefault = [this](const ShaderUniformSpec2D& spec)
        {
            switch (spec.type)
            {
            case ShaderUniformType2D::Float:
                m_shader->m_shader->setUniform(spec.name, 0.f);
                break;
            case ShaderUniformType2D::Integer:
                m_shader->m_shader->setUniform(spec.name, 0);
                break;
            case ShaderUniformType2D::Boolean:
                m_shader->m_shader->setUniform(spec.name, false);
                break;
            case ShaderUniformType2D::Vector2:
                m_shader->m_shader->setUniform(spec.name, sf::Vector2f{});
                break;
            case ShaderUniformType2D::Vector3:
                m_shader->m_shader->setUniform(spec.name, sf::Vector3f{});
                break;
            case ShaderUniformType2D::Color:
            {
                const sf::Glsl::Vec4 transparent = sf::Color::Transparent;
                m_shader->m_shader->setUniform(spec.name, transparent);
                break;
            }
            case ShaderUniformType2D::Texture:
                m_shader->m_shader->setUniform(spec.name, sf::Shader::CurrentTexture);
                break;
            }
        };

        for (const ShaderUniformSpec2D& spec : m_shader->uniformLayout())
        {
            const auto iterator = m_uniforms.find(spec.name);
            if (iterator != m_uniforms.end())
                applyValue(iterator->first, iterator->second);
            else
                applyDefault(spec);
        }

        next.shader = m_shader->m_shader.get();
        states = next;
        return true;
    }

    bool Material2D::setUniform(std::string name, ShaderUniformType2D type,
                                MaterialUniformValue2D value)
    {
        if (!m_shader || !uniformMatchesLayout(name, type)) return false;

        const auto iterator = m_uniforms.find(name);
        if (iterator == m_uniforms.end() && m_uniforms.size() >= Shader2D::MaximumUniformCount)
            return false;

        m_uniforms.insert_or_assign(std::move(name), std::move(value));
        return true;
    }

    bool Material2D::uniformMatchesLayout(std::string_view name,
                                          ShaderUniformType2D type) const noexcept
    {
        if (!m_shader) return false;
        const ShaderUniformSpec2D* spec = m_shader->uniform(name);
        return spec != nullptr && spec->type == type;
    }

    std::optional<ShaderUniformType2D> Material2D::typeOf(
        const MaterialUniformValue2D& value) noexcept
    {
        return std::visit(
            [](const auto& typedValue) -> std::optional<ShaderUniformType2D>
            {
                using Value = std::decay_t<decltype(typedValue)>;

                if constexpr (std::is_same_v<Value, float>)
                    return ShaderUniformType2D::Float;
                else if constexpr (std::is_same_v<Value, int>)
                    return ShaderUniformType2D::Integer;
                else if constexpr (std::is_same_v<Value, bool>)
                    return ShaderUniformType2D::Boolean;
                else if constexpr (std::is_same_v<Value, sf::Vector2f>)
                    return ShaderUniformType2D::Vector2;
                else if constexpr (std::is_same_v<Value, sf::Vector3f>)
                    return ShaderUniformType2D::Vector3;
                else if constexpr (std::is_same_v<Value, sf::Color>)
                    return ShaderUniformType2D::Color;
                else if constexpr (std::is_same_v<Value, TextureHandle> ||
                                   std::is_same_v<Value, MaterialCurrentTexture2D>)
                    return ShaderUniformType2D::Texture;

                return std::nullopt;
            },
            value);
    }

    bool Material2D::isFinite(sf::Vector2f value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    bool Material2D::isFinite(sf::Vector3f value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }
}
