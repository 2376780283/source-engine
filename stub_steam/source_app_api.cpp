#include "SourceApp/sourceapp_userstats.h"
#include "platform.h"

// 定义一个纯 C++ 链接的导出宏（不带 extern "C"）
#if defined(__GNUC__)
    #define SOURCEAPP_API __attribute__((visibility("default")))
#elif defined(_WIN32)
    #define SOURCEAPP_API __declspec(dllexport)
#else
    #define SOURCEAPP_API
#endif

SOURCEAPP_API CSourceAppApicontext* SourceAppApicontext = nullptr;
