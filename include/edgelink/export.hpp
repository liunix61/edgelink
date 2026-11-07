#pragma once

#if !defined(EDGELINK_EXPORT)

#    if defined(_WIN32)
#        define EDGELINK_EXPORT __declspec(dllexport)
#    else // defined(_WIN32)
#        define EDGELINK_EXPORT __attribute__((visibility("default")))
#    endif // defined(_WIN32)

#endif // !defined(EDGELINK_EXPORT)

// 去 Boost：BOOST_SYMBOL_VISIBLE / BOOST_SYMBOL_EXPORT 原由 boost 头提供，此处本地化为 EDGELINK_SYMBOL_*
#if !defined(EDGELINK_SYMBOL_VISIBLE)
#    if defined(_WIN32)
#        define EDGELINK_SYMBOL_VISIBLE __declspec(dllexport)
#    else
#        define EDGELINK_SYMBOL_VISIBLE __attribute__((visibility("default")))
#    endif
#endif

#if !defined(EDGELINK_SYMBOL_EXPORT)
#    if defined(_WIN32)
#        define EDGELINK_SYMBOL_EXPORT __declspec(dllexport)
#    else
#        define EDGELINK_SYMBOL_EXPORT __attribute__((visibility("default")))
#    endif
#endif
