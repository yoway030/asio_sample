#include <Libinclude.h>

void spdlog_default_initialize()
{
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e | %^%L%$ | %t | %v | %s:%#");
    spdlog::set_level(spdlog::level::debug);
    
    LOG_DEBUG("spdlog default initialized");
}