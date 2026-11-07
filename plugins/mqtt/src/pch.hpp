#pragma once

#include <edgelink/pch.hpp>

// 去 Boost：async_mqtt 已用 NanoSDK（nng）替代
#include <nng/mqtt/mqtt_client.h>
#include <nng/nng.h>
#include <nng/supplemental/util/platform.h>
