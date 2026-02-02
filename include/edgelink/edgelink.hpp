#pragma once

#include "export.hpp"
#include "common.hpp"
#include "errors.hpp"
#include "utils.hpp"
#include "json.hpp"
#include "propex.hpp"
#include "settings.hpp"

#include "flows/common.hpp"
#include "flows/msg.hpp"
#include "flows/abstractions.hpp"
#include "flows/engine.hpp"
#include "flows/registry.hpp"
#include "flows/engine.hpp"

namespace edgelink {

struct EDGELINK_EXPORT IDaemonApp {
    virtual void run() = 0;
};

}; // namespace edgelink

/*
template <> struct fmt::formatter<std::decimal::decimal64> {
    // Presentation format: 'f' - fixed, 'e' - exponential.
    char presentation = 'f';

    // Parses format specifications of the form ['f' | 'e'].
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        // [ctx.begin(), ctx.end()) is a character range that contains a part of
        // the format string starting from the format specifications to be parsed,
        // e.g. in
        //
        //   fmt::format("{:f} - point of interest", point{1, 2});
        //
        // the range will contain "f} - point of interest". The formatter should
        // parse specifiers until '}' or the end of the range. In this example
        // the formatter should parse the 'f' specifier and return an iterator
        // pointing to '}'.

        // Please also note that this character range may be empty, in case of
        // the "{}" format string, so therefore you should check ctx.begin()
        // for equality with ctx.end().

        // Parse the presentation format and store it in the formatter:
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'f' || *it == 'e')) {
            presentation = *it++;
        }

        // Check if reached the end of the range:
        if (it != end && *it != '}') {
            fmt::detail::throw_format_error("invalid format");
        }

        // Return an iterator past the end of the parsed range:
        return it;
    }

    // Formats the point p using the parsed format specification (presentation)
    // stored in this formatter.
    auto format(const std::decimal::decimal64& p, format_context& ctx) const -> format_context::iterator {
        // ctx.out() is an output iterator to write to.
        return presentation == 'f' ? fmt::format_to(ctx.out(), "{:.1f}", std::decimal::decimal64_to_double(p))
                                   : fmt::format_to(ctx.out(), "{:.1e}", std::decimal::decimal64_to_double(p));
    }

*/
