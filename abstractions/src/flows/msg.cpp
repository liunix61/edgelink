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
    if (prop_segs.empty()) {
        // 修复：空属性表达式（如 ""）原会 null 解引用崩溃
        throw InvalidDataException("Empty property expression");
    }
    const JsonValue* presult = nullptr;
    for (size_t i = 0; i < prop_segs.size(); i++) {
        const auto& ps = prop_segs[i];
        if (i == 0) {
            if (propex::kind(ps) != propex::PropertySegmentKind::IDENTIFIER) {
                throw InvalidDataException("Bad propex");
            }
            presult = &_data.at(std::string(std::get<std::string_view>(ps)));
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
    if (presult == nullptr) {
        // 防御：解析成功但未产生结果
        throw InvalidDataException("Property expression produced no result");
    }
    return *presult;
}

MsgID Msg::generate_msg_id() {
    // 修复：原实现 load()>=0xFFFFFFF0 后 store(0) 有竞态（多线程并发），改用纯 fetch_add 原子回绕
    static std::atomic<uint32_t> msg_id_counter(0);
    return msg_id_counter.fetch_add(1);
}

}; // namespace edgelink