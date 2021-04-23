/*
* Copyright (c) 2015-2020 Cadence Design Systems Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef _HOST_MAIN_H
#define _HOST_MAIN_H

#include <stdint.h>

#define HOST2DSP_MAJOR_VERSION (0)
#define HOST2DSP_MINOR_VERSION (1)

/* Max string length for each argument*/
#define MAX 1024

#define SHELL_BUFFER_SIZE (64U)

/* Max string length for a command line number in user mode*/
#define USERINPUT_MAX_SIZE 1000

/*Maximum number of allowed arguments along with command*/
#define MAX_ARGC_COUNT 10

#define PRINT_LIMITED_BUF_SIZE 150
/* Data bytes to send for codec initialization for file playback. */
#define FILE_PLAYBACK_INITIAL_READ_SIZE (16 * 1024)

/*Command index in argv*/
#define COMMAND_INDEX 1

/*! @brief Defines enum for HOST side commands */
typedef enum {
	HOST_CMD_HELP = 1,
    HOST_CMD_VERSION,
    HOST_CMD_GAIN,
    HOST_CMD_FILE,
    HOST_CMD_EXIT,
    HOST_CMD_LIST,
    HOST_CMD_STOP

#if 0    
    HOST_CMD_AAC     		,
    HOST_CMD_MP3        	,
    HOST_CMD_OPUSDEC     	,
    HOST_CMD_OPUSENC     	,
    HOST_CMD_VORBIS      	,
    HOST_CMD_SRC    		,
    HOST_CMD_RECORD_DMIC 	,
    #endif
} host_command_t;

/* File function return code (FRESULT) */

typedef enum {
	FR_OK = 0,				/* (0) Succeeded */
	FR_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
	FR_INT_ERR,				/* (2) Assertion failed */
	FR_NOT_READY,			/* (3) The physical drive cannot work */
	FR_NO_FILE,				/* (4) Could not find the file */
	FR_NO_PATH,				/* (5) Could not find the path */
	FR_INVALID_NAME,		/* (6) The path name format is invalid */
	FR_DENIED,				/* (7) Access denied due to prohibited access or directory full */
	FR_EXIST,				/* (8) Access denied due to prohibited access */
	FR_INVALID_OBJECT,		/* (9) The file/directory object is invalid */
	FR_WRITE_PROTECTED,		/* (10) The physical drive is write protected */
	FR_INVALID_DRIVE,		/* (11) The logical drive number is invalid */
	FR_NOT_ENABLED,			/* (12) The volume has no work area */
	FR_NO_FILESYSTEM,		/* (13) There is no valid FAT volume */
	FR_MKFS_ABORTED,		/* (14) The f_mkfs() aborted due to any problem */
	FR_TIMEOUT,				/* (15) Could not get a grant to access the volume within defined period */
	FR_LOCKED,				/* (16) The operation is rejected according to the file sharing policy */
	FR_NOT_ENOUGH_CORE,		/* (17) LFN working buffer could not be allocated */
	FR_TOO_MANY_OPEN_FILES,	/* (18) Number of open files > FF_FS_LOCK */
	FR_INVALID_PARAMETER	/* (19) Given parameter is invalid */
} FRESULT;
/*! @brief Defines format command for parsed data from user_input */
typedef struct formatCommands
{
	host_command_t cmd;
	char arg1[MAX];
	char arg2[MAX];
	char file_path[USERINPUT_MAX_SIZE];
	int  argcount;
	int  file_type;
}formatCommand;

/*! @brief Defines format command for parsed data from user_input */
typedef struct callBacks
{
	int success;
	int exit;
	int stop;
}callBack;

int standalone_exit = 0;
/*!
 * @brief Passing argc/argv into thread.
 */
typedef struct hostArgs
{
    int argc;
    char argv[MAX_ARGC_COUNT][USERINPUT_MAX_SIZE];
}hostArg;

/*!
 * @brief File descriptor for creating a pipe.
 */
int shell_host_pipe[2];
int host_ack_pipe[2];

void *startShellThread();
void *StartHostDSPThread();

static bool file_playing = false;
static bool exit_flag = false;

FILE * fp;
int fdmax_sh = 0;
int fdmax_ha = 0;

/*!
 * @brief List of commands supported.
 */
void commandmenu(void);

/*!
 * @brief Set major and minor version numbers.
 */
static void initMessage(struct xrpm_message *msg);

/*!
 * @brief Parse and decode user input from command line args.
 */
int DecodeCommandLine(int argc, char **argv,formatCommand *format_command);

/*!
 * @brief Parse and decode user input from console.
 */
int DecodeCommandUserInput(char user_input[],formatCommand *format_command);

/*!
 * @brief Process commands and transfer data/command to DSP.
 */
int host_dsp_start(int devid,formatCommand format_command);

/*!
 * @brief To process and decode user input and pass to command processor.
 */
int shellCommand(char user_input[],formatCommand *format_command,hostArg *args);

/*!
 * @brief To read the data from the file.
 */
void shellFileStart(struct xrpm_message *msg,formatCommand format_command);

/*!
 * @brief To parse file extension and set the file type.
 */
void setup_filetype(const char *dot,formatCommand *format_command);
#endif