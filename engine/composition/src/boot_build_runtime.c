/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Optional build supervision at the node composition boundary. */

#include "config/boot_internal.h"
#include "services/build_fabric_runtime.h"
#include "util/log_macros.h"

void boot_register_build_runtime(struct boot_svc_ctx *svc)
{
    if (!svc || !svc->app_ctx) {
        LOG_ERROR("build_fabric", "registration: missing service/application context");
        return;
    }
    if (svc->app_ctx->runtime_profile == ZCL_RUNTIME_ZCLASSIC_ONLY)
        return;
    struct zcl_result result = build_fabric_runtime_register(
        svc->app_ctx->build_worker, svc->datadir);
    if (!result.ok)
        LOG_WARN("build_fabric", "%s", result.message);
}
