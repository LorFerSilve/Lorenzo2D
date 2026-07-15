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
#include <type_traits>
#include <utility>

namespace l2d::test
{
    namespace detail
    {
        template <typename Value, typename = void>
        struct HasVector2Components : std::false_type
        {
        };

        template <typename Value>
        struct HasVector2Components<
            Value,
            std::void_t<
                decltype(std::declval<const Value&>().x),
                decltype(std::declval<const Value&>().y)
            >
        > : std::true_type
        {
        };

        template <typename Value, typename = void>
        struct IsStreamInsertable : std::false_type
        {
        };

        template <typename Value>
        struct IsStreamInsertable<
            Value,
            std::void_t<decltype(
                std::declval<std::ostream&>() <<
                std::declval<const Value&>()
            )>
        > : std::true_type
        {
        };

        template <typename Value>
        std::string formatValue(const Value& value)
        {
            using ValueType =
                std::remove_cv_t<std::remove_reference_t<Value>>;

            if constexpr (std::is_enum_v<ValueType>)
            {
                using UnderlyingType = std::underlying_type_t<ValueType>;
                return formatValue(static_cast<UnderlyingType>(value));
            }
            else if constexpr (HasVector2Components<ValueType>::value)
            {
                return "(" + formatValue(value.x) + ", " +
                    formatValue(value.y) + ")";
            }
            else if constexpr (IsStreamInsertable<ValueType>::value)
            {
                std::ostringstream stream;
                stream << std::boolalpha << value;
                return stream.str();
            }
            else
            {
                return "<unprintable>";
            }
        }

        template <typename Left, typename Right>
        constexpr bool valuesEqual(const Left& left, const Right& right)
        {
            using LeftValue = std::remove_cv_t<std::remove_reference_t<Left>>;
            using RightValue = std::remove_cv_t<std::remove_reference_t<Right>>;

            if constexpr (
                std::is_integral_v<LeftValue> &&
                std::is_integral_v<RightValue> &&
                !std::is_same_v<LeftValue, bool> &&
                !std::is_same_v<RightValue, bool>
            )
            {
                if constexpr (
                    std::is_signed_v<LeftValue> ==
                    std::is_signed_v<RightValue>
                )
                {
                    return left == right;
                }
                else if constexpr (std::is_signed_v<LeftValue>)
                {
                    if (left < 0)
                        return false;

                    using UnsignedLeft = std::make_unsigned_t<LeftValue>;
                    using UnsignedRight = std::make_unsigned_t<RightValue>;
                    using CommonUnsigned =
                        std::common_type_t<UnsignedLeft, UnsignedRight>;

                    return static_cast<CommonUnsigned>(
                        static_cast<UnsignedLeft>(left)
                    ) == static_cast<CommonUnsigned>(right);
                }
                else
                {
                    if (right < 0)
                        return false;

                    using UnsignedLeft = std::make_unsigned_t<LeftValue>;
                    using UnsignedRight = std::make_unsigned_t<RightValue>;
                    using CommonUnsigned =
                        std::common_type_t<UnsignedLeft, UnsignedRight>;

                    return static_cast<CommonUnsigned>(left) ==
                        static_cast<CommonUnsigned>(
                            static_cast<UnsignedRight>(right)
                        );
                }
            }
            else
            {
                return left == right;
            }
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
        if (detail::valuesEqual(actual, expected))
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
            " (actual: " + detail::formatValue(actual) +
            ", expected: " + detail::formatValue(expected) +
            ", epsilon: " + detail::formatValue(epsilon) + ")"
        );
    }

    template <
        typename Vector,
        typename Epsilon,
        std::enable_if_t<
            detail::HasVector2Components<
                std::remove_cv_t<std::remove_reference_t<Vector>>
            >::value,
            int
        > = 0
    >
    void requireApproximatelyEqual(
        const Vector& actual,
        const Vector& expected,
        Epsilon epsilon,
        const char* actualExpression,
        const char* expectedExpression,
        int line
    )
    {
        requireApproximatelyEqual2D(
            actual,
            expected,
            epsilon,
            actualExpression,
            expectedExpression,
            line
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
