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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xrpm_msg.h"
#include "xaf-hosted-utils.h"

#include <xtensa/config/core.h>
#include <xtensa/xos.h>
#include "xaf-api.h"

 
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define INIT_DEBUG_CONSOLE 0


#define DSP_THREAD_STACK_SIZE (8 * 1024)
#define DSP_THREAD_PRIORITY   (XOS_MAX_PRIORITY - 3)

#define AUDIO_BUFFER_SIZE (32 * 1024)

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/* XRPM prototypes */
int xrpm_decoder(dsp_handle_t *dsp, unsigned int *pCmdParams, unsigned int dec_name);
int xrpm_encoder(dsp_handle_t *dsp, unsigned int *pCmdParams, unsigned int enc_name);
int xrpm_src(dsp_handle_t *dsp, unsigned int *pCmdParams);
int xrpm_pcm_gain(dsp_handle_t *dsp, unsigned int *pCmdParams);
int xrpm_file_dec_create(dsp_handle_t *dsp, xrpm_audio_component_t type);
int xrpm_capturer_gain_renderer_init(unsigned int *pCmdParams, bool i2s);



/*******************************************************************************
 * Variables
 ******************************************************************************/



dsp_handle_t dsp;

//static uint8_t dsp_thread_stack[DSP_THREAD_STACK_SIZE];

int readBufferSize, writeBufferSize;
int readBufferPtr, writeBufferPtr;
int num_bytes_write, num_bytes_read;

/*******************************************************************************
 * Code
 ******************************************************************************/
#define TICK_CYCLES       (10000)
#define CLK_FREQ      (100000000)

static void XOS_Init(void)
{
    xos_set_clock_freq(CLK_FREQ);
    xos_start_system_timer(0, TICK_CYCLES);
}


static void DSP_XAF_Init(dsp_handle_t *dsp)
{
    uint8_t *version[3];

    xos_mutex_create(&dsp->audioMutex, XOS_MUTEX_WAIT_PRIORITY, 0);
    
    dsp->audioBuffer = circularbuf_create(AUDIO_BUFFER_SIZE);
    if (!dsp->audioBuffer)
    {
        DSP_PRINTF("[DSP_XAF_Init] circularbuffer allocation failed\r\n");
    }

    xos_event_create(&dsp->pipeline_event, 0xFF, XOS_EVENT_AUTO_CLEAR);

    xaf_get_verinfo(version);

    DSP_PRINTF("\r\n");
    DSP_PRINTF("  Cadence Xtensa Audio Framework\r\n");
    DSP_PRINTF("  Library Name    : %s\r\n", version[0]);
    DSP_PRINTF("  Library Version : %s\r\n", version[1]);
    DSP_PRINTF("  API Version     : %s\r\n", version[2]);
    DSP_PRINTF("\r\n");
}


static int handleMSG_GENERAL(dsp_handle_t *dsp, xrpm_message *msg)
{
    switch (msg->head.command)
    {
        /* Return SDK key component versions to host*/
        case XRPM_Command_ECHO:
            // 0 Audio Framework version high 16 bits major, lower 16 bits minor
            msg->param[0] = ((2) << 16 | (3));
            // 1 Audio Framework API version high 16 bits major, lower 16 bits minor
            msg->param[1] = ((1) << 16 | (3));
            // 6 VORBIS Decoder version high 16 bits major, lower 16 bits minor
            msg->param[2] = ((1) << 16 | (12));
            break;

        /* Unknown message. */
        default:
            msg->head.type = XRPM_MessageTypeNotification;
            msg->error     = XRPM_Status_InvalidMessage;
            break;
    }

    return 0;
}

static void wait_msg_request_data(dsp_handle_t *dsp, xrpm_message *msg)
{
    //@Temp Fix: Wait to pull data from host based on DSP request..
    while(1){
        if(dsp->request_data == 1){
#ifdef DEBUG_LOG				
            printf("\n[DSP0]buffer_needed:%d\n",buffer_needed);
#endif				
            msg->head.type         = XRPM_MessageTypeRequest;
            msg->head.majorVersion = XRPM_VERSION_MAJOR;
            msg->head.minorVersion = XRPM_VERSION_MINOR;

            msg->head.category = XRPM_MessageCategory_AUDIO;
            msg->head.command  = XRPM_Command_FileData;				
            break;
        }else if(dsp->request_data == 2 && (!dsp->file_playing)){
            msg->head.type         = XRPM_MessageTypeRequest;
            msg->head.majorVersion = XRPM_VERSION_MAJOR;
            msg->head.minorVersion = XRPM_VERSION_MINOR;

            msg->head.category = XRPM_MessageCategory_AUDIO;
            msg->head.command  = XRPM_Command_FileEnd;
#ifdef DEBUG_LOG 				
            printf("\n[DSP0] buffer_needed:%d\n",buffer_needed);
#endif 				
            break;
        }
        xos_thread_sleep_msec(5);
    }
}
static int handleMSG_AUDIO(dsp_handle_t *dsp, xrpm_message *msg)
{
    switch (msg->head.command)
    {
        case XRPM_Command_GAIN:
            /* Param 0 PCM input buffer offset*/
            /* Param 1 PCM input buffer size*/
            /* Param 2 PCM output buffer offset*/
            /* Param 3 PCM output buffer size*/
            /* Param 4 PCM sampling rate, default 44100*/
            /* Param 5 PCM number of channels, only 1 or 2 supported, default 1*/
            /* Param 6 PCM sample width, default 16*/
            /* Param 7 Gain control index, default is 4, range is 0 to 6 -> {0db, -6db, -12db, -18db, 6db,
            12db,
             * 18db}*/
            /* Param 8 return parameter, actual read bytes*/
            /* Param 9 return parameter, actual written bytes*/
            msg->param[10] = msg->param[0];
            msg->param[11] = msg->param[1];
            msg->param[12] = msg->param[2];
            msg->param[13] = msg->param[3];
            msg->param[14] = msg->param[4];
            msg->param[15] = msg->param[5];
            msg->param[16] = msg->param[6];
            msg->param[17] = msg->param[7];
#ifdef DEBUG_LOG
            DSP_PRINTF(
                "Input buffer addr 0x%X, size %d, output buffer addr 0x%X, size %d, sampling rate %d, num \
				of channels %d, pcm sample width %d, gain control index %d\r\n",
                msg->param[0], msg->param[1], msg->param[2], msg->param[3], msg->param[4], msg->param[5], msg->param[6],
                msg->param[7]);
#endif
            if ((msg->param[0] == 0) || (msg->param[1] == 0) || (msg->param[2] == 0) || (msg->param[3] == 0) ||
                (msg->param[4] == 0) || (msg->param[5] == 0) || (msg->param[6] == 0))
            {
                msg->head.type = XRPM_MessageTypeNotification;
                msg->error     = XRPM_Status_InvalidParameter;
            }
            else
            {
				dsp->eof = 1;//we pass whole buffer
                msg->error = xrpm_pcm_gain(dsp, (unsigned int *)&msg->param[0]);
            }
            break;

        case XRPM_Command_FileStart:
            /* Param 0 File buffer */
            /* Param 1 File buffer size */
            if ((msg->param[0] == 0) || (msg->param[1] == 0))
            {
                msg->head.type = XRPM_MessageTypeNotification;
                msg->error     = XRPM_Status_InvalidParameter;
            }
            /* Check to see if file already playing */
            else if (dsp->file_playing)
            {
                msg->error = XRPM_Status_InvalidState;
            }
            else
            {                
				/* Clear circularbuffer from previous playback */
                circularbuf_clear(dsp->audioBuffer);
                /* Write initial data chunk to the circularbuffer */
                DSP_AudioWriteRing(dsp, (char *)msg->param[0], msg->param[1]);
                /* Initialize pipeline */
                dsp->eof         = msg->param[2];
                dsp->ipc_waiting = false;
                msg->error       = xrpm_file_dec_create(dsp, msg->param[3]);
                
                wait_msg_request_data(dsp, msg);
            }           
            break;
        case XRPM_Command_FileData:
            /* Param 0 File buffer */
            /* Param 1 File buffer size */
            /* Param 2 File EOF status (boolean true/false) */			
            if (msg->param[0] == 0)
            {				
                msg->head.type = XRPM_MessageTypeNotification;
                msg->error     = XRPM_Status_InvalidParameter;
            }
            else
            {				
                DSP_AudioWriteRing(dsp, (char *)msg->param[0], msg->param[1]);
                /* Signal EOF to processing thread if set by remote */
                if (msg->param[2])
                {
										
                    dsp->eof = true;
					DSP_SendFileEnd(dsp);
                }
                wait_msg_request_data(dsp, msg);
            }

            dsp->ipc_waiting = false;            
            
            
            break;	
		case XRPM_Command_FileStop:
            /* File must be already playing */
            if (!dsp->file_playing)
            {
                msg->error = XRPM_Status_InvalidState;
            }
            else
            {
                DSP_PRINTF("File playback stop\r\n");
                xos_event_set(&dsp->pipeline_event, DSP_EVENT_STOP);
            }
            break;
        /* Unknown message. */
        default:
            msg->head.type = XRPM_MessageTypeNotification;
            msg->error     = XRPM_Status_InvalidMessage;
    }   
    
    return 0;
}

static int DSP_MSG_Process(dsp_handle_t *dsp, xrpm_message *msg)
{
    xrpm_message_type_t input_type = msg->head.type;
    /* Sanity check */
    if ((msg->head.majorVersion != XRPM_VERSION_MAJOR) || (msg->head.minorVersion != XRPM_VERSION_MINOR))
    {
        DSP_PRINTF("XRPM version doesn't match!\r\n");
        return -1;
    }

    /* Process message */
    switch (msg->head.category)
    {
        case XRPM_MessageCategory_GENERAL:
            handleMSG_GENERAL(dsp, msg);
            break;
        case XRPM_MessageCategory_AUDIO:
			handleMSG_AUDIO(dsp, msg);
            break;
        default:
            msg->head.type = XRPM_MessageTypeNotification;
            msg->error     = XRPM_Status_InvalidMessage;
    }

    /* Send response message, if request was received */
    if (input_type == XRPM_MessageTypeRequest)
    {
        /* Prepare response msg */
        msg->head.type = XRPM_MessageTypeResponse;

        /* Send response */
    }

    return 0;
}

/*
 * @brief XAF_Init function
 */
void XAF_Init()
{
#ifdef DEBUG_LOG
	DSP_PRINTF("[XAF_Init] start\r\n");
#endif // #ifdef DEBUG_LOG
	DSP_XAF_Init(&dsp);
    XOS_Init();
}

/*
 * @brief Main function
 */
int DSP_Main(void *arg)
{
    
    xrpm_message *msg= (xrpm_message *)arg;
    int status;

    dsp.request_data = 0;
    
	/* Process request */
    status = DSP_MSG_Process(&dsp, msg);

    return 0;
}

