#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace l2d
{
    using SaveValue = std::variant<bool, std::int64_t, double, std::string>;

    class SaveDocument
    {
      public:
        SaveDocument() = default;
        SaveDocument(std::string schema, std::uint32_t revision);

        bool setIdentity(std::string schema, std::uint32_t revision);

        const std::string& schema() const noexcept;
        std::uint32_t revision() const noexcept;

        bool setBool(std::string key, bool value);
        bool setInteger(std::string key, std::int64_t value);
        bool setNumber(std::string key, double value);
        bool setString(std::string key, std::string value);

        const SaveValue* find(std::string_view key) const noexcept;
        std::optional<bool> boolean(std::string_view key) const;
        std::optional<std::int64_t> integer(std::string_view key) const;
        std::optional<double> number(std::string_view key) const;
        std::optional<std::string> string(std::string_view key) const;

        bool contains(std::string_view key) const noexcept;
        bool erase(std::string_view key);
        void clearValues() noexcept;
        std::size_t size() const noexcept;
        const std::map<std::string, SaveValue>& values() const noexcept;

        friend bool operator==(const SaveDocument& left, const SaveDocument& right) noexcept;
        friend bool operator!=(const SaveDocument& left, const SaveDocument& right) noexcept;

      private:
        bool set(std::string key, SaveValue value);

        std::string m_schema = "default";
        std::uint32_t m_revision = 1u;
        std::map<std::string, SaveValue> m_values;
    };

    struct SaveGameLimits
    {
        std::size_t maxEntries = 4096u;
        std::size_t maxKeyBytes = 128u;
        std::size_t maxSchemaBytes = 128u;
        std::size_t maxStringBytes = 1024u * 1024u;
        std::size_t maxTotalBytes = 4u * 1024u * 1024u;
    };

    class SaveGameSerializer
    {
      public:
        static constexpr std::uint32_t FormatVersion = 1u;

        static bool save(std::ostream& output, const SaveDocument& document,
                         SaveGameLimits limits = {});
        static bool load(std::istream& input, SaveDocument& document, SaveGameLimits limits = {});

        static bool saveToFile(const std::filesystem::path& path, const SaveDocument& document,
                               SaveGameLimits limits = {});
        static bool loadFromFile(const std::filesystem::path& path, SaveDocument& document,
                                 SaveGameLimits limits = {});
    };
}
