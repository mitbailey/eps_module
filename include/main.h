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

#ifndef MAIN_PRIVATE
#ifndef MODULE_NAME
//#error "Define MODULE_NAME before including main.h"
#endif
#endif

#define eprintf(str, ...) \
    fprintf(stderr, "%s, %d: " str "\n", __func__, __LINE__, ##__VA_ARGS__); \
    fflush(stderr)

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
    ERR_INVALID_INPUT,
    ERR_REREGISTER,
    ERR_MAXLOGSIZE_NOT_SET,
    ERR_MAXLOGSIZE_EXCEEDED,
    ERR_CHDIR_FAIL,
    ERR_INDEX_OPEN,

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
int dlgr_LogData(char *moduleName, ssize_t size, void *data);

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
int dlgr_RetrieveData(char *moduleName, char *output, int numRequestedLogs);

/**
 * @brief Provides the memory size necessary to store some number of logs.
 * 
 * Use this prior to allocating memory for a pointer in which you want
 * retrieved logs to be stored. For instance, prior to calling dlgr_retrieveData(...)
 * to retrieve 10 logs, you would first malloc whatever size dlgr_queryMemorySize(..., 10)
 * returns.
 * 
 * @param moduleName The name of the calling module.
 * @param numRequestedLogs The number of logs that will be requested for retrieval.
 * @return ssize_t The size necessary to store a number of logs.
 */
ssize_t dlgr_QueryMemorySize(char *moduleName, int numRequestedLogs);

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
int dlgr_EditSettings(char *moduleName, int value, int setting);

#ifdef DATALOGGER_PRIVATE // aka datalogger.h

// File and directories cannot exceed these limits.
#define SIZE_FILE_HARDLIMIT 1048576 // 1MB
#define SIZE_DIR_HARDLIMIT 16777216 // 16MB

typedef struct DATALOGGER
{
    uint64_t logIndex;
    ssize_t moduleLogSize;
    int maxFileSize;
    int maxDirSize;
} datalogger_t;

/**
 * @brief Initializes the datalogger and its files.
 * 
 * If files such as index.inf exist, init will set dlgr's variables
 * to match. If they do not exist, init will create them.
 * Requests to log data must be composed of a data structure of the size 
 * passed here. Sizes smaller than max_size are allowable (datalogger
 * provides padding). This value, once set, can not be changed.
 * 
 * @param moduleName The calling module's name for which this datalogger is being initialized.
 * @param maxLogSize The maximum desired log size for this module's logs.
 * @return int Negative on failure (see: datalogger_extern.h's ERROR enum), 1 on success.
 */
int dlgr_init(char* moduleName, ssize_t maxLogSize);

/**
 * @brief A helper function for dlgr_retrieveData
 * 
 * @param output A char* to store the output.
 * @param numRequestedLogs The number of logs to be fetched.
 * @param moduleName The name of the calling module.
 * @param indexOffset Essentially, the number of files we've had to go through already.
 * @return int The number of logs added to output.
 */
int dlgr_retrieve(char *moduleName, char *output, int numRequestedLogs, int indexOffset);

/**
 * @brief WIP
 * 
 */
void dlgr_destroy();

#endif // DATALOGGER_PRIVATE
#endif // DATALOGGER