/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
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

#define LOG_TAG "bt_stack_manager"

#include "stack_manager.h"

#include <hardware/bluetooth.h>

#include "btcore/include/module.h"
#include "btcore/include/osi_module.h"
#include "btif_api.h"
#include "btif_common.h"
#include "device/include/controller.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"
#include "osi/include/semaphore.h"
#include "osi/include/thread.h"

// Temp includes
#include "bt_utils.h"
#include "btif_config.h"
#include "btif_profile_queue.h"

static thread_t* management_thread;

// If initialized, any of the bluetooth API functions can be called.
// (e.g. turning logging on and off, enabling/disabling the stack, etc)
static bool stack_is_initialized;
// If running, the stack is fully up and able to bluetooth.
static bool stack_is_running;
#ifdef BOARD_HAVE_FMRADIO_BCM
static bool radio_is_running;
#endif

static void event_init_stack(void* context);
static void event_start_up_stack(void* context);
static void event_shut_down_stack(void* context);
static void event_clean_up_stack(void* context);

static void event_signal_stack_up(void* context);
static void event_signal_stack_down(void* context);

#ifdef BOARD_HAVE_FMRADIO_BCM
static void event_start_up_radio(void *context);
static void event_shut_down_radio(void *context);

static future_t *radio_future;
#endif

// Unvetted includes/imports, etc which should be removed or vetted in the
// future
static future_t* hack_future;
void btif_thread_post(thread_fn func, void* context);

// End unvetted section

// Interface functions

static void init_stack(void) {
  // This is a synchronous process. Post it to the thread though, so
  // state modification only happens there. Using the thread to perform
  // all stack operations ensures that the operations are done serially
  // and do not overlap.
  semaphore_t* semaphore = semaphore_new(0);
  thread_post(management_thread, event_init_stack, semaphore);
  semaphore_wait(semaphore);
  semaphore_free(semaphore);
}

static void start_up_stack_async(void) {
  thread_post(management_thread, event_start_up_stack, NULL);
}

static void shut_down_stack_async(void) {
  thread_post(management_thread, event_shut_down_stack, NULL);
}

static void clean_up_stack(void) {
  // This is a synchronous process. Post it to the thread though, so
  // state modification only happens there.
  semaphore_t* semaphore = semaphore_new(0);
  thread_post(management_thread, event_clean_up_stack, semaphore);
  semaphore_wait(semaphore);
  semaphore_free(semaphore);
}

static bool get_stack_is_running(void) { return stack_is_running; }

#ifdef BOARD_HAVE_FMRADIO_BCM
static bool start_up_radio(void) {
  radio_future = future_new();
  thread_post(management_thread, event_start_up_radio, NULL);
  future_await(radio_future);
  return radio_is_running;
}

static bool shut_down_radio(void) {
  radio_future = future_new();
  thread_post(management_thread, event_shut_down_radio, NULL);
  future_await(radio_future);
  return !radio_is_running;
}

static bool get_radio_is_running(void) {
  return radio_is_running;
}
#endif

// Internal functions

// Synchronous function to initialize the stack
static void event_init_stack(void* context) {
  semaphore_t* semaphore = (semaphore_t*)context;

  LOG_INFO(LOG_TAG, "%s is initializing the stack", __func__);

  if (stack_is_initialized) {
    LOG_INFO(LOG_TAG, "%s found the stack already in initialized state",
             __func__);
  } else {
    module_management_start();

    module_init(get_module(OSI_MODULE));
    module_init(get_module(BT_UTILS_MODULE));
    module_init(get_module(BTIF_CONFIG_MODULE));
    btif_init_bluetooth();

    // stack init is synchronous, so no waiting necessary here
    stack_is_initialized = true;

    stack_is_running = false;
#ifdef BOARD_HAVE_FMRADIO_BCM
    radio_is_running = false;
#endif
  }

  LOG_INFO(LOG_TAG, "%s finished", __func__);

  if (semaphore) semaphore_post(semaphore);
}

static void ensure_stack_is_initialized(void) {
  if (!stack_is_initialized) {
    LOG_WARN(LOG_TAG, "%s found the stack was uninitialized. Initializing now.",
             __func__);
    // No semaphore needed since we are calling it directly
    event_init_stack(NULL);
  }
}

// Synchronous function to start up the stack
static void event_start_up_stack(UNUSED_ATTR void* context) {
  if (stack_is_running) {
    LOG_INFO(LOG_TAG, "%s stack already brought up", __func__);
    return;
  }

  ensure_stack_is_initialized();

  LOG_INFO(LOG_TAG, "%s is bringing up the stack", __func__);
  future_t* local_hack_future = future_new();
  hack_future = local_hack_future;

#ifdef BOARD_HAVE_FMRADIO_BCM
  if (!radio_is_running) {
    // Include this for now to put btif config into a shutdown-able state
    module_start_up(get_module(BTIF_CONFIG_MODULE));
    bte_main_enable();
  } else {
    btif_transfer_context(btif_init_ok, 1, NULL, 0, NULL);
  }
#else
  // Include this for now to put btif config into a shutdown-able state
  module_start_up(get_module(BTIF_CONFIG_MODULE));
  bte_main_enable();
#endif

  if (future_await(local_hack_future) != FUTURE_SUCCESS) {
    LOG_ERROR(LOG_TAG, "%s failed to start up the stack", __func__);
    stack_is_running = true;  // So stack shutdown actually happens
    event_shut_down_stack(NULL);
    return;
  }

  stack_is_running = true;
  LOG_INFO(LOG_TAG, "%s finished", __func__);
  btif_thread_post(event_signal_stack_up, NULL);
}

// Synchronous function to shut down the stack
static void event_shut_down_stack(UNUSED_ATTR void* context) {
  if (!stack_is_running) {
    LOG_INFO(LOG_TAG, "%s stack is already brought down", __func__);
    return;
  }

  LOG_INFO(LOG_TAG, "%s is bringing down the stack", __func__);
  future_t* local_hack_future = future_new();
  hack_future = local_hack_future;
  stack_is_running = false;

#ifdef BOARD_HAVE_FMRADIO_BCM
  if (!radio_is_running) {
    btif_disable_bluetooth();
    module_shut_down(get_module(BTIF_CONFIG_MODULE));

    future_await(hack_future);
    module_shut_down(get_module(CONTROLLER_MODULE)); // Doesn't do any work, just puts it in a restartable state
  } else {
    btif_disable_bluetooth();
    future_await(hack_future);
  }
#else
  btif_disable_bluetooth();
  module_shut_down(get_module(BTIF_CONFIG_MODULE));

  future_await(local_hack_future);
  module_shut_down(get_module(CONTROLLER_MODULE));  // Doesn't do any work, just
                                                    // puts it in a restartable
                                                    // state
#endif

  hack_future = future_new();
  btif_thread_post(event_signal_stack_down, NULL);
  future_await(hack_future);
  LOG_INFO(LOG_TAG, "%s finished", __func__);
}

static void ensure_stack_is_not_running(void) {
  if (stack_is_running) {
    LOG_WARN(LOG_TAG,
             "%s found the stack was still running. Bringing it down now.",
             __func__);
    event_shut_down_stack(NULL);
  }
}

// Synchronous function to clean up the stack
static void event_clean_up_stack(void* context) {
  future_t* local_hack_future;

  if (!stack_is_initialized) {
    LOG_INFO(LOG_TAG, "%s found the stack already in a clean state", __func__);
    goto cleanup;
  }

  ensure_stack_is_not_running();

  LOG_INFO(LOG_TAG, "%s is cleaning up the stack", __func__);
  local_hack_future = future_new();
  hack_future = local_hack_future;
  stack_is_initialized = false;

  btif_cleanup_bluetooth();
  module_clean_up(get_module(BTIF_CONFIG_MODULE));
  module_clean_up(get_module(BT_UTILS_MODULE));
  module_clean_up(get_module(OSI_MODULE));
  module_management_stop();
  LOG_INFO(LOG_TAG, "%s finished", __func__);

cleanup:;
  semaphore_t* semaphore = (semaphore_t*)context;
  if (semaphore) semaphore_post(semaphore);
}

static void event_signal_stack_up(UNUSED_ATTR void* context) {
  // Notify BTIF connect queue that we've brought up the stack. It's
  // now time to dispatch all the pending profile connect requests.
  btif_queue_connect_next();
  HAL_CBACK(bt_hal_cbacks, adapter_state_changed_cb, BT_STATE_ON);
}

static void event_signal_stack_down(UNUSED_ATTR void* context) {
  HAL_CBACK(bt_hal_cbacks, adapter_state_changed_cb, BT_STATE_OFF);
  future_ready(stack_manager_get_hack_future(), FUTURE_SUCCESS);
}

#ifdef BOARD_HAVE_FMRADIO_BCM
static void event_start_up_radio(void *context) {
  if (radio_is_running) {
    APPL_TRACE_DEBUG("%s radio already brought up.", __func__);
    return;
  }

  ensure_stack_is_initialized();

  APPL_TRACE_DEBUG("%s is bringing up the radio.", __func__);
  radio_is_running = true;

  if (!stack_is_running) {
    hack_future = future_new();

    // Include this for now to put btif config into a shutdown-able state
    module_start_up(get_module(BTIF_CONFIG_MODULE));
    bte_main_enable();

    if (future_await(hack_future) != FUTURE_SUCCESS) {
      event_shut_down_radio(NULL);
      return;
    }
  }

  APPL_TRACE_DEBUG("%s finished", __func__);
  future_ready(radio_future, FUTURE_SUCCESS);
}

static void event_shut_down_radio(void *context) {
  if (!radio_is_running) {
    APPL_TRACE_DEBUG("%s radio is already brought down.", __func__);
    return;
  }

  APPL_TRACE_DEBUG("%s is bringing down the radio.", __func__);
  radio_is_running = false;

  if (!stack_is_running) {
    hack_future = future_new();
    btif_disable_bluetooth_evt();
    future_await(hack_future);

    module_shut_down(get_module(BTIF_CONFIG_MODULE));
    module_shut_down(get_module(CONTROLLER_MODULE)); // Doesn't do any work, just puts it in a restartable state
  }

  APPL_TRACE_DEBUG("%s finished.", __func__);
  future_ready(radio_future, FUTURE_SUCCESS);
}
#endif

static void ensure_manager_initialized(void) {
  if (management_thread) return;

  management_thread = thread_new("stack_manager");
  if (!management_thread) {
    LOG_ERROR(LOG_TAG, "%s unable to create stack management thread", __func__);
    return;
  }
}

static const stack_manager_t interface = {init_stack, start_up_stack_async,
                                          shut_down_stack_async, clean_up_stack,

#ifdef BOARD_HAVE_FMRADIO_BCM
                                          get_stack_is_running,
                                          start_up_radio,
                                          shut_down_radio,
                                          get_radio_is_running};
#else
                                          get_stack_is_running};

#endif

const stack_manager_t* stack_manager_get_interface() {
  ensure_manager_initialized();
  return &interface;
}

future_t* stack_manager_get_hack_future() { return hack_future; }
