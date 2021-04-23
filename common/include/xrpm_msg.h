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

#ifndef _XRPM_MSG_H
#define _XRPM_MSG_H

#include <stdint.h>

/*! Macro to define a variable with alignbytes alignment */
#define SDK_ALIGN(var, alignbytes) var __attribute__((aligned(alignbytes)))

/*! @brief Defines XRPM major version */
#define XRPM_VERSION_MAJOR (0x00U)

/*! @brief Defines XRPM minor version */
#define XRPM_VERSION_MINOR (0x01U)

/**
 * @brief XRPM message type fields
 */
typedef enum _xrpm_message_type
{
    XRPM_MessageTypeRequest = 0x00U, /*!< Request message */
    XRPM_MessageTypeResponse,        /*!< Response message for certain Request */
    XRPM_MessageTypeNotification,    /*!< Notification message that doesn't require response */
    XRPM_MessageTypeCommLast,        /*!< Last value of communication message */
    XRPM_MessageTypeProcedure = 0x40, /*!< Local procedure */
    XRPM_MessageTypeRawData   = 0x41, /*!< Raw msg message */
} xrpm_message_type_t;

/*! @brief XRPM error code */
typedef enum _xrpm_status
{
    XRPM_Status_Success = 0x00U, /*!< Success */
    XRPM_Status_Error,           /*!< Failed */

    XRPM_Status_InvalidParameter, /*!< Invalid parameter */
    XRPM_Status_InvalidMessage,   /*!< Invalid message */
    XRPM_Status_InvalidState,     /*!< Operate in invalid state */
    XRPM_Status_OutOfMemory,      /*!< Memory allocation failed */
    XRPM_Status_Timeout,          /*!< Timeout when waiting for an event */
    XRPM_Status_ListAddFailed,    /*!< Cannot add to list as node already in another list */
    XRPM_Status_ListRemoveFailed, /*!< Cannot remove from list as node not in list */

    XRPM_Status_TransferTimeout,  /*!< Transfer timeout */
    XRPM_Status_TransferNotAvail, /*!< Transfer failed due to peer core not ready */
    XRPM_Status_TransferFailed,   /*!< Transfer failed due to communication failure */

    XRPM_Status_ServiceNotFound,    /*!< Cannot find service for a request/notification */
    XRPM_Status_ServiceVerMismatch, /*!< Service version cannot support the request/notification */
} xrpm_status_t;

/**
 * @brief XRPM communication packet head
 * Do NOT use any typedef enums for any shared structure, the size will be different cross different platforms!
 * ONLY use basic msg types for consistant structure size!
 */
typedef struct _xrpm_packet_head
{
    char category;
    char majorVersion;
    char minorVersion;
    char type;
    char command;
    char priority;
    char reserved[4U];
} xrpm_packet_head_t;

#define AUDIO_MAX_INPUT_BUFFER  (113 * 1024)
#define AUDIO_MAX_OUTPUT_BUFFER (200 * 1024)

#define XRPM_CMD_PARAMS_MAX 32
typedef struct xrpm_message {
    xrpm_packet_head_t head;/*!< XRPM raw msg, including header and payload for CommMessage */
	int	error;              /*!< XRPM message error status */
    int	param[XRPM_CMD_PARAMS_MAX]; 		/*!< XRPM user defined message params */
	uint8_t s_audioInput[AUDIO_MAX_INPUT_BUFFER];	
	uint8_t s_audioOutput[AUDIO_MAX_OUTPUT_BUFFER];
	
}xrpm_message;

/**
 * @brief XRPM message category fields
 */
typedef enum _xrpm_message_category
{
    XRPM_MessageCategory_GENERAL = 0x00U,
    XRPM_MessageCategory_AUDIO,
    XRPM_MessageCategory_NN,
} xrpm_message_category_t;

/**
 * @brief XRPM audio command fields
 */
typedef enum _xrpm_audio_component
{
    DSP_COMPONENT_MP3,
    DSP_COMPONENT_AAC,
    DSP_COMPONENT_VORBIS,
    DSP_COMPONENT_OPUS
} xrpm_audio_component_t;

/**
 * @brief XRPM dsp audio command fields
 */
typedef enum _xrpm_dsp_audio_command
{
    XRPM_Command_ECHO,      /* Fetch XAF version */
    XRPM_Command_GAIN,      /* PCM Gain control */
    XRPM_Command_FileStart, /* File playback start */
	XRPM_Command_FileData,  /* Data frame from remote file playback */
	XRPM_Command_FileStop,
	XRPM_Command_FileEnd,
	XRPM_Command_EXIT,	
	XRPM_Command_STOP	
} xrpm_audio_command_t;

#endif



