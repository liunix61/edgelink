#pragma once

#include "edgelink/export.hpp"
#include "edgelink/common.hpp"
#include "edgelink/errors.hpp"
#include "edgelink/utils.hpp"
#include "edgelink/json.hpp"
#include "edgelink/settings.hpp"

#include "edgelink/flows/common.hpp"
#include "edgelink/flows/msg.hpp"
#include "edgelink/flows/abstractions.hpp"
#include "edgelink/flows/engine.hpp"


namespace edgelink {

struct BOOST_SYMBOL_VISIBLE IPlugin {
    virtual std::string_view name() const = 0;
    virtual const std::unordered_map<std::string, std::unique_ptr<INodeDescriptor>>& node_descriptors() const = 0;

    virtual ~IPlugin() = default;
};

#define EDGELINK_PLUGIN_DEFINE(class_)                                                                                 \
    extern "C" BOOST_SYMBOL_EXPORT class_ __edgelink_plugin_object;                                                    \
    class_ __edgelink_plugin_object

}; // namespace edgelink