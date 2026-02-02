#include "edgelink/edgelink.hpp"

#if __cplusplus >= 202002L
    // 支持 C++20 的代码
#else
    static_assert(false, "我日你妈");
#endif


RTTR_REGISTRATION {
    rttr::registration::class_<edgelink::INodeDescriptor>("edgelink::INodeDescriptor");
    rttr::registration::class_<edgelink::INodeProvider>("edgelink::IStandaloneNodeProvider");
    rttr::registration::class_<edgelink::IStandaloneNodeProvider>("edgelink::IStandaloneNodeProvider");
    rttr::registration::class_<edgelink::IFlowNodeProvider>("edgelink::IFlowNodeProvider");
}
