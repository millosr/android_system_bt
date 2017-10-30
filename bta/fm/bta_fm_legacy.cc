/******************************************************************************
 *
 *  Copyright (C) 2003-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "bt_target.h"
#include "btm_api.h"
#include "btcore/include/module.h"
#include "device/include/controller.h"

extern "C" void LogMsg_0(uint32_t trace_set_mask, const char *fmt_str) {
    LogMsg(trace_set_mask, fmt_str);
}

extern "C" void LogMsg_1(uint32_t trace_set_mask, const char *fmt_str, uint32_t p1) {

    LogMsg(trace_set_mask, fmt_str, p1);
}

extern "C" void LogMsg_2(uint32_t trace_set_mask, const char *fmt_str, uint32_t p1, uint32_t p2) {
    LogMsg(trace_set_mask, fmt_str, p1, p2);
}

extern "C" void LogMsg_3(uint32_t trace_set_mask, const char *fmt_str, uint32_t p1, uint32_t p2,
        uint32_t p3) {
    LogMsg(trace_set_mask, fmt_str, p1, p2, p3);
}

extern "C" void LogMsg_4(uint32_t trace_set_mask, const char *fmt_str, uint32_t p1, uint32_t p2,
        uint32_t p3, uint32_t p4) {
    LogMsg(trace_set_mask, fmt_str, p1, p2, p3, p4);
}

extern "C" void LogMsg_5(uint32_t trace_set_mask, const char *fmt_str, uint32_t p1, uint32_t p2,
        uint32_t p3, uint32_t p4, uint32_t p5) {
    LogMsg(trace_set_mask, fmt_str, p1, p2, p3, p4, p5);
}

extern "C" void LogMsg_6(uint32_t trace_set_mask, const char *fmt_str, uint32_t p1, uint32_t p2,
        uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6) {
    LogMsg(trace_set_mask, fmt_str, p1, p2, p3, p4, p5, p6);
}

extern "C" void GKI_sched_lock(void) {

}

extern "C" void GKI_sched_unlock(void) {

}

extern "C" tBTM_STATUS BTM_ReadLocalVersion (bt_version_t *p_vers) {
    if (module_start_up(get_module(CONTROLLER_MODULE))) {
        const controller_t *controller = controller_get_interface();

        *p_vers = *(controller->get_bt_version());
        return BTM_SUCCESS;
    }

    return BTM_ERR_PROCESSING;
}
