/**
 * @file eps.h
 * @author Mit Bailey (mitbailey99@gmail.com)
 * @brief Declarations of EPS command handling functions and structures.
 * @version 0.2
 * @date 2021-03-17
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef EPS_H
#define EPS_H

#include <eps_extern.h>
#include <eps_iface.h>
#include <stdbool.h>
#include <stdint.h>

#define EPS_CMD_TIMEOUT 5

#define EPS_CMD_PING 0
#define EPS_CMD_REBOOT 1
#define EPS_CMD_TLUP 2
#define EPS_CMD_SLUP 3

/**
 * @brief Command type for enqueuing.
 *
 * nCmds:       Number of commands.
 * cmds:        Array of commands where each is a single integer, i.e. EPS_CMD_PING.
 * nCmdArgs:    Array of the number of command arguments per command.
 *              i.e. nCmdArgs[i] is the number of arguments for command cmds[i].
 * cmdArgs:     A 2-dimensional array of command arguments.
 *              i.e. cmdArgs[i][k] is the k'th argument of the command cmds[i].
 * 
 */
typedef struct Command {
    int nCmds;
    int* cmds;
    int* nCmdArgs; 
    int** cmdArgs; 
} command_t;

/**
 * @brief Node object for the command queue.
 *
 * cmd: Points to where the command array begins.
 * next: Points to the next CmdNode in the queue.
 * retval: Points to the location of the return value or array of values.
 *
 */
typedef struct CmdNode {
    command_t* cmd;
    pthread_cond_t *wakeup;
    struct CmdNode* next;
    int* retval;
} cmdnode_t;

/**
 * @brief Queue which holds EPS commands for processing.
 *
 */
typedef struct CmdQ {
    cmdnode_t* head;
    cmdnode_t* tail;
    uint8_t size;
    const uint8_t maxSize; // maxSize = 255
} cmdq_t;

/**
 * @brief Adds a command to the queue.
 *
 * @return Pointer to a location which will hold the return value on execution.
 */
void* eps_cmdq_enqueue(uint8_t value[]);

/**
 * @brief Returns and removes the next command in the queue.
 *
 * @return note_t* The next command in the queue.
 */
cmdnode_t* eps_cmdq_dequeue();

/**
 * @brief Executes the next command in the queue.
 *
 * @return int 1 on success, -1 on dequeue error
 */
int eps_cmdq_execute();

/**
 * @brief Destroys / frees the entire command queue.
 *
 */
void eps_cmdq_destroy();

/**
 * @brief Allocates a space for the eventual return value in the return queue.
 *
 * Similar to enqueue.
 *
 * @return Returns a pointer to a retnode_t object.
 */
retnode_t* eps_retq_allocate();

/**
 * @brief Removes and frees the head node from the queue, setting a new head.
 *
 * This function detaches the head node from the queue and frees it.
 * It should be called whenever a command is executed (or removed for
 * whatever reason). It does not free() the node's pointer to the
 * return value in memory, void* retval, because that would prevent
 * whomever queued the command from accessing their return value. The
 * return value memory must be free()'d by the original caller.
 *
 * eps_req_remove's return value is typically only used by eps_retq_destroy().
 *
 * @return Returns a pointer to the void pointer to the command's return value.
 */
void* eps_retq_remove();

/**
 * @brief Destroys / frees the entire return queue.
 *
 */
void eps_retq_destroy();


#endif // EPS_H