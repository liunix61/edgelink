#include <boost/json.hpp>
#include <iostream>
#include <memory>

int main() {
    auto obj = std::make_shared<boost::json::object>();
    obj->emplace("test", 3.14);

    // 打印整个 JSON 对象
    std::cout << "BEFORE: " << boost::json::serialize(*obj) << std::endl;

    // 检查属性是否存在，如果存在，则更新其值，如果不存在，则添加属性
    auto it = obj->find("test");
    if (it != obj->end()) {
        it->value() = 9999.33;
    } else {
        obj->emplace("test", 9999.33);
    }

    std::cout << "AFTER: " << boost::json::serialize(*obj) << std::endl;

    return 0;
}






