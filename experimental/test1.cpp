/* test.c */
#include <iostream>
#include <memory>
#include <variant>
#include "duktape.h"
#include "../include/nlohmann/json.hpp"
#include "duktape-cpp/DuktapeCpp.h"

#include <memory>
#include <iostream>

using DukValue = nlohmann::json;

/*
    null,             ///< null value
    object,           ///< object (unordered set of name/value pairs)
    array,            ///< array (ordered collection of values)
    string,           ///< string value
    boolean,          ///< boolean value
    number_integer,   ///< number value (signed integer)
    number_unsigned,  ///< number value (unsigned integer)
    number_float,     ///< number value (floating-point)
    binary,           ///< binary array (ordered collection of bytes)
    discarded         ///< discarded by the parser callback function
*/

template <> struct duk::Type<DukValue> {

    static void push(duk::Context& d, DukValue const& value) {

        switch (value.type()) {

        case DukValue::value_t::null: {
            duk_push_null(d);
        } break;

        case DukValue::value_t::array: {
            duk_push_array(d);
            for (int i = 0; i < value.size(); ++i) {
                duk::Type<DukValue>::push(d, value[i]);
                duk_put_prop_index(d, -2, i);
            }
            std::cerr << "数组搞起 push" << std::endl;
        } break;

        case DukValue::value_t::boolean: {
            duk::Type<bool>::push(d, value);
        } break;

        case DukValue::value_t::number_integer: {
            duk::Type<int>::push(d, value);
        } break;

        case DukValue::value_t::number_unsigned: {
            duk::Type<unsigned int>::push(d, value);
        } break;

        case DukValue::value_t::number_float: {
            duk::Type<double>::push(d, value);
        } break;

        case DukValue::value_t::string: {
            duk::Type<std::string>::push(d, value);
        } break;

        default: {
            //
            std::cerr << value.dump() << std::endl;
        }

        } // switch
    }

    static void get(duk::Context& d, DukValue& value, int index) {

        std::cerr << "nmother : " << duk_get_type(d, index) << std::endl;
        switch (duk_get_type(d, index)) {

        case DUK_TYPE_NONE: {
            value = nullptr;
        } break;

        case DUK_TYPE_UNDEFINED: {
            value = nullptr;
        } break;

        case DUK_TYPE_NULL: {
            value = nullptr;
        } break;

        case DUK_TYPE_BOOLEAN: {
            bool boolean;
            duk::Type<bool>::get(d, boolean, index);
            value = boolean;
        } break;

        case DUK_TYPE_NUMBER: {
            double number;
            duk::Type<double>::get(d, number, index);
            value = number;
        } break;

        case DUK_TYPE_STRING: {
            std::string str_value;
            duk::Type<std::string>::get(d, str_value, index);
            std::cerr << str_value << std::endl;
            value = nlohmann::json(str_value);
            std::cerr << "这里不是设置字符串了么" << std::endl;
        } break;

        case DUK_TYPE_OBJECT: {
        } break;

        case DUK_TYPE_BUFFER: {
        } break;

        case DUK_TYPE_POINTER: {
        } break;

        default: {
            break;
        }

        } // switch
        std::cerr << "---------------" << value.dump() << std::endl;
    }

    static constexpr bool isPrimitive() { return false; };
};

namespace SpaceInvaders {

class Spaceship {
  public:
    explicit Spaceship(int pos) : _pos(pos) {}

    void moveLeft() { _pos -= 1; }
    void moveRight() { _pos += 1; }

    int pos() const { return _pos; }

    /**
     * You can define `inspect` method or specialize `duk::Inspect` for your class
     */
    template <class Inspector> static void inspect(Inspector& i) {
        i.construct(&std::make_shared<Spaceship, int>);
        i.method("moveRight", &Spaceship::moveRight);
        i.method("moveLeft", &Spaceship::moveLeft);
        i.property("pos", &Spaceship::pos);
    }

  private:
    int _pos;
};

} // namespace SpaceInvaders

struct Dog {
    explicit Dog(const std::string& name) : _name(name) {
        std::cerr << "<<<<<"
                  << "狗调用构造函数了"
                  << "<<<" << std::endl;
    }

    const std::string& name() const { return _name; }

    std::string _name;

    const std::string get(const std::string& prop) const {
        std::cerr << "<<<<<" << _name << "<<<" << std::endl;
        return _name;
    }

    template <class Inspector> static void inspect(Inspector& i) {
        i.construct(&std::make_shared<Dog, const std::string&>);
        i.property("name", &Dog::name);
        i.method("get", &Dog::get);
    }
};

DUK_CPP_DEF_CLASS_NAME(SpaceInvaders::Spaceship);

DUK_CPP_DEF_CLASS_NAME(Dog);

int main(int argc, char** argv) {
    try {
        /**
         * Create context
         */
        duk::Context ctx;

        /**
         * Register class.
         * Make sure the following requirements are met:
         * - Either `inspect` method or `Inspect` template specialization must be defined
         * - Class name must be defined (via DUK_CPP_DEF_CLASS_NAME macro)
         */
        ctx.registerClass<SpaceInvaders::Spaceship>();
        ctx.registerClass<Dog>();

        /**
         * Create spaceship in js
         */
        ctx.evalStringNoRes("var spaceship = new SpaceInvaders.Spaceship(5)");

        auto msg = nlohmann::json::object();
        msg["name"] = "Bear";

        auto dv = DukValue();
        dv = "abcd";
        ctx.addGlobal("dv", dv);
        ctx.evalStringNoRes(" dv = 'Change!!!!!!' ");
        std::cerr << "dv dump: " << dv.dump() << std::endl;

        auto ex1 = nlohmann::json::parse(R"(
            {
                "name": "Bear",
                "happy": true
            }
        )");
        ctx.addGlobal("j", ex1.dump());

        std::string json_text;
        auto js = R"(
            var t = JSON.parse(j);
            t.age = 7;
            JSON.stringify(t);
        )";
        ctx.evalString(json_text, js);
        std::cerr << "json_text=" << json_text << std::endl;

        /**
         * Get pointer to spaceship
         */
        std::shared_ptr<SpaceInvaders::Spaceship> spaceship;
        ctx.getGlobal("spaceship", spaceship);
        std::cerr << "pos=" << spaceship->pos() << std::endl;

        ctx.evalStringNoRes("spaceship.moveRight()");
        assert(spaceship->pos() == 6);

        spaceship->moveLeft();

        /**
         * Evaluate script and get result
         */
        int spaceshipPos = -1;
        ctx.evalString(spaceshipPos, "spaceship.pos");
        assert(spaceshipPos == 5);
    } catch (duk::ScriptEvaluationExcepton& e) {
        std::cout << e.what() << std::endl;
    }
}
