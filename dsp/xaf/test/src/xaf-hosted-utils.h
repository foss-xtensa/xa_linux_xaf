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

#ifndef __XRPM_UTILS_H__
#define __XRPM_UTILS_H__

#include <stdint.h>
#include <xtensa/xos.h>

#include "circularbuffer.h"

#if (INIT_DEBUG_CONSOLE == 1)
#define DSP_PRINTF PRINTF
#else
#define DSP_PRINTF printf
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define DSP_EVENT_STOP (1 << 0)

typedef struct _dsp_buffer_t
{
    char *data;
    uint32_t size;
    uint32_t index;
} dsp_buffer_t;

typedef struct _dsp_handle_t
{


    void *audio_device;
    void *comp_codec;
    void *comp_renderer;
    void *comp_client_proxy;
    void *comp;
    XosThread dec_thread;
    XosThread buffer_thread;
    XosThread cleanup_thread;
    XosEvent pipeline_event;

    /* Audio buffer read/write function pointers for processing thread */
    uint32_t (*audio_read)(struct _dsp_handle_t *dsp, char *data, uint32_t size);
    uint32_t (*audio_write)(struct _dsp_handle_t *dsp, char *data, uint32_t size);

    /* Memory buffer playback variables */
    dsp_buffer_t buffer_in;
    dsp_buffer_t buffer_in2;
    dsp_buffer_t buffer_out;

    /* File playback circular buffer variables */
    circular_buf_t *audioBuffer;
    XosMutex audioMutex;
    XosMutex rpmsgMutex;

    /* File playback state management variables */
    volatile bool eof;
    volatile bool ipc_waiting;
    volatile bool file_playing;
    
    volatile int request_data;
} dsp_handle_t;

/*******************************************************************************
 * API Function Prototypes
 ******************************************************************************/
void *DSP_Malloc(int32_t size, int32_t id);
void DSP_Free(void *ptr, int32_t id);

uint32_t DSP_AudioRead(dsp_handle_t *dsp, char *data, uint32_t size);
uint32_t DSP_AudioWrite(dsp_handle_t *dsp, char *data, uint32_t size);
uint32_t DSP_AudioReadRing(dsp_handle_t *dsp, char *data, uint32_t size);
uint32_t DSP_AudioWriteRing(dsp_handle_t *dsp, char *data, uint32_t size);

int DSP_ProcessThread(void *arg, int wake_value);
int DSP_BufferThread(void *arg, int wake_value);
void DSP_SendFileEnd(dsp_handle_t *dsp);

#endif /* __XRPM_UTILS_H__ */
