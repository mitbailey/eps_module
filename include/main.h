/**
 * @file main.h
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief Includes all headers necessary for the core flight software, including ACS, and defines
 * ACS states (which are flight software states), error codes, and relevant error functions.
 * @version 0.1
 * @date 2020-03-19
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#ifndef MAIN_H
#define MAIN_H
#include <signal.h>
/**
 * @brief Describes ACS (system) states.
 * 
 */
typedef enum
{
    STATE_ACS_DETUMBLE, // Detumbling
    STATE_ACS_SUNPOINT, // Sunpointing
    STATE_ACS_NIGHT,    // Night
    STATE_ACS_READY,    // Do nothing
    STATE_XBAND_READY   // Ready to do X-Band things
} SH_ACS_MODES;

/**
 * @brief Describes possible system errors.
 * 
 */
typedef enum
{
    ERROR_MALLOC = -1,
    ERROR_HBRIDGE_INIT = -2,
    ERROR_MUX_INIT = -3,
    ERROR_CSS_INIT = -4,
    ERROR_MAG_INIT = -5,
    ERROR_FSS_INIT = -6,
    ERROR_FSS_CONFIG = -7
} SH_ERRORS;
/**
 * @brief Thread-local system status variable (similar to errno).
 * 
 */
extern __thread int sys_status;
// thread local error printing
void sherror(const char *);
/**
 * @brief Control variable for thread loops.
 * 
 */
extern volatile sig_atomic_t done;
/**
 * @brief System variable containing the current boot count of the system.
 * This variable is provided to all modules by main.
 */ 
extern int sys_boot_count;
#ifdef MAIN_PRIVATE
/**
 * @brief Name of the file where bootcount is stored on the file system.
 * 
 */
#define BOOTCOUNT_FNAME "bootcount_fname.txt"

/**
 * @brief Function that returns the current bootcount of the system.
 * Returns current boot count, and increases by 1 and stores it in nvmem.
 * Expected to be invoked only by _main()
 * 
 * @return int Current boot count (C-style)
 */
int bootCount(void);

// interrupt handler for SIGINT
void catch_sigint(int);
#endif // MAIN_PRIVATE
#endif // MAIN_H

/***************************
 * 
 * DATALOGGER
 * 
 * *************************/

#ifndef DATALOGGER // aka datalogger_extern.h
#define DATALOGGER

#include <unistd.h>

enum ERROR
{
    ERR_INIT = -20,
    ERR_SETTINGS_OPEN,
    ERR_SETTINGS_ACCESS,
    ERR_SETTINGS_SET,
    ERR_DATA_OPEN,
    ERR_DATA_REMOVE,
    ERR_DATA_READ,
    ERR_DEFAULT_CASE,
    ERR_FILE_DNE,
    ERR_MALLOC,
    ERR_LOG_SIZE,
    ERR_MODU_OPEN,
    ERR_READ_NUM,
    
    ERR_MISC
};

enum SETTING
{
    MAX_FILE_SIZE = 0,
    MAX_DIR_SIZE
};

/**
 * @brief Logs passed data to a file.
 * 
 * Logs the data passed to it as binary in a .dat file, which is
 * located in /log/<MODULE>/. It follows the settings set in
 * /log/<MODULE>/settings.cfg, which means it will
 * create a new .dat file when the file size exceeds maxFileSize
 * (line 2) and will begin deleting old .dat files when the 
 * directory size exceeds maxDirSize (line 3). It also stores
 * the .dat file's index for naming (ie: 42.dat). Encapsulates
 * each section of written data between FBEGIN and FEND.
 * 
 * @param size The size of the data to be logged.
 * @param dataIn The data to be logged.
 * @param moduleName The calling module's name, a unique directory.
 * @return int Negative on failure (see: datalogger_extern.h's ERROR enum), 1 on success.
 */
int dlgr_logData(ssize_t size, void *data, char *moduleName);

/**
 * @brief Retrieves logged data.
 * 
 * Pulls information from binary .dat files and places it into output.
 * Use dlgr_getMemorySize() or dlgr_queryMemorySize() to determine the amount
 * of memory that needs to be allocated to store the number of logs that are
 * being requested.
 * 
 * @param output The location in memory where the data will be stored.
 * @param numRequestedLogs How many logs would you like?
 * @param moduleName The name of the caller module.
 * @return int Negative on error (see: datalogger_extern.h's ERROR enum), 1 on success.
 */
int dlgr_retrieveData(char* output, int numRequestedLogs, char *moduleName);

/**
 * @brief Provides the memory size necessary to store some number of logs.
 * 
 * Use this if you know the size of a single log.
 * 
 * @param logSize The size of a single log structure.
 * @param numRequestedLogs The number of logs that will be requested.
 * @return ssize_t The size required to store n-logs.
 */
ssize_t dlgr_queryMemorySize (ssize_t logSize, int numRequestedLogs);

/**
 * @brief Provides the memory size necessary to store some number of logs.
 * 
 * Use this if you do not know the size of a log. This function will try to
 * figure it out itself.
 * 
 * @param numRequestedLogs The number of logs that will be requested.
 * @param moduleName The name of the calling module.
 * @return ssize_t Size of memory needed.
 */
ssize_t dlgr_getMemorySize (int numRequestedLogs, char* moduleName);

/**
 * @brief Used to edit settings.cfg.
 * 
 * This function sets a setting to a value within a module's own datalogger directory.
 * 
 * @param value The value to be written.
 * @param setting The setting to edit (see: datalogger_extern.h's SETTING enum).
 * @param directory The calling module's name, a unique directory.
 * @return int Negative on failure (see: datalogger_extern.h's ERROR enum), 1 on success.
 */
int dlgr_editSettings(int value, int setting, char *moduleName);

#ifdef DATALOGGER_PRIVATE // aka datalogger.h

// File and directories cannot exceed these limits.
#define SIZE_FILE_HARDLIMIT 1048576 // 1MB
#define SIZE_DIR_HARDLIMIT 16777216  // 16MB

typedef struct SETTINGS
{
    int maxFileSize;
    int maxDirSize;
} settings_t;

/**
 * @brief Initializes the datalogger and its files.
 * 
 * If files such as index.inf exist, init will set dlgr's variables
 * to match. If they do not exist, init will create them.
 * 
 * @param moduleName Temp. variable, assumption is this is a module name such as "eps".
 * @return int Negative on failure (see: datalogger_extern.h's ERROR enum), 1 on success.
 */
int dlgr_init();

/**
 * @brief A helper function for dlgr_retrieveData
 * 
 * @param output A char* to store the output.
 * @param numRequestedLogs The number of logs to be fetched.
 * @param moduleName The name of the calling module.
 * @param indexOffset Essentially, the number of files we've had to go through already.
 * @return int The number of logs added to output.
 */
int dlgr_retrieve(char* output, int numRequestedLogs, char* moduleName, int indexOffset);

/**
 * @brief Returns the index of the token within buffer.
 * 
 * Searches for the first instance of token within buffer and returns the index of
 * the first character.
 * 
 * @param buffer The character array to be searched.
 * @param bufferSize The size of the buffer.
 * @param token The token to search for within buffer.
 * @param tokenSize The size of the token.
 * @return int Index of the token within buffer on success, ERR_MISC on failure.
 */
int dlgr_indexOf(char *buffer, ssize_t bufferSize, char *token, ssize_t tokenSize);

/**
 * @brief WIP
 * 
 */
void dlgr_destroy();

#endif // DATALOGGER_PRIVATE
#endif // DATALOGGER