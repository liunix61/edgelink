#pragma once

#if !defined(EDGELINK_EXPORT)

#    if defined(_WIN32)
#        define EDGELINK_EXPORT __declspec(dllexport)
#    else // defined(_WIN32)
#        define EDGELINK_EXPORT __attribute__((visibility("default")))
#    endif // defined(_WIN32)

#endif // !defined(EDGELINK_EXPORT)
