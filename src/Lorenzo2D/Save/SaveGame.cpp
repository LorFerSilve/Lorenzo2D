#include <Lorenzo2D/Save/SaveGame.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace l2d
{
    namespace
    {
        using Json = nlohmann::ordered_json;

        bool validLimits(const SaveGameLimits& limits)
        {
            return limits.maxEntries > 0u && limits.maxKeyBytes > 0u &&
                   limits.maxSchemaBytes > 0u && limits.maxStringBytes > 0u &&
                   limits.maxTotalBytes > 0u;
        }

        bool valueWithinLimits(const SaveValue& value, const SaveGameLimits& limits)
        {
            if (const auto* number = std::get_if<double>(&value)) return std::isfinite(*number);

            if (const auto* text = std::get_if<std::string>(&value))
                return text->size() <= limits.maxStringBytes;

            return true;
        }

        bool documentWithinLimits(const SaveDocument& document, const SaveGameLimits& limits)
        {
            if (!validLimits(limits) || document.schema().empty() ||
                document.schema().size() > limits.maxSchemaBytes || document.revision() == 0u ||
                document.size() > limits.maxEntries)
                return false;

            for (const auto& [key, value] : document.values())
            {
                if (key.empty() || key.size() > limits.maxKeyBytes ||
                    !valueWithinLimits(value, limits))
                    return false;
            }

            return true;
        }

        Json encodeValue(const SaveValue& value)
        {
            Json encoded = Json::object();

            if (const auto* boolean = std::get_if<bool>(&value))
            {
                encoded["type"] = "bool";
                encoded["value"] = *boolean;
            }
            else if (const auto* integer = std::get_if<std::int64_t>(&value))
            {
                encoded["type"] = "integer";
                encoded["value"] = *integer;
            }
            else if (const auto* number = std::get_if<double>(&value))
            {
                encoded["type"] = "number";
                encoded["value"] = *number;
            }
            else
            {
                encoded["type"] = "string";
                encoded["value"] = std::get<std::string>(value);
            }

            return encoded;
        }

        bool readAll(std::istream& input, std::string& content, std::size_t maximumBytes)
        {
            content.clear();
            std::array<char, 4096u> buffer{};

            while (input)
            {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const std::streamsize readCount = input.gcount();
                if (readCount <= 0) break;

                const std::size_t count = static_cast<std::size_t>(readCount);
                if (count > maximumBytes - std::min(maximumBytes, content.size())) return false;
                content.append(buffer.data(), count);
            }

            return !input.bad();
        }

        bool decodeValue(const Json& encoded, const SaveGameLimits& limits, SaveValue& value)
        {
            if (!encoded.is_object() || !encoded.contains("type") || !encoded.contains("value") ||
                !encoded["type"].is_string())
                return false;

            const std::string type = encoded["type"].get<std::string>();
            const Json& raw = encoded["value"];

            if (type == "bool")
            {
                if (!raw.is_boolean()) return false;
                value = raw.get<bool>();
                return true;
            }

            if (type == "integer")
            {
                if (!raw.is_number_integer() && !raw.is_number_unsigned()) return false;

                try
                {
                    value = raw.get<std::int64_t>();
                    return true;
                }
                catch (const std::exception&)
                {
                    return false;
                }
            }

            if (type == "number")
            {
                if (!raw.is_number()) return false;
                const double number = raw.get<double>();
                if (!std::isfinite(number)) return false;
                value = number;
                return true;
            }

            if (type == "string")
            {
                if (!raw.is_string()) return false;
                std::string text = raw.get<std::string>();
                if (text.size() > limits.maxStringBytes) return false;
                value = std::move(text);
                return true;
            }

            return false;
        }

        std::filesystem::path siblingPath(const std::filesystem::path& path, const char* suffix)
        {
            std::filesystem::path result = path;
            result += suffix;
            return result;
        }
    }

    SaveDocument::SaveDocument(std::string schema, std::uint32_t revision)
    {
        if (!setIdentity(std::move(schema), revision))
        {
            m_schema = "default";
            m_revision = 1u;
        }
    }

    bool SaveDocument::setIdentity(std::string schema, std::uint32_t revision)
    {
        if (schema.empty() || revision == 0u) return false;

        m_schema = std::move(schema);
        m_revision = revision;
        return true;
    }

    const std::string& SaveDocument::schema() const noexcept
    {
        return m_schema;
    }

    std::uint32_t SaveDocument::revision() const noexcept
    {
        return m_revision;
    }

    bool SaveDocument::setBool(std::string key, bool value)
    {
        return set(std::move(key), value);
    }

    bool SaveDocument::setInteger(std::string key, std::int64_t value)
    {
        return set(std::move(key), value);
    }

    bool SaveDocument::setNumber(std::string key, double value)
    {
        if (!std::isfinite(value)) return false;
        return set(std::move(key), value);
    }

    bool SaveDocument::setString(std::string key, std::string value)
    {
        return set(std::move(key), std::move(value));
    }

    const SaveValue* SaveDocument::find(std::string_view key) const noexcept
    {
        const auto iterator = m_values.find(std::string(key));
        return iterator == m_values.end() ? nullptr : &iterator->second;
    }

    std::optional<bool> SaveDocument::boolean(std::string_view key) const
    {
        const SaveValue* value = find(key);
        const auto* result = value != nullptr ? std::get_if<bool>(value) : nullptr;
        return result != nullptr ? std::optional<bool>(*result) : std::nullopt;
    }

    std::optional<std::int64_t> SaveDocument::integer(std::string_view key) const
    {
        const SaveValue* value = find(key);
        const auto* result = value != nullptr ? std::get_if<std::int64_t>(value) : nullptr;
        return result != nullptr ? std::optional<std::int64_t>(*result) : std::nullopt;
    }

    std::optional<double> SaveDocument::number(std::string_view key) const
    {
        const SaveValue* value = find(key);
        const auto* result = value != nullptr ? std::get_if<double>(value) : nullptr;
        return result != nullptr ? std::optional<double>(*result) : std::nullopt;
    }

    std::optional<std::string> SaveDocument::string(std::string_view key) const
    {
        const SaveValue* value = find(key);
        const auto* result = value != nullptr ? std::get_if<std::string>(value) : nullptr;
        return result != nullptr ? std::optional<std::string>(*result) : std::nullopt;
    }

    bool SaveDocument::contains(std::string_view key) const noexcept
    {
        return find(key) != nullptr;
    }

    bool SaveDocument::erase(std::string_view key)
    {
        return m_values.erase(std::string(key)) != 0u;
    }

    void SaveDocument::clearValues() noexcept
    {
        m_values.clear();
    }

    std::size_t SaveDocument::size() const noexcept
    {
        return m_values.size();
    }

    const std::map<std::string, SaveValue>& SaveDocument::values() const noexcept
    {
        return m_values;
    }

    bool operator==(const SaveDocument& left, const SaveDocument& right) noexcept
    {
        return left.m_schema == right.m_schema && left.m_revision == right.m_revision &&
               left.m_values == right.m_values;
    }

    bool operator!=(const SaveDocument& left, const SaveDocument& right) noexcept
    {
        return !(left == right);
    }

    bool SaveDocument::set(std::string key, SaveValue value)
    {
        if (key.empty()) return false;

        if (const auto* number = std::get_if<double>(&value))
            if (!std::isfinite(*number)) return false;

        m_values.insert_or_assign(std::move(key), std::move(value));
        return true;
    }

    bool SaveGameSerializer::save(std::ostream& output, const SaveDocument& document,
                                  SaveGameLimits limits)
    {
        if (!output || !documentWithinLimits(document, limits)) return false;

        Json root = Json::object();
        root["format_version"] = FormatVersion;
        root["schema"] = document.schema();
        root["revision"] = document.revision();
        Json values = Json::object();

        for (const auto& [key, value] : document.values())
            values[key] = encodeValue(value);

        root["values"] = std::move(values);
        const std::string serialized = root.dump(2) + "\n";
        if (serialized.size() > limits.maxTotalBytes) return false;

        output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        return static_cast<bool>(output);
    }

    bool SaveGameSerializer::load(std::istream& input, SaveDocument& document,
                                  SaveGameLimits limits)
    {
        if (!input || !validLimits(limits)) return false;

        std::string content;
        if (!readAll(input, content, limits.maxTotalBytes) || content.empty()) return false;

        const Json root = Json::parse(content, nullptr, false);
        if (root.is_discarded() || !root.is_object() || !root.contains("format_version") ||
            !root.contains("schema") || !root.contains("revision") || !root.contains("values") ||
            !root["format_version"].is_number_unsigned() || !root["schema"].is_string() ||
            !root["revision"].is_number_unsigned() || !root["values"].is_object())
            return false;

        try
        {
            if (root["format_version"].get<std::uint64_t>() != FormatVersion) return false;

            const std::string schema = root["schema"].get<std::string>();
            const std::uint64_t revision = root["revision"].get<std::uint64_t>();
            if (schema.empty() || schema.size() > limits.maxSchemaBytes || revision == 0u ||
                revision > std::numeric_limits<std::uint32_t>::max() ||
                root["values"].size() > limits.maxEntries)
                return false;

            SaveDocument candidate(schema, static_cast<std::uint32_t>(revision));

            for (auto iterator = root["values"].begin(); iterator != root["values"].end();
                 ++iterator)
            {
                const std::string& key = iterator.key();
                if (key.empty() || key.size() > limits.maxKeyBytes) return false;

                SaveValue value;
                if (!decodeValue(iterator.value(), limits, value)) return false;

                if (const auto* boolean = std::get_if<bool>(&value))
                {
                    if (!candidate.setBool(key, *boolean)) return false;
                }
                else if (const auto* integer = std::get_if<std::int64_t>(&value))
                {
                    if (!candidate.setInteger(key, *integer)) return false;
                }
                else if (const auto* number = std::get_if<double>(&value))
                {
                    if (!candidate.setNumber(key, *number)) return false;
                }
                else if (!candidate.setString(key, std::get<std::string>(std::move(value))))
                {
                    return false;
                }
            }

            document = std::move(candidate);
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    bool SaveGameSerializer::saveToFile(const std::filesystem::path& path,
                                        const SaveDocument& document, SaveGameLimits limits)
    {
        if (path.empty()) return false;

        const std::filesystem::path temporary = siblingPath(path, ".tmp");
        const std::filesystem::path backup = siblingPath(path, ".bak");
        std::error_code error;
        std::filesystem::remove(temporary, error);
        error.clear();

        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output || !save(output, document, limits))
            {
                output.close();
                std::filesystem::remove(temporary, error);
                return false;
            }
            output.close();
            if (!output)
            {
                std::filesystem::remove(temporary, error);
                return false;
            }
        }

        const bool hadOriginal = std::filesystem::exists(path, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            return false;
        }

        if (hadOriginal)
        {
            std::filesystem::remove(backup, error);
            error.clear();
            std::filesystem::rename(path, backup, error);
            if (error)
            {
                std::filesystem::remove(temporary, error);
                return false;
            }
        }

        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            if (hadOriginal)
            {
                std::error_code restoreError;
                std::filesystem::rename(backup, path, restoreError);
            }
            std::filesystem::remove(temporary, error);
            return false;
        }

        if (hadOriginal)
        {
            error.clear();
            std::filesystem::remove(backup, error);
        }

        return true;
    }

    bool SaveGameSerializer::loadFromFile(const std::filesystem::path& path, SaveDocument& document,
                                          SaveGameLimits limits)
    {
        std::ifstream input(path, std::ios::binary);
        return input && load(input, document, limits);
    }
}
