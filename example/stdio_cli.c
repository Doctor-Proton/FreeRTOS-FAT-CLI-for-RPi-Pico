/* stdio_cli.c
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
// FreeRTOS
#include "FreeRTOS.h"
//
#include "task.h"
// Pico
#include "pico/stdlib.h"
#include "pico/util/queue.h"
//
#include "hardware/irq.h"
#include "hardware/rtc.h"
#include "pico/error.h"
#include "pico/multicore.h"
#include "pico/stdio.h"
#include "pico/util/datetime.h"
//
#include "CLI-commands.h"
#include "File-related-CLI-commands.h"
#include "FreeRTOS_CLI.h"
#include "crash.h"
#include "filesystem_test_suite.h"
#include "stdio_cli.h"
#include "sd_card.h"

//#define TRACE_PRINTF(fmt, args...)
#define TRACE_PRINTF printf  // task_printf

// stdioTask - the function which handles input
static void stdioTask(void *arg) {
    (void)arg;

    size_t cInputIndex = 0;
    static char cOutputString[cmdMAX_OUTPUT_SIZE] = {0};
    static char cInputString[cmdMAX_INPUT_SIZE] = {0};
    BaseType_t xMoreDataToFollow = 0;
    bool in_overflow = false;

    printf("\033[2J\033[H");  // Clear Screen
    // Check fault capture from RAM:
    crash_info_t const *const pCrashInfo = crash_handler_get_info();
    if (pCrashInfo) {
        printf("*** Fault Capture Analysis (RAM): ***\n");
        int n = 0;
        do {
            char buf[256] = {0};
            n = dump_crash_info(pCrashInfo, n, buf, sizeof(buf));
            if (buf[0]) printf("\t%s", buf);
        } while (n != 0);
    }
    if (!rtc_running()) printf("RTC is not running.\n");
    datetime_t t = {0, 0, 0, 0, 0, 0, 0};
    rtc_get_datetime(&t);
    char datetime_buf[256] = {0};
    datetime_to_str(datetime_buf, sizeof(datetime_buf), &t);
    printf("%s\n", datetime_buf);
    printf("FreeRTOS+CLI> ");
    stdio_flush();

    for (;;) {
        int cRxedChar = getchar_timeout_us(0);
        /* Get the character from terminal */
        if (PICO_ERROR_TIMEOUT == cRxedChar) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        if (!isprint(cRxedChar) && !isspace(cRxedChar) && '\r' != cRxedChar &&
            '\b' != cRxedChar && cRxedChar != (char)127)
            continue;
        printf("%c", cRxedChar);  // echo
        stdio_flush();

        /* Newline characters are taken as the end of the command
         string. */
        if (cRxedChar == '\r') {
            in_overflow = false;

            TickType_t xStart = 0;
            /* Just to space the output from the input. */
            printf("%c", '\n');
            stdio_flush();

            if (!strnlen(cInputString,
                         sizeof cInputString)) {  // Empty input
                printf("%s", pcEndOfOutputMessage);
                continue;
            }
            const char timestr[] = "time ";
            if (0 == strncmp(cInputString, timestr, 5)) {
                xStart = xTaskGetTickCount();
                char tmp[cmdMAX_INPUT_SIZE] = {0};
                strlcpy(tmp, cInputString + 5, sizeof tmp);
                strlcpy(cInputString, tmp, cmdMAX_INPUT_SIZE);
            }
            /* Process the input string received prior to the
             newline. */
            do {
                /* Pass the string to FreeRTOS+CLI. */
                cOutputString[0] = 0x00;
                xMoreDataToFollow = FreeRTOS_CLIProcessCommand(
                    cInputString, cOutputString, cmdMAX_OUTPUT_SIZE);

                /* Send the output generated by the command's
                 implementation. */
                printf("%s", cOutputString);

                /* Until the command does not generate any more output.
                 */
            } while (xMoreDataToFollow);

            if (xStart) {
                printf("Time: %lu s\n",
                       (unsigned long)(xTaskGetTickCount() - xStart) /
                           configTICK_RATE_HZ);
            }
            /* All the strings generated by the command processing
             have been sent.  Clear the input string ready to receive
             the next command.  */
            cInputIndex = 0;
            memset(cInputString, 0x00, cmdMAX_INPUT_SIZE);

            /* Transmit a spacer to make the console easier to
             read. */
            printf("%s", pcEndOfOutputMessage);
            fflush(stdout);

        } else {  // Not newline

            if (in_overflow) continue;

            if ((cRxedChar == '\b') || (cRxedChar == cmdASCII_DEL)) {
                /* Backspace was pressed.  Erase the last character
                 in the string - if any. */
                if (cInputIndex > 0) {
                    cInputIndex--;
                    cInputString[(int)cInputIndex] = '\0';
                }
            } else {
                /* A character was entered.  Add it to the string
                 entered so far.  When a \n is entered the complete
                 string will be passed to the command interpreter. */
                if (cInputIndex < cmdMAX_INPUT_SIZE - 1) {
                    cInputString[(int)cInputIndex] = cRxedChar;
                    cInputIndex++;
                } else {
                    printf("\a[Input overflow!]\n");
                    fflush(stdout);
                    memset(cInputString, 0, sizeof(cInputString));
                    cInputIndex = 0;
                    in_overflow = true;
                }
            }
        }
    }
}

/* Start UART operation. */
void CLI_Start() {
    vRegisterCLICommands();
    vRegisterMyCLICommands();
    register_fs_tests();
    vRegisterFileSystemCLICommands();

    extern const CLI_Command_Definition_t xDataLogDemo;
    FreeRTOS_CLIRegisterCommand(&xDataLogDemo);

    static StackType_t xStack[1024];
    static StaticTask_t xTaskBuffer;
    TaskHandle_t th = xTaskCreateStatic(
        stdioTask, "stdio Task", sizeof xStack / sizeof xStack[0], 0,
        configMAX_PRIORITIES - 2, /* Priority at which the task is created. */
        xStack, &xTaskBuffer);
    configASSERT(th);
}

/* [] END OF FILE */
