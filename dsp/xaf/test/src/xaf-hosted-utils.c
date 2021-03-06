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
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


#include "xaf-hosted-utils.h"
#include "xrpm_msg.h"

#include "xaf-api.h"



#define AUDIO_BUFFER_FILL_THRESHOLD (16 * 1024)

long long dsp_comps_cycles, enc_cycles, dec_cycles, mix_cycles, pcm_gain_cycles, src_cycles,capturer_cycles, renderer_cycles,aac_dec_cycles;
long long tot_cycles, frmwk_cycles, fread_cycles, fwrite_cycles;
 
 
 
 
/*******************************************************************************
 * Utility Functions
 ******************************************************************************/

/* Wrap stdlib malloc() for audio framework allocator */
void *DSP_Malloc(int32_t size, int32_t id)
{
    /* If malloc can return un-aligned data, must return 4-byte aligned pointer for XAF. */
    return malloc(size);
}

/* Wrap stdlib free() for audio framework allocator */
void DSP_Free(void *ptr, int32_t id)
{
    free(ptr);
}

uint32_t DSP_AudioReadRing(dsp_handle_t *dsp, char *data, uint32_t size)
{
    uint32_t bytes_read=0;

    xos_mutex_lock(&dsp->audioMutex);
    bytes_read = circularbuf_read(dsp->audioBuffer, (uint8_t *)data, size);
	xos_mutex_unlock(&dsp->audioMutex);
	
    if (bytes_read != size)
    {
        /* UNDERRUN */
    }

    return bytes_read;
}

uint32_t DSP_AudioWriteRing(dsp_handle_t *dsp, char *data, uint32_t size)
{
    uint32_t written;

    xos_mutex_lock(&dsp->audioMutex);
    written = circularbuf_write(dsp->audioBuffer, (uint8_t *)data, size);
    xos_mutex_unlock(&dsp->audioMutex);

    if (written != size)
    {
        /* OVERFLOW */
    }

    return written;
}

uint32_t DSP_AudioSizeRing(dsp_handle_t *dsp)
{
    uint32_t size;

    xos_mutex_lock(&dsp->audioMutex);
    size = circularbuf_get_filled(dsp->audioBuffer);
    xos_mutex_unlock(&dsp->audioMutex);

    return size;
}

/* Read audio data for DSP processing
 *
 * param buffer Input buffer from application
 * param out Output buffer
 * param size Size of data to read in bytes
 * return number of bytes read */
uint32_t DSP_AudioRead(dsp_handle_t *dsp, char *data, uint32_t size)
{
    uint32_t read_size = size;

    if (size + dsp->buffer_in.index > dsp->buffer_in.size)
    {
        read_size = dsp->buffer_in.size - dsp->buffer_in.index;
    }

    memcpy(data, &dsp->buffer_in.data[dsp->buffer_in.index], read_size);

    dsp->buffer_in.index += read_size;

    return read_size;
}

/* Consume audio data output from DSP processing
 *
 * param buffer Output buffer
 * param in Input buffer from DSP
 * param size Size of data to write in bytes
 * return number of bytes written */
uint32_t DSP_AudioWrite(dsp_handle_t *dsp, char *data, uint32_t size)
{
    uint32_t write_size = size;

    if (size + dsp->buffer_out.index > dsp->buffer_out.size)
    {
        write_size = dsp->buffer_out.size - dsp->buffer_out.index;
    }

    memcpy(&dsp->buffer_out.data[dsp->buffer_out.index], data, write_size);

    dsp->buffer_out.index += write_size;

    return write_size;
}
extern volatile int request_data;
static void DSP_RequestData(dsp_handle_t *dsp)
{
	dsp->request_data = 1;	
}

void DSP_SendFileEnd(dsp_handle_t *dsp)
{
	dsp->request_data = 2;
	
}

int DSP_BufferThread(void *arg, int wake_value)
{
    dsp_handle_t *ctx = (dsp_handle_t *)arg;

    DSP_PRINTF("[DSP_BufferThread] start\r\n");

    while (ctx->file_playing)
    {
		
        /* Check to see if more input data needed from Host core */
        if (!ctx->eof && !ctx->ipc_waiting)
        {
			
            /* Check circularbuffer fill threshold */
            if (DSP_AudioSizeRing(ctx) < AUDIO_BUFFER_FILL_THRESHOLD)
            {
				
                /* Send request to Host core for more input data */
                DSP_RequestData(ctx);
            }
        }

        xos_thread_sleep_msec(5);
    }

    DSP_PRINTF("[DSP_BufferThread] exiting\r\n");

    return 0;
}

/* Thread for processing DSP pipeline
 *
 * This thread will take care of polling the audio framework for status, feeding
 * data when needed, and consuming output when needed.  It will end when the
 * input or output data is exhausted.
 */
int DSP_ProcessThread(void *arg, int wake_value)
{
    dsp_handle_t *ctx = (dsp_handle_t *)arg;
    xaf_comp_status status;
    XAF_ERR_CODE ret;
    char *buffer;
    uint32_t buffer_len;
    uint32_t event_state;
    int info[4];
    int read_size, write_size;

    DSP_PRINTF("[DSP_ProcessThread] start\r\n");

    ret = xaf_comp_process(NULL, ctx->comp, NULL, 0, XAF_EXEC_FLAG);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("[DSP_ProcessThread] xaf_comp_process XAF_EXEC_FLAG failure: %d\r\n", ret);
        return -1;
    }

    while (1)
    {
        /* Check for external events to the processing thread */
        xos_event_get(&ctx->pipeline_event, &event_state);
        if (event_state & DSP_EVENT_STOP)
        {
            xos_event_clear(&ctx->pipeline_event, DSP_EVENT_STOP);
            /* Send INPUT_OVER to decoder to gracefully shutdown pipeline */
            ret = xaf_comp_process(NULL, ctx->comp, NULL, 0, XAF_INPUT_OVER_FLAG);
            if (ret != XAF_NO_ERR)
            {
                DSP_PRINTF("[DSP_ProcessThread] xaf_comp_process XAF_INPUT_OVER_FLAG failure: %d\r\n", ret);
            }

            break;
        }

        ret = xaf_comp_get_status(NULL, ctx->comp, &status, &info[0]);
        if (ret != XAF_NO_ERR)
        {
            DSP_PRINTF("[DSP_ProcessThread] xaf_comp_get_status failure: %d\r\n", ret);
            return -1;
        }

        if (status == XAF_EXEC_DONE)
        {
            DSP_PRINTF("[DSP_ProcessThread] Execution complete - exiting\r\n");
            break;
        }
        else if (status == XAF_NEED_INPUT)
        {
            /* Read input and feed data to pipeline for processing */
            buffer     = (char *)info[0];
            buffer_len = (uint32_t)info[1];

            read_size = ctx->audio_read(ctx, buffer, buffer_len);
			if(read_size ==0){
				while(!ctx->eof){
					xos_thread_sleep_msec(5);
					read_size = ctx->audio_read(ctx, buffer, buffer_len);
					if(read_size>0){
						break;
					}
				}
			}
            if (read_size > 0)
            {
                ret = xaf_comp_process(NULL, ctx->comp, (void *)buffer, read_size, XAF_INPUT_READY_FLAG);
                if (ret != XAF_NO_ERR)
                {
                    DSP_PRINTF("[DSP_ProcessThread] xaf_comp_process XAF_INPUT_READY_FLAG failure: %d\r\n", ret);
                    return -1;
                }
            }
            else
            {
                ret = xaf_comp_process(NULL, ctx->comp, NULL, 0, XAF_INPUT_OVER_FLAG);
                if (ret != XAF_NO_ERR)
                {
                    DSP_PRINTF("[DSP_ProcessThread] xaf_comp_process XAF_INPUT_OVER_FLAG failure: %d\r\n", ret);
                    return -1;
                }

                DSP_PRINTF("[DSP_ProcessThread] input over\r\n");
            }
        }
        else if (status == XAF_OUTPUT_READY)
        {
            /* Consume output from pipeline */
            buffer     = (char *)info[0];
            buffer_len = (uint32_t)info[1];

            if (buffer_len == 0)
            {				
                /* No output available */
            }
            else
            {
                write_size = ctx->audio_write(ctx, buffer, buffer_len);

                ret = xaf_comp_process(NULL, ctx->comp, buffer, buffer_len, XAF_NEED_OUTPUT_FLAG);
                if (ret != XAF_NO_ERR)
                {
                    DSP_PRINTF("[DSP_ProcessThread] xaf_comp_process XAF_NEED_OUTPUT_FLAG failure: %d\r\n", ret);
                    return -1;
                }
            }
        }
        else
        {
            /* Error or nonstandard response. */
            DSP_PRINTF("[DSP_ProcessThread] unexpected status: %d\r\n", status);
        }
    }

    ctx->file_playing = false;
    DSP_PRINTF("[DSP_ProcessThread] exiting\r\n");

    return 0;
}
