#include "threading.h"
#include <stdio.h>
#include <stdlib.h>

void t_init()
{
    // Initialize all contexts to INVALID state
    for (int i = 0; i < NUM_CTX; i++) {
        contexts[i].state = INVALID;
    }
    current_context_idx = NUM_CTX; // Start with an invalid index
}

int32_t t_create(fptr foo, int32_t arg1, int32_t arg2)
{
    for (volatile int i = 0; i < NUM_CTX; i++) {
        if (contexts[i].state == INVALID) {
            // Get new context and allocate stack
            if (getcontext(&contexts[i].context) == -1) {
                return 1;
            }

            contexts[i].context.uc_stack.ss_sp = malloc(STK_SZ);
            if (!contexts[i].context.uc_stack.ss_sp) {
                return 1;
            }

            contexts[i].context.uc_stack.ss_size = STK_SZ;
            contexts[i].context.uc_link = NULL; // Return to main if finished

            makecontext(&contexts[i].context, (ctx_ptr)foo, 2, arg1, arg2);

            contexts[i].state = VALID;
            return 0; // Success
        }
    }
    return 1; // No available context
}

int32_t t_yield()
{
    int prev_idx = current_context_idx;
    int valid_count = 0;

    // Log current states of all contexts
    for (int i = 0; i < NUM_CTX; i++) {
        if (contexts[i].state == VALID) {
            valid_count++;
        }
    }

    if (valid_count == 0) {
        return -1; // No valid contexts
    }

    // Search for the next VALID context
    do {
        current_context_idx = (uint8_t)(current_context_idx + 1) % NUM_CTX;
        if (contexts[current_context_idx].state == VALID) {
            break; // Found a valid context
        }
    } while (current_context_idx != prev_idx);

    // Swap to the next valid context
    if (current_context_idx != prev_idx) {
        swapcontext(&contexts[prev_idx].context, &contexts[current_context_idx].context);
    }

    return valid_count;  // Return number of valid contexts remaining
}

void t_finish()
{
    // Mark the current context as done
    contexts[current_context_idx].state = DONE;

    // Free the stack space allocated for this context
    if (contexts[current_context_idx].context.uc_stack.ss_sp != NULL) {
        free(contexts[current_context_idx].context.uc_stack.ss_sp);
        contexts[current_context_idx].context.uc_stack.ss_sp = NULL; // Ensure it's not freed again
    }

    t_yield(); 
}
