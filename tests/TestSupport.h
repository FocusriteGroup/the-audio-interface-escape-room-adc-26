#pragma once

// Checks shared by the test programs. A failed check prints the test function and
// line it came from, then the message, and exits so CTest reports a failure:
//
//     [testRouting() at MissionTests.cpp:67] route Input 1 -> Output R: ...

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <version>

#if defined(__cpp_lib_source_location)
#include <source_location>
#endif

namespace test_support
{
#if defined(__cpp_lib_source_location)
using Location = std::source_location;
#else
// Older standard libraries without <source_location>: the same compiler intrinsics.
struct Location
{
    static constexpr Location current(const char* function = __builtin_FUNCTION(),
                                      const char* file = __builtin_FILE(),
                                      unsigned line = __builtin_LINE()) noexcept
    {
        return { function, file, line };
    }
    constexpr const char* function_name() const noexcept { return function; }
    constexpr const char* file_name() const noexcept { return file; }
    constexpr unsigned line() const noexcept { return lineNumber; }

    const char* function = "";
    const char* file = "";
    unsigned lineNumber = 0;
};
#endif

// Compilers spell function names differently, e.g.
// "void (anonymous namespace)::testRouting()". Keep just "testRouting()".
inline std::string functionName(const Location& where)
{
    std::string_view name = where.function_name();
    if (const auto close = name.rfind(')'); close != std::string_view::npos)
    {
        // Drop the parameter list, matching brackets back from the last ')'.
        auto depth = 0;
        for (auto index = close + 1; index-- > 0;)
        {
            depth += name[index] == ')' ? 1 : name[index] == '(' ? -1 : 0;
            if (depth == 0)
            {
                name = name.substr(0, index);
                break;
            }
        }
    }
    for (const auto* separator : { "::", " ", "`" })
        if (const auto position = name.rfind(separator); position != std::string_view::npos)
            name = name.substr(position + std::string_view(separator).size());
    return std::string(name) + "()";
}

inline std::string_view fileName(const Location& where)
{
    const std::string_view path = where.file_name();
    const auto separator = path.find_last_of("/\\");
    return separator == std::string_view::npos ? path : path.substr(separator + 1);
}

[[noreturn]] inline void fail(std::string_view message, const Location& where)
{
    std::cerr << '[' << functionName(where) << " at " << fileName(where) << ':'
              << where.line() << "] " << message << '\n';
    std::exit(EXIT_FAILURE);
}

inline void require(bool condition, std::string_view message,
                    const Location& where = Location::current())
{
    if (!condition)
        fail(message, where);
}

inline void requireNear(float actual, float expected, float tolerance, std::string_view message,
                        const Location& where = Location::current())
{
    if (std::isfinite(actual) && std::abs(actual - expected) <= tolerance)
        return;
    std::ostringstream text;
    text << message << ": expected " << expected << ", got " << actual;
    fail(text.str(), where);
}
}

using test_support::require;
using test_support::requireNear;
