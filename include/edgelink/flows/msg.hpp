#pragma once

namespace edgelink {

struct IFlowNode;

using MsgID = uint32_t;


enum class MsgValueKind : unsigned char {
    NULLPTR,
    DOUBLE,
    INT64,
    BOOL,
    STRING,
    BUFFER,
};

using MsgValue = boost::variant< //
    std::nullptr_t,              //
    double,                      //
    int64_t,                     //
    bool,                        //
    std::string,                 //
    std::vector<uint8_t>         //
    >;

inline MsgValueKind kind(const MsgValue& value) { return static_cast<MsgValueKind>(value.which()); }

struct FlowNode;

class EDGELINK_EXPORT Msg final : private boost::noncopyable {
  public:
    Msg(IFlowNode* birth_place = nullptr) : Msg(Msg::generate_msg_id(), birth_place) {}

    Msg(MsgID id, IFlowNode* birth_place = nullptr)
        : _birth_place(birth_place), _data(std::move(JsonObject({{"_msgid", id}, {"payload", nullptr}}))) {}

    Msg(Msg&& other) : _birth_place(other._birth_place), _data(std::move(other._data)) {}

    Msg(JsonObject const& data, IFlowNode* birth_place = nullptr) : _birth_place(birth_place), _data(data) {}

    Msg(JsonObject&& data, IFlowNode* birth_place) : _birth_place(birth_place), _data(std::move(data)) {}

    JsonObject& data() { return _data; }

    JsonObject const& data() const { return _data; }

    MsgID id() const {
        MsgID id = _data.at("_msgid").to_number<MsgID>();
        return id;
    }

    IFlowNode* birth_place() { return _birth_place; }
    const IFlowNode* birth_place() const { return _birth_place; }

    /*
    const std::optional<std::string_view> topic() const {
        auto iter = _data.find("topic");
        return iter = _data.end() ? std::optional<std::string_view>((*iter).as_string()) : std::optional<std::string_view>();
    }
    */

    void set_id(MsgID new_id);

    std::shared_ptr<Msg> clone(bool new_id = true) const;

    inline const JsonString to_json_string() const { return JsonString(std::move(boost::json::serialize(_data))); }
    inline const std::string to_string() const { return boost::json::serialize(_data); }

    JsonValue const& at_propex(const std::string_view propex) const&;

    JsonValue& at_propex(const std::string_view propex) & {
        auto const& self = *this;
        return const_cast<JsonValue&>(self.at_propex(propex));
    }
    JsonValue&& at_propex(const std::string_view propex) && { return std::move(this->at_propex(propex)); }

    JsonValue const& at(const std::string_view prop) const& { return _data.at(prop); }

    JsonValue& at(const std::string_view prop) & {
        auto const& self = *this;
        return const_cast<JsonValue&>(self.at(prop));
    }

    JsonValue&& at(const std::string_view prop) && { return std::move(this->at(prop)); }

    void insert_or_assign(const std::string_view prop_expr, JsonValue&& value) {
        this->data().insert_or_assign(prop_expr, std::move(value));
    }

    static MsgID generate_msg_id();

  private:
    IFlowNode* _birth_place;
    JsonObject _data;
};

using MsgPtr = std::shared_ptr<Msg>;

}; // namespace edgelink
