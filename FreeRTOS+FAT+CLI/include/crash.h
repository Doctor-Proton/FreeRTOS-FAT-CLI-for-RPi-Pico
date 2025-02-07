/* crash.h
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
*/
#ifndef _CRASH_H_
#define _CRASH_H_
// Original from M0AGX (blog@m0agx.eu), "Preserving debugging breadcrumbs across
// reboots in Cortex-M,"
// https://m0agx.eu/2018/08/18/preserving-debugging-breadcrumbs-across-reboots-in-cortex-m/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"  // configMAX_TASK_NAME_LEN

//
#include "pico/stdlib.h"

/* The crash info section is at the beginning of the RAM,
 * that is not initialized by the linker to preserve
 * information across reboots.
 */

#define CRASH_INFO_SECTION_LENGTH 192

#define CY_R0_Pos \
    (0U) /**< The position of the R0  content in a fault structure */
#define CY_R1_Pos \
    (1U) /**< The position of the R1  content in a fault structure */
#define CY_R2_Pos \
    (2U) /**< The position of the R2  content in a fault structure */
#define CY_R3_Pos \
    (3U) /**< The position of the R3  content in a fault structure */
#define CY_R12_Pos \
    (4U) /**< The position of the R12 content in a fault structure */
#define CY_LR_Pos \
    (5U) /**< The position of the LR  content in a fault structure */
#define CY_PC_Pos \
    (6U) /**< The position of the PC  content in a fault structure */
#define CY_PSR_Pos \
    (7U) /**< The position of the PSR content in a fault structure */

typedef struct {
    uint32_t r0;  /**< R0 register content */
    uint32_t r1;  /**< R1 register content */
    uint32_t r2;  /**< R2 register content */
    uint32_t r3;  /**< R3 register content */
    uint32_t r12; /**< R12 register content */
    uint32_t lr;  /**< LR register content */
    uint32_t pc;  /**< PC register content */
    uint32_t psr; /**< PSR register content */
} __attribute__((packed)) cy_stc_fault_frame_t;

typedef enum {
    crash_magic_none = 0,
    crash_magic_bootloader_entry = 0xB000B000,
    crash_magic_hard_fault = 0xCAFEBABE,
    crash_magic_debug_mon = 0x01020304,
    crash_magic_reboot_requested = 0x0ABCDEF,
    crash_magic_stack_overflow = 0xBADBEEF,
    crash_magic_assert = 0xDEBDEBDE
} crash_magic_t;

typedef struct {
    char file[32];
    int line;
    char func[32];
    char pred[32];
} crash_assert_t;

typedef struct {
    uint32_t magic;
    time_t timestamp;
    union {
        cy_stc_fault_frame_t cy_faultFrame;
        crash_assert_t assert;
        char task_name[configMAX_TASK_NAME_LEN];
        char calling_func[64];
    };
    uint32_t xor_checksum;  // last to avoid including in calculation
} crash_info_t;

typedef struct {
    crash_info_t crash_info;
    uint8_t unused[CRASH_INFO_SECTION_LENGTH - sizeof(crash_info_t)];
} crash_info_ram_t;

_Static_assert(sizeof(crash_info_ram_t) == CRASH_INFO_SECTION_LENGTH,
               "Wrong size");

// Trick to find struct size at compile time:
// char (*__kaboom)[sizeof(crash_info_flash_t)] = 1;
// warning: initialization of 'char (*)[132]' from 'int' makes ...

#ifdef __cplusplus
extern "C" {
#endif

void crash_handler_init();
const crash_info_t *crash_handler_get_info(void);
volatile const crash_info_t *crash_handler_get_info_flash();

#define SYSTEM_RESET() system_reset_func(__FUNCTION__)
__attribute__((noreturn)) void system_reset_func(char const *const func);
#ifdef BOOTLOADER_BUILD
bool system_check_bootloader_request_flag(void);
#else
__attribute__((noreturn)) void system_request_bootloader_entry(void);
#endif

void capture_assert(const char *file, int line, const char *func,
                    const char *pred);
void capture_assert_case_not(const char *file, int line, const char *func,
                             int v);

int dump_crash_info(crash_info_t const *const pCrashInfo, int next,
                    char *const buf, size_t const buf_sz);

#ifdef __cplusplus
}
#endif

#endif
/* [] END OF FILE */
