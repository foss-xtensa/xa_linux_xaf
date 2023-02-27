

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xtensa/xos.h>

#include "xaf-api.h"
#include "xrpm_msg.h"

#include "audio/xa-pcm-gain-api.h"

#include "xaf-hosted-utils.h"



/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define AUDIO_FRMWK_BUF_SIZE (256 * 256)
#define AUDIO_COMP_BUF_SIZE  (128 * 1024)
#define NUM_COMP_IN_GRAPH    1

/* Stack size for DSP data processing thread. */
#define STACK_SIZE_COMP (4 * 1024)

// component parameters
static unsigned int XRPM_PCM_GAIN_SAMPLE_WIDTH = 16;
// supports only 16-bit PCM
static unsigned int XRPM_PCM_GAIN_NUM_CH = 1;
// supports 1 and 2 channels only
static unsigned int XRPM_PCM_GAIN_IDX_FOR_GAIN = 4;
// gain index range is 0 to 6 -> {0db, -6db, -12db, -18db, 6db, 12db, 18db}
static unsigned int XRPM_PCM_GAIN_SAMPLE_RATE = 44100;

/*******************************************************************************
 * Component Setup/ Config
 ******************************************************************************/
static int pcm_gain_setup(void *p_comp)
{
    int param[10];
    int pcm_width   = XRPM_PCM_GAIN_SAMPLE_WIDTH; // supports only 16-bit PCM
    int num_ch      = XRPM_PCM_GAIN_NUM_CH;       // supports 1 and 2 channels only
    int sample_rate = XRPM_PCM_GAIN_SAMPLE_RATE;
    int gain_idx =
        XRPM_PCM_GAIN_IDX_FOR_GAIN; // gain index range is 0 to 6 -> {0db, -6db, -12db, -18db, 6db, 12db, 18db}
    int frame_size = XAF_INBUF_SIZE;

    param[0] = XA_PCM_GAIN_CONFIG_PARAM_CHANNELS;
    param[1] = num_ch;
    param[2] = XA_PCM_GAIN_CONFIG_PARAM_SAMPLE_RATE;
    param[3] = sample_rate;
    param[4] = XA_PCM_GAIN_CONFIG_PARAM_PCM_WIDTH;
    param[5] = pcm_width;
    param[6] = XA_PCM_GAIN_CONFIG_PARAM_FRAME_SIZE;
    param[7] = frame_size;
    param[8] = XA_PCM_GAIN_CONFIG_PARAM_GAIN_FACTOR;
    param[9] = gain_idx;

    return (xaf_comp_set_config(p_comp, 5, &param[0]));
}

static int get_comp_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[6];
    int ret;

    param[0] = XA_PCM_GAIN_CONFIG_PARAM_CHANNELS;
    param[2] = XA_PCM_GAIN_CONFIG_PARAM_PCM_WIDTH;
    param[4] = XA_PCM_GAIN_CONFIG_PARAM_SAMPLE_RATE;

    ret = xaf_comp_get_config(p_comp, 3, &param[0]);
    if (ret < 0)
        return ret;

    comp_format->channels    = param[1];
    comp_format->pcm_width   = param[3];
    comp_format->sample_rate = param[5];

    return 0;
}

/*******************************************************************************
 * Commands processing
 ******************************************************************************/
int xrpm_pcm_gain(dsp_handle_t *dsp, unsigned int *pCmdParams)
{
    void *p_adev = NULL;
    XosThread pcm_gain_thread;
    unsigned char pcm_gain_stack[STACK_SIZE_COMP];
    void *p_pcm_gain = NULL;
    xaf_comp_status pcm_gain_status;
    int pcm_gain_info[4];
    void *pcm_gain_inbuf[2];
    int read_length;
    int i, cid = 0;
    xaf_format_t pcm_gain_format;
    xf_id_t comp_id = "post-proc/pcm_gain";
    int (*comp_setup)(void *p_comp);
    int num_comp;
    xaf_comp_type comp_type;

    TRACE_INIT("Xtensa Audio Framework - Linux + Host v2.10");
    num_comp = NUM_COMP_IN_GRAPH;

    uint32_t *input_size, *output_size;
    XAF_ERR_CODE ret;


    dsp->buffer_in[cid].data  = (char *)pCmdParams[PARAM_INDEX_IN_BUF_OFFSET];

    dsp->buffer_in[cid].size = (uint32_t)pCmdParams[PARAM_INDEX_IN_BUF_SIZE];


    dsp->buffer_out[cid].data = (char *)pCmdParams[PARAM_INDEX_OUT_BUF_OFFSET];

    dsp->buffer_out[cid].size = (uint32_t)pCmdParams[PARAM_INDEX_OUT_BUF_SIZE];

    dsp->buffer_in[cid].index  = 0;
    dsp->buffer_out[cid].index = 0;

    unsigned int xrpm_sampling_rate = (unsigned int)(pCmdParams[PARAM_INDEX_COMP_SAMPLE_RATE]);
    unsigned int xrpm_channel_num   = (unsigned int)(pCmdParams[PARAM_INDEX_COMP_CHANNELS]);
    unsigned int xrpm_sample_width  = (unsigned int)(pCmdParams[PARAM_INDEX_COMP_PCM_WIDTH]);
    unsigned int xrpm_gain_index    = (unsigned int)(pCmdParams[PARAM_INDEX_COMP_PCM_GAIN_INDEX]);

    input_size  = &pCmdParams[PARAM_INDEX_IN_BYTES_CONSUMED];
    output_size = &pCmdParams[PARAM_INDEX_OUT_BYTES_PRODUCED];

    comp_setup = pcm_gain_setup;

    XRPM_PCM_GAIN_SAMPLE_RATE  = xrpm_sampling_rate;
    XRPM_PCM_GAIN_NUM_CH       = xrpm_channel_num;
    XRPM_PCM_GAIN_SAMPLE_WIDTH = xrpm_sample_width;
    XRPM_PCM_GAIN_IDX_FOR_GAIN = xrpm_gain_index;

    if ((dsp->buffer_in[cid].data == 0) || (dsp->buffer_in[cid].size == 0) || (dsp->buffer_out[cid].data == 0) ||
        (dsp->buffer_out[cid].size == 0))
    {
        DSP_PRINTF("Invalid IO Buffers!\n");
        exit(-1);
    }

    xaf_adev_config_t adev_config;
    ret = xaf_adev_config_default_init(&adev_config);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_adev_config_default_init failure: %d\r\n", ret);
        return -1;
    }

    adev_config.pmem_malloc =  DSP_Malloc;
    adev_config.pmem_free =  DSP_Free;
    adev_config.audio_framework_buffer_size =  AUDIO_FRMWK_BUF_SIZE;
    adev_config.audio_component_buffer_size =  AUDIO_COMP_BUF_SIZE;

    ret = xaf_adev_open(&p_adev, &adev_config);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_adev_open failure: %d\r\n", ret);
        return -1;
    }

    DSP_PRINTF("[DSP Gain] Audio Device Ready\r\n");

    /* ...create pcm gain component */
    comp_type = XAF_POST_PROC;
    ret       = COMP_CREATE_API(p_adev, &p_pcm_gain, comp_id, 2, 1, &pcm_gain_inbuf[0], comp_type);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_comp_create failure: %d\r\n", ret);
        return -1;
    }

    comp_setup(p_pcm_gain);

    /* ...start pcm gain component */
    ret = xaf_comp_process(p_adev, p_pcm_gain, NULL, 0, XAF_START_FLAG);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_comp_process XAF_START_FLAG failure: %d\r\n", ret);
        return -1;
    }

    DSP_PRINTF("[DSP Gain] Gainer component started\r\n");

    /* Feed input to gainer component until initialization is complete */
    while (1)
    {
        read_length = DSP_AudioRead(dsp, pcm_gain_inbuf[0], XAF_INBUF_SIZE, 0 /* cid */);
        if (read_length)
        {
            ret = xaf_comp_process(p_adev, p_pcm_gain, pcm_gain_inbuf[0], read_length, XAF_INPUT_READY_FLAG);
            if (ret != XAF_NO_ERR)
            {
                DSP_PRINTF("xaf_comp_process XAF_INPUT_READY_FLAG failure: %d\r\n", ret);
                return -1;
            }
        }
        else
        {
            break;
        }

        ret = xaf_comp_get_status(p_adev, p_pcm_gain, &pcm_gain_status, &pcm_gain_info[0]);
        if (ret != XAF_NO_ERR)
        {
            DSP_PRINTF("xaf_comp_get_status failure: %d\r\n", ret);
            return -1;
        }

        if (pcm_gain_status == XAF_INIT_DONE || pcm_gain_status == XAF_EXEC_DONE)
        {
            break;
        }
    }

    if (pcm_gain_status != XAF_INIT_DONE)
    {
        DSP_PRINTF("ERROR: Failed to initialize gainer component\r\n");
        return -1;
    }

    get_comp_config(p_pcm_gain, &pcm_gain_format);

    /* Store gain component into context to pass to processing thread */
    dsp->comp[cid] = p_pcm_gain;

    /* Initialize buffer read/write functions */
    dsp->audio_read  = DSP_AudioRead;
    dsp->audio_write = DSP_AudioWrite;

    {
        unsigned int thread_args[2];
        thread_args[0] = cid;
        thread_args[1] = (unsigned int)dsp;
        dsp->file_playing[cid] = 0; /* to not to wait for further input from host, for the example of "gain" */

        /* Start processing thread */
        xos_thread_create(&pcm_gain_thread, NULL, DSP_ProcessThread, (void *)thread_args, "DSP_ProcessThread", pcm_gain_stack,
                      STACK_SIZE_COMP, 7, 0, 0);
    }

    /* Wait for processing thread to complete before exiting. */
    xos_thread_join(&pcm_gain_thread, &i);
    xos_thread_delete(&pcm_gain_thread);

    /* Clean up and shut down XAF */
    ret = xaf_comp_delete(p_pcm_gain);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_comp_delete failure: %d\r\n", ret);
        return -1;
    }

    ret = xaf_adev_close(p_adev, XAF_ADEV_NORMAL_CLOSE);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_adev_close failure: %d\r\n", ret);
        return -1;
    }

    DSP_PRINTF("[DSP Gain] Audio device closed\r\n\r\n");

    /* Report the size of the input and decoded output buffer */
    *input_size  = dsp->buffer_in[cid].index;
    *output_size = dsp->buffer_out[cid].index;

    return 0;
}
