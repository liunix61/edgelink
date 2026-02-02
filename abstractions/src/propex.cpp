#include <tao/pegtl.hpp>

#include "edgelink/edgelink.hpp"
#include "edgelink/propex.hpp"

namespace json = boost::json;
namespace pegtl = tao::pegtl;

namespace edgelink::propex {

struct HexDigit : pegtl::xdigit {};
struct UnicodeChar : pegtl::list<pegtl::seq<pegtl::one<'u'>, pegtl::rep<4, HexDigit>>, pegtl::one<'\\'>> {};
struct EscapedChar : pegtl::one<'"', '\\', '/', 'b', 'f', 'n', 'r', 't'> {};
struct Escaped : pegtl::sor<EscapedChar, UnicodeChar> {};
struct Unescaped : pegtl::utf8::range<0x20, 0x10FFFF> {};
struct Char_ : pegtl::if_then_else<pegtl::one<'\\'>, Escaped, Unescaped> {}; // NOLINT(readability-identifier-naming)

struct StringContent : pegtl::until<pegtl::at<pegtl::one<'"'>>, Char_> {};
struct String : pegtl::seq<pegtl::one<'"'>, StringContent, pegtl::any> {
    using content = StringContent;
};

struct KeyContent : pegtl::until<pegtl::at<pegtl::one<'"'>>, Char_> {};
struct Key : pegtl::seq<pegtl::one<'"'>, KeyContent, pegtl::any> {
    using content = KeyContent;
};

template <char TChar> struct StringLiteralWithout : pegtl::star<pegtl::not_one<TChar, 10, 13>> {};

struct DoubleQuotedString : pegtl::seq<pegtl::one<'"'>, StringLiteralWithout<'"'>, pegtl::one<'"'>> {};
struct SingleQuotedString : pegtl::seq<pegtl::one<'\''>, StringLiteralWithout<'\''>, pegtl::one<'\''>> {};
struct QuotedString : pegtl::sor<DoubleQuotedString, SingleQuotedString> {};

template <typename TSymbol> struct Tokenize : pegtl::pad<TSymbol, pegtl::ascii::space, pegtl::ascii::space> {};

struct Identifier : pegtl::identifier {};
struct IdentifierToken : Tokenize<Identifier> {};
struct DotToken : Tokenize<pegtl::one<'.'>> {};
struct LBracketToken : Tokenize<pegtl::one<'['>> {};
struct RBracketToken : Tokenize<pegtl::one<']'>> {};

struct ArrayIndexInteger : pegtl::plus<pegtl::digit> {};
struct ArrayIndexExpr : pegtl::seq<LBracketToken, Tokenize<ArrayIndexInteger>, RBracketToken> {};

struct StringKeyMapIndex : QuotedString {};
struct StringKeyMapIndexExpr : pegtl::seq<LBracketToken, Tokenize<StringKeyMapIndex>, RBracketToken> {};

struct IndexExpr : pegtl::sor<ArrayIndexExpr, StringKeyMapIndexExpr> {};

struct PropertyExprSegment : pegtl::seq<IdentifierToken, pegtl::star<IndexExpr>> {};

struct PropertyExpr : pegtl::list<PropertyExprSegment, DotToken> {};

template <typename Rule> struct Action {};

template <> struct Action<Identifier> {
    template <typename ActionInput, typename TData> static void apply(const ActionInput& in, TData& data) {
        auto seg_str = std::string_view(in.begin(), in.end());
        auto ps = PropertySegment(std::move(seg_str));
        data.emplace_back(ps);
    }
};

template <> struct Action<ArrayIndexInteger> {
    template <typename ActionInput, typename TData> static void apply(const ActionInput& in, TData& data) {
        auto seg_str = std::string_view(in.begin(), in.end());
        auto i = boost::lexical_cast<size_t>(seg_str);
        auto ps = PropertySegment(i);
        data.push_back(std::move(ps));
    }
};

template <> struct Action<StringKeyMapIndex> {
    template <typename ActionInput, typename TData> static void apply(const ActionInput& in, TData& data) {
        auto seg_str = std::string_view(in.begin() + 1, in.end() - 1); // trim the quotes
        auto ps = PropertySegment(std::move(seg_str));
        data.emplace_back(ps);
    }
};

bool try_parse(const std::string_view input, PropertySegments& result) {
    tao::pegtl::memory_input mi(input, "");
    return tao::pegtl::parse<PropertyExpr, Action>(mi, result);
}

const PropertySegments parse(const std::string_view input) {
    PropertySegments result;
    if (!try_parse(input, result)) {
        throw std::runtime_error("Failed to parse input");
    }
    return result;
}

// Assuming you have a getMessageProperty function
// and other required functions declared and defined.

JsonValue evaluate_property_value(const JsonValue& value, const std::string_view type, const INode& node,
                                  const Msg& msg) {

    auto result = value;

    if (type == "str") {
        result = value;
    } else if (type == "num") {
        if (value.is_string()) {
            try {
                result = json::parse(value.as_string());
            } catch (const std::exception&) {
                throw std::runtime_error("Invalid number format");
                return result;
            }
        } else if (value.is_number()) {
            result = value;
        } else {
            throw std::runtime_error("Invalid number format");
        }
    } else if (type == "json") {
        try {
            if (value.is_string()) {
                result = json::parse(value.as_string());
            } else {
                result = value;
            }
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid JSON format");
        }
    } else if (type == "re") {
        /*
        try {
            result = std::regex(value.as_string().c_str());
        } catch (const std::exception&) {
            if (callback) {
                callback("Invalid regular expression");
            } else {
                throw std::runtime_error("Invalid regular expression");
            }
            return result;
        }
        */
        TODO("不支持正则");
    } else if (type == "date") {
        // Your date conversion logic here
        auto now = std::chrono::system_clock::now();
        auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        result = int64_t(time_ms);
    } else if (type == "bin") {
        try {
            JsonValue data = boost::json::parse(value.as_string());
            if(data.is_array()) {
                result = data.as_array();
            }
            else if(data.is_string()) {
                const auto& bin_str = data.as_string();
                auto ja = JsonArray();
                for(auto c: bin_str) {
                    ja.emplace_back(JsonValue(c));
                }
                result = ja;
            } else {
                throw std::runtime_error("Invalid buffer data");
            }
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid JSON format");
        }
    } else if (type == "msg") {
        result = JsonValue(msg.at_propex(value.as_string()));
    } else if (type == "flow") {
        //
        TODO("暂时不知支持");
    } else if (type == "global") {
        /*
        ContextKey contextKey = parseContextStore(value.as_string().c_str());
        if (std::regex_search(contextKey.key, std::regex("\\[msg"))) {
            // The key has a nested msg reference to evaluate first
            contextKey.key = normalisePropertyExpression(contextKey.key, msg, true);
        }
        result = node->context()[type]->get(contextKey.key, contextKey.store, callback);
        */
        TODO("暂时不知支持");
    } else if (type == "bool") {
        result = json::parse(value.as_string());
    } else if (type == "jsonata") {
        TODO("暂时不知支持 jsonata");
    } else if (type == "env") {
        TODO("暂时不知支持 env");
    }

    return result;
}

}; // namespace edgelink::propex