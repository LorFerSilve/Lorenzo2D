#pragma once

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace l2d::test
{
    namespace detail
    {
        template <typename Value>
        std::string formatValue(const Value& value)
        {
            std::ostringstream stream;
            stream << std::boolalpha << value;
            return stream.str();
        }

        template <typename Vector>
        std::string formatVector2(const Vector& value)
        {
            return "(" + formatValue(value.x) + ", " +
                formatValue(value.y) + ")";
        }

        template <typename Actual, typename Expected, typename Epsilon>
        [[noreturn]] void throwApproximateFailure(
            const Actual& actual,
            const Expected& expected,
            const Epsilon& epsilon,
            const char* actualExpression,
            const char* expectedExpression,
            int line
        )
        {
            throw std::runtime_error(
                "line " + std::to_string(line) + ": expected " +
                actualExpression + " ~= " + expectedExpression +
                " (actual: " + formatValue(actual) +
                ", expected: " + formatValue(expected) +
                ", epsilon: " + formatValue(epsilon) + ")"
            );
        }
    }

    inline void require(bool condition, const char* expression, int line)
    {
        if (condition)
            return;

        throw std::runtime_error(
            "line " + std::to_string(line) + ": " + expression
        );
    }

    template <typename Actual, typename Expected>
    void requireEqual(
        const Actual& actual,
        const Expected& expected,
        const char* actualExpression,
        const char* expectedExpression,
        int line
    )
    {
        if (actual == expected)
            return;

        throw std::runtime_error(
            "line " + std::to_string(line) + ": expected " +
            actualExpression + " == " + expectedExpression +
            " (actual: " + detail::formatValue(actual) +
            ", expected: " + detail::formatValue(expected) + ")"
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

    inline void requireApproximatelyEqual(
        float actual,
        float expected,
        float epsilon,
        const char* actualExpression,
        const char* expectedExpression,
        int line
    )
    {
        if (approximatelyEqual(actual, expected, epsilon))
            return;

        detail::throwApproximateFailure(
            actual,
            expected,
            epsilon,
            actualExpression,
            expectedExpression,
            line
        );
    }

    inline void requireApproximatelyEqual(
        double actual,
        double expected,
        double epsilon,
        const char* actualExpression,
        const char* expectedExpression,
        int line
    )
    {
        if (approximatelyEqual(actual, expected, epsilon))
            return;

        detail::throwApproximateFailure(
            actual,
            expected,
            epsilon,
            actualExpression,
            expectedExpression,
            line
        );
    }

    template <typename Vector, typename Epsilon>
    bool approximatelyEqual2D(
        const Vector& left,
        const Vector& right,
        Epsilon epsilon
    )
    {
        return approximatelyEqual(left.x, right.x, epsilon) &&
            approximatelyEqual(left.y, right.y, epsilon);
    }

    template <typename Vector, typename Epsilon>
    void requireApproximatelyEqual2D(
        const Vector& actual,
        const Vector& expected,
        Epsilon epsilon,
        const char* actualExpression,
        const char* expectedExpression,
        int line
    )
    {
        if (approximatelyEqual2D(actual, expected, epsilon))
            return;

        throw std::runtime_error(
            "line " + std::to_string(line) + ": expected " +
            actualExpression + " ~= " + expectedExpression +
            " (actual: " + detail::formatVector2(actual) +
            ", expected: " + detail::formatVector2(expected) +
            ", epsilon: " + detail::formatValue(epsilon) + ")"
        );
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

#define L2D_REQUIRE_EQUAL(actual, expected) \
    ::l2d::test::requireEqual( \
        (actual), \
        (expected), \
        #actual, \
        #expected, \
        __LINE__ \
    )

#define L2D_REQUIRE_APPROX(actual, expected, epsilon) \
    ::l2d::test::requireApproximatelyEqual( \
        (actual), \
        (expected), \
        (epsilon), \
        #actual, \
        #expected, \
        __LINE__ \
    )

#define L2D_REQUIRE_APPROX_2D(actual, expected, epsilon) \
    ::l2d::test::requireApproximatelyEqual2D( \
        (actual), \
        (expected), \
        (epsilon), \
        #actual, \
        #expected, \
        __LINE__ \
    )
