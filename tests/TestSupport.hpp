#pragma once

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace l2d::test
{
    inline void require(bool condition, const char* expression, int line)
    {
        if (condition)
            return;

        throw std::runtime_error(
            "line " + std::to_string(line) + ": " + expression
        );
    }

    inline bool approximatelyEqual(
        float left,
        float right,
        float epsilon = 0.0001f
    )
    {
        return std::fabs(left - right) <= epsilon;
    }

    inline bool approximatelyEqual(
        double left,
        double right,
        double epsilon = 0.000000001
    )
    {
        return std::fabs(left - right) <= epsilon;
    }

    class TemporaryFile final
    {
    public:
        explicit TemporaryFile(
            const std::string& prefix,
            const std::string& extension = ".txt"
        )
        {
            const std::chrono::high_resolution_clock::duration::rep timestamp =
                std::chrono::high_resolution_clock::now()
                    .time_since_epoch()
                    .count();
            const std::random_device::result_type randomValue =
                std::random_device{}();

            m_path = std::filesystem::temp_directory_path() /
                (prefix + "_" + std::to_string(timestamp) + "_" +
                    std::to_string(randomValue) + extension);
        }

        ~TemporaryFile()
        {
            std::error_code error;
            std::filesystem::remove(m_path, error);
        }

        TemporaryFile(const TemporaryFile&) = delete;
        TemporaryFile& operator=(const TemporaryFile&) = delete;
        TemporaryFile(TemporaryFile&&) = delete;
        TemporaryFile& operator=(TemporaryFile&&) = delete;

        const std::filesystem::path& path() const
        {
            return m_path;
        }

    private:
        std::filesystem::path m_path;
    };

    template <typename Function>
    void runTest(const char* name, Function&& function, int& failures)
    {
        try
        {
            std::forward<Function>(function)();
            std::cout << "[PASS] " << name << '\n';
        }
        catch (const std::exception& exception)
        {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << exception.what()
                << '\n';
        }
    }
}

#define L2D_REQUIRE(expression) \
    ::l2d::test::require(static_cast<bool>(expression), #expression, __LINE__)
