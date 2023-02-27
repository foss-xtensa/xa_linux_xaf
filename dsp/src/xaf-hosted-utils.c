

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
 
 
#ifndef XA_DISABLE_EVENT
/* Disabling error reporting now... can enable this when error handler is implemented in application. */
uint32_t g_enable_error_channel_flag = XAF_ERR_CHANNEL_DISABLE;
#endif
 
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

uint32_t DSP_AudioReadRing(dsp_handle_t *dsp, char *data, uint32_t size, uint32_t cid)
{
    uint32_t bytes_read=0;

    xos_mutex_lock(&dsp->audioMutex);
    bytes_read = circularbuf_read(dsp->audioBuffer[cid], (uint8_t *)data, size);
	xos_mutex_unlock(&dsp->audioMutex);
	
    if (bytes_read != size)
    {
        /* UNDERRUN */
    }

    return bytes_read;
}

uint32_t DSP_AudioWriteRing(dsp_handle_t *dsp, char *data, uint32_t size, uint32_t cid)
{
    uint32_t written;

    xos_mutex_lock(&dsp->audioMutex);
    written = circularbuf_write(dsp->audioBuffer[cid], (uint8_t *)data, size);
    xos_mutex_unlock(&dsp->audioMutex);

    if (written != size)
    {
        /* OVERFLOW */
    }

    return written;
}

uint32_t DSP_AudioSizeRing(dsp_handle_t *dsp, uint32_t cid)
{
    uint32_t size;

    xos_mutex_lock(&dsp->audioMutex);
    size = circularbuf_get_filled(dsp->audioBuffer[cid]);
    xos_mutex_unlock(&dsp->audioMutex);

    return size;
}

/* Read audio data for DSP processing
 *
 * param buffer Input buffer from application
 * param out Output buffer
 * param size Size of data to read in bytes
 * return number of bytes read */
uint32_t DSP_AudioRead(dsp_handle_t *dsp, char *data, uint32_t size, uint32_t cid)
{
    uint32_t read_size = size;

    if (size + dsp->buffer_in[cid].index > dsp->buffer_in[cid].size)
    {
        read_size = dsp->buffer_in[cid].size - dsp->buffer_in[cid].index;
    }

    memcpy(data, &dsp->buffer_in[cid].data[dsp->buffer_in[cid].index], read_size);

    dsp->buffer_in[cid].index += read_size;

    return read_size;
}

/* Consume audio data output from DSP processing
 *
 * param buffer Output buffer
 * param in Input buffer from DSP
 * param size Size of data to write in bytes
 * return number of bytes written */
uint32_t DSP_AudioWrite(dsp_handle_t *dsp, char *data, uint32_t size, uint32_t cid)
{
    do
    {
        uint32_t write_size = size;
        uint32_t buffer_full_flag = 0;

        if (size + dsp->buffer_out[cid].index > dsp->buffer_out[cid].size)
        {
            buffer_full_flag = 1;
            write_size = dsp->buffer_out[cid].size - dsp->buffer_out[cid].index;
        }
        
        memcpy(&dsp->buffer_out[cid].data[dsp->buffer_out[cid].index], data, write_size);
        
        dsp->buffer_out[cid].index += write_size;
        //*(uint32_t*)dsp->poutput_produced[cid] = dsp->buffer_out[cid].index;
        
        size -= write_size;
        dsp->response_data[cid] = buffer_full_flag; /* ..set response_data after writing the last chunk to make the buffer full */
	    while(dsp->response_data[cid])
        {
	    	xos_thread_sleep_msec(5);
	    }
    }
    while(size);

    return size;
}
extern volatile int request_data;
static void DSP_RequestData(dsp_handle_t *dsp, uint32_t cid)
{
	dsp->request_data[cid] = 1;	
}

void DSP_SendFileEnd(dsp_handle_t *dsp, uint32_t cid)
{
	dsp->request_data[cid] = 2;
	
}

int DSP_BufferThreadInput(void *arg, int wake_value)
{
    unsigned int *pargs = (unsigned int*)arg;
    unsigned int cid = pargs[0];
    dsp_handle_t *dsp = (dsp_handle_t *)pargs[1];

    DSP_PRINTF("[DSP_BufferThreadInput] start cid[%d]\r\n", cid);

    while (dsp->request_data[cid] == 3)
        xos_thread_sleep_msec(5);

    DSP_PRINTF("[DSP_BufferThreadInput] FileStarted cid[%d] dsp->request_data[cid]:%d\r\n", cid, dsp->request_data[cid]);
    while (dsp->file_playing[cid])
    {
		
        /* Check to see if more input data needed from Host core */
        if (!dsp->eof[cid])
        {
			
            /* Check circularbuffer fill threshold */
            if (DSP_AudioSizeRing(dsp, cid) < AUDIO_BUFFER_FILL_THRESHOLD)
            {
				
                /* Send request to Host core for more input data */
                DSP_RequestData(dsp, cid);
            }
        }

        xos_thread_sleep_msec(5);
    }

    DSP_PRINTF("[DSP_BufferThreadInput] exiting cid:%d\r\n", cid);

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
    unsigned int *pargs = (unsigned int *)arg;
    unsigned int cid = (unsigned int)pargs[0];
    dsp_handle_t *dsp = (dsp_handle_t *)pargs[1];

    xaf_comp_status status;
    XAF_ERR_CODE ret;
    char *buffer;
    uint32_t buffer_len;
    uint32_t event_state;
    int info[4];
    int read_size, write_size=0;

    DSP_PRINTF("[DSP_ProcessThread] start cid:%d\r\n", cid);

    ret = xaf_comp_process(NULL, dsp->comp[cid], NULL, 0, XAF_EXEC_FLAG);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("[DSP_ProcessThread] xaf_comp_process XAF_EXEC_FLAG failure: %d cid:%d\r\n", ret, cid);
        return -1;
    }

    while (1)
    {
        /* Check for external events to the processing thread */
        xos_event_get(&dsp->pipeline_event, &event_state);
        if (event_state & DSP_EVENT_STOP)
        {
            xos_event_clear(&dsp->pipeline_event, DSP_EVENT_STOP);
            /* Send INPUT_OVER to decoder to gracefully shutdown pipeline */
            ret = xaf_comp_process(NULL, dsp->comp[cid], NULL, 0, XAF_INPUT_OVER_FLAG);
            if (ret != XAF_NO_ERR)
            {
                DSP_PRINTF("[DSP_ProcessThread] xaf_comp_process XAF_INPUT_OVER_FLAG failure: %d cid:%d\r\n", ret, cid);
            }

            break;
        }

        ret = xaf_comp_get_status(NULL, dsp->comp[cid], &status, &info[0]);
        if (ret != XAF_NO_ERR)
        {
            DSP_PRINTF("[DSP_ProcessThread] xaf_comp_get_status failure: %d cid:%d\r\n", ret, cid);
            return -1;
        }

        if (status == XAF_EXEC_DONE)
        {
            if(dsp->ninbufs[cid])
            {
                DSP_SendFileEnd(dsp, cid);
            }
            DSP_PRINTF("[DSP_ProcessThread] Execution complete - exiting loop cid:%d\r\n", cid);
            break;
        }
        else if (status == XAF_NEED_INPUT)
        {
            /* Read input and feed data to pipeline for processing */
            buffer     = (char *)info[0];
            buffer_len = (uint32_t)info[1];

            read_size = dsp->audio_read(dsp, buffer, buffer_len, cid);
			if(read_size ==0){
				while(!dsp->eof[cid]){
					xos_thread_sleep_msec(5);
					read_size = dsp->audio_read(dsp, buffer, buffer_len, cid);
					if(read_size>0){
						break;
					}
				}
			}
            if (read_size > 0)
            {
                ret = xaf_comp_process(NULL, dsp->comp[cid], (void *)buffer, read_size, XAF_INPUT_READY_FLAG);
                if (ret != XAF_NO_ERR)
                {
                    DSP_PRINTF("[DSP_ProcessThread] xaf_comp_process XAF_INPUT_READY_FLAG failure: %d cid:%d\r\n", ret, cid);
                    return -1;
                }
            }
            else
            {
                ret = xaf_comp_process(NULL, dsp->comp[cid], NULL, 0, XAF_INPUT_OVER_FLAG);
                if (ret != XAF_NO_ERR)
                {
                    DSP_PRINTF("[DSP_ProcessThread] xaf_comp_process XAF_INPUT_OVER_FLAG failure: %d cid:%d\r\n", ret, cid);
                    return -1;
                }

                DSP_PRINTF("[DSP_ProcessThread] input over cid:%d\r\n", cid);
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
                dsp->eofOutput[cid] = 1;
            }
            else
            {
                write_size = dsp->audio_write(dsp, buffer, buffer_len, cid);

                ret = xaf_comp_process(NULL, dsp->comp[cid], buffer, buffer_len, XAF_NEED_OUTPUT_FLAG);
                if (ret != XAF_NO_ERR)
                {
                    DSP_PRINTF("[DSP_ProcessThread] xaf_comp_process XAF_NEED_OUTPUT_FLAG failure: %d cid:%d\r\n", ret, cid);
                    return -1;
                }
            }
        }
        else
        {
            /* Error or nonstandard response. */
            DSP_PRINTF("[DSP_ProcessThread] unexpected status: %d cid:%d\r\n", status, cid);
        }
    }

    dsp->eofOutput[cid] = 1;

    if( dsp->buffer_out[cid].index && dsp->file_playing[cid])
    {
        if(dsp->response_data[cid] == 0) 
           dsp->response_data[cid] = 2;
        //DSP_PRINTF("[DSP_ProcessThread] exec_done waiting on outbuf cid:%d (response_data:%d) buffer_out.index:%d\r\n", cid, dsp->response_data[cid], dsp->buffer_out[cid].index);
	    while(dsp->response_data[cid])
        {
            /* ... wait until buffer is sent to host */
	    	xos_thread_sleep_msec(5);
        }
    }

    dsp->file_playing[cid] = false;

    DSP_PRINTF("[DSP_ProcessThread] exiting thread cid:%d\r\n", cid);

    return 0;
}

int DSP_ConnectThread(void *arg, int wake_value)
{
    dsp_handle_t *dsp = (dsp_handle_t *)arg;
    xaf_comp_status comp_status;
    XAF_ERR_CODE ret;
    int comp_info[4];
    int nconnects = dsp->connect_info[0];
    unsigned int src_cid_prev = -1;
    unsigned int dst_cid_prev = -1;
    unsigned int *pCmdParams;

    DSP_PRINTF("[DSP_ConnectThread] start\r\n");

    while(nconnects)
    {
        pCmdParams = (unsigned int*)&dsp->connect_info[dsp->connect_info[1]];
        unsigned int src_cid = pCmdParams[0];
        unsigned int src_port= pCmdParams[1];
        unsigned int dst_cid = pCmdParams[2];
        unsigned int dst_port= pCmdParams[3];
        unsigned int nbufs = pCmdParams[4];

        if(src_cid_prev == -1)
            src_cid_prev = src_cid;
        if(dst_cid_prev == -1)
            dst_cid_prev = dst_cid;

        dsp->connect_info[1] += 5;
        dsp->connect_info[0]--;
        nconnects--;
        if((src_cid_prev == src_cid) || (dst_cid_prev == src_cid))
        {
            ret = xaf_connect(dsp->comp[src_cid], src_port, dsp->comp[dst_cid], dst_port, nbufs);
            if (ret != XAF_NO_ERR)
            {
                DSP_PRINTF("xaf_connect failure: %d cid:%d, port:%d->cid:%d, port:%d nbufs:%d\r\n", ret, src_cid, src_port, dst_cid, dst_port, nbufs);
                return ret;
            }
            
            ret = xaf_comp_get_status(dsp->audio_device, dsp->comp[dst_cid], &comp_status, &comp_info[0]);
            if (ret != XAF_NO_ERR)
            {
                DSP_PRINTF("xaf_comp_get_status failure: %d cid:%d\r\n", ret, dst_cid);
                return ret;
            }
            if (comp_status != XAF_INIT_DONE)
            {
                DSP_PRINTF("ERROR: Failed to initialize decoder component: %d cid:%d@%d\r\n", comp_status, dst_cid, __LINE__);
                return -1;
            }
            DSP_PRINTF("[DSP Codec] connected component initialized cid:%d\r\n", dst_cid);
            
            if(dsp->noutbufs[dst_cid] && !dsp->ninbufs[dst_cid] && !dsp->comp_thread_state[dst_cid])
            {
                extern char dec_stack[DSP_NUM_COMP_IN_GRAPH_MAX][STACK_SIZE_COMP];
                dsp->thread_args[0] = dst_cid;
                dsp->thread_args[1] = (unsigned int)dsp;
            
                /* Start processing thread */
                xos_thread_create(&dsp->dec_thread[dst_cid], NULL, DSP_ProcessThread, (void *)dsp->thread_args, "DSP_ProcessThread", dec_stack[dst_cid],
                              STACK_SIZE_COMP, 5, 0, 0);
                dsp->comp_thread_state[dst_cid] = 1;
            }
        }//if(cid == src_cid)
        dst_cid_prev = dst_cid;
        src_cid_prev = src_cid;
    }//while(nconnects);

    DSP_PRINTF("[DSP_ConnectThread] connects done, exiting@%d\r\n", __LINE__);

    return 0;
}
