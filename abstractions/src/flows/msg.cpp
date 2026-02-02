#include "edgelink/edgelink.hpp"

namespace edgelink {

std::shared_ptr<Msg> Msg::clone(bool new_id) const {
    //? 是否要重新生成消息 ID?
    auto new_json = JsonObject(_data);
    auto msg = std::make_shared<Msg>(std::move(new_json), _birth_place);
    if (new_id) {
        msg->set_id(generate_msg_id());
    }
    return msg;
}

void Msg::set_id(MsgID new_id) { this->insert_or_assign("_msgid", new_id); }

JsonValue const& Msg::at_propex(const std::string_view propex) const& {
    auto prop_segs = propex::parse(propex);
    const JsonValue* presult = nullptr;
    for (size_t i = 0; i < prop_segs.size(); i++) {
        const auto& ps = prop_segs[i];
        if (i == 0) {
            if (propex::kind(ps) != propex::PropertySegmentKind::IDENTIFIER) {
                throw InvalidDataException("Bad propex");
            }
            std::string key(std::get<std::string_view>(ps));
            presult = &_data.at(std::get<std::string_view>(ps));
        } else {
            if (propex::kind(ps) == propex::PropertySegmentKind::IDENTIFIER) {
                std::string key(std::get<std::string_view>(ps));
                presult = &(presult->at(key));
            } else {
                auto index = std::get<size_t>(ps);
                presult = &(presult->at(index));
            }
        }
    }
    return *presult;
}

MsgID Msg::generate_msg_id() {
    static std::atomic<uint32_t> msg_id_counter(0); // 初始化计数器为0
    if (msg_id_counter.load() >= 0xFFFFFFF0) {
        msg_id_counter.store(0);
        return msg_id_counter.fetch_add(1);
    } else {
        return msg_id_counter.fetch_add(1);
    }
}

}; // namespace edgelink