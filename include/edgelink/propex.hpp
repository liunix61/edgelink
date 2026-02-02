#pragma once

namespace edgelink {

struct INode;
class Msg;

};


namespace edgelink::propex {

const size_t PROPERTY_SEGMENT_MAX = 16;

using PropertyValue = std::variant<                     //
    std::string,                                        //
    double,                                             //
    JsonValue,                                          //
    std::chrono::time_point<std::chrono::system_clock>, //
    std::vector<uint8_t>,                               //
    bool                                                //
    >;

enum class PropertySegmentKind : size_t {
    IDENTIFIER = 0,
    INT_INDEX,
};

using PropertySegment = std::variant<std::string_view, size_t>;

using PropertySegments = boost::container::static_vector<PropertySegment, PROPERTY_SEGMENT_MAX>;

inline PropertySegmentKind kind(const PropertySegment& seg) { return static_cast<PropertySegmentKind>(seg.index()); }

EDGELINK_EXPORT bool try_parse(const std::string_view input, PropertySegments& result);

EDGELINK_EXPORT const PropertySegments parse(const std::string_view input);

EDGELINK_EXPORT JsonValue evaluate_property_value(const JsonValue& value, const std::string_view type,
                                                  const INode& node, const Msg& msg);

}; // namespace edgelink::flows::propex
