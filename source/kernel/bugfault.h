#ifndef _KERNEL_BUGFAULT_H
#define _KERNEL_BUGFAULT_H 1

#include "../common/common.h"

#include "CPU/ISR/ISR.h"

/**
 * Trigger a kernel exception.
 *
 * @param message The panic message.
 */
#define THROW(message) triggerError(message, __FILE__, __LINE__);

/**
 * @brief Weak assertion
 *
 * @note If the condition is true, then an error is invoked and returns
 *       (This should be used within functions that are called)
 *
 * @param condition The condition to evaluate.
 * @param message   Optional message to describe the assertion.
 */
#define BUG_CHECK(condition, message)                                     \
    if ((condition)) {                                                    \
        triggerAssert(__FILE__, __func__, __LINE__, #condition, message); \
        return;                                                           \
    }

/**
 * @brief Fatal assertion
 *
 * @note If the condition is not met (false), then an error is invoked
 *       terminating the program (this does not return)
 *
 * @param condition The condition to evaluate.
 * @param message   Optional message to describe the assertion.
 */
#define BUG_FATAL(condition, message)                                     \
    if ((condition)) {                                                   \
        ASM VOLATILE ("cli");                                             \
        triggerAssert(__FILE__, __func__, __LINE__, #condition, message); \
        ASM VOLATILE ("hlt");                                             \
    }


/**
 * @brief Triggers a KERNEL PANIC!
 *
 * This function is called when a critical system error occurs, such as a device driver,
 * an bad operation, or an buggy kernel section. It halts the system to prevent further
 * damage and provides an stacktrace about the error.
 *
 * @param reason A description of the error or reason for the panic.
 * @param interrupt The interrupt number where the error occurred.
 * @param segment The memory segment associated with the error.
 * @param registers Pointer to the CPU registers at the time of the error (optional).
 */
void triggerPanic(const char *reason, uint32_t interrupt, uint32_t segment, registers_t *registers);


/**
 * @brief Trigger a short kernel exception.
 *
 * @param message The exception description message.
 * @param file The file where the exception occurred.
 * @param line The line number where the exception occurred.
 */
void triggerError(const char *message, const char *file, uint32_t line);


/**
 * @brief Trigger a kernel assertion failure.
 *
 * @param file The name of the source file where the assertion occurred.
 * @param func The name of the function where the assertion occurred.
 * @param line The line number where the assertion occurred.
 * @param condition The failed condition.
 * @param message Description of what happened.
 */
void triggerAssert(const char *file, const char *func, uint32_t line, const char *condition, const char *message);


#endif /* _KERNEL_BUGFAULT_H */
