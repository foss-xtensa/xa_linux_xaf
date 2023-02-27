

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <xtensa/xos.h>

#include "xaf-api.h"
#include "audio/xa_mp3_dec_api.h"
#include "audio/xa_aac_dec_api.h"
#include "audio/xa_vorbis_dec_api.h"
#include "audio/xa-renderer-api.h"
#include "audio/xa-audio-decoder-api.h"
#include "audio/xa-pcm-gain-api.h"
#include "audio/xa-opus-encoder-api.h"
#include "audio/xa-opus-decoder-api.h"
#include "xrpm_msg.h"


#include "xaf-hosted-utils.h"


/*******************************************************************************
 * Definitions
 ******************************************************************************/
//#define AUDIO_FRMWK_BUF_SIZE (64 * 1024)
//#define AUDIO_COMP_BUF_SIZE  (256 * 1024)
#define AUDIO_FRMWK_BUF_SIZE (256 <<13)
#define AUDIO_COMP_BUF_SIZE  (1024 << 10)

#define VORBIS_DEC_RAW_VORBIS_LAST_PKT_GRANULE_POS -1
#define VORBIS_DEC_OGG_MAX_PAGE_SIZE               12
#define VORBIS_DEC_RUNTIME_MEM                     0

#define MP3_DEC_PCM_WIDTH          16
#define AAC_DEC_PCM_WIDTH          16
#define CLIENT_PROXY_FRAME_SIZE_US (10000)

/* Stack size for DSP data processing thread. */
#define STACK_SIZE_COMP (4 * 1024)

char dec_stack[DSP_NUM_COMP_IN_GRAPH_MAX][STACK_SIZE_COMP];
char buffer_stack[DSP_NUM_COMP_IN_GRAPH_MAX][STACK_SIZE_COMP];
char cleanup_stack[STACK_SIZE_COMP];
char connect_stack[STACK_SIZE_COMP];

int xrpm_file_dec_close(void *arg, int wake_value);

/*******************************************************************************
 * Component Setup/ Config
 ******************************************************************************/
// component parameters
static unsigned int XRPM_PCM_GAIN_SAMPLE_WIDTH = 16;
// supports only 16-bit PCM
static unsigned int XRPM_PCM_GAIN_NUM_CH = 1;
// supports 1 and 2 channels only
static unsigned int XRPM_PCM_GAIN_IDX_FOR_GAIN = 0;
// gain index range is 0 to 6 -> {0db, -6db, -12db, -18db, 6db, 12db, 18db}
static unsigned int XRPM_PCM_GAIN_SAMPLE_RATE = 44100;

static int xrpm_pcm_gain_setup(void *p_comp)
{
    int param[10];
    int pcm_width   = XRPM_PCM_GAIN_SAMPLE_WIDTH; // supports only 16-bit PCM
    int num_ch      = XRPM_PCM_GAIN_NUM_CH;       // supports 1 and 2 channels only
    int sample_rate = XRPM_PCM_GAIN_SAMPLE_RATE;
    int gain_idx    = XRPM_PCM_GAIN_IDX_FOR_GAIN; // gain index range is 0 to 6 -> {0db, -6db, -12db, -18db, 6db, 12db, 18db}
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

static XAF_ERR_CODE xrpm_vorbis_setup(void *p_decoder)
{
    int param[8];

    param[0] = XA_VORBISDEC_CONFIG_PARAM_RAW_VORBIS_FILE_MODE;
    param[1] = 0;
    param[2] = XA_VORBISDEC_CONFIG_PARAM_RAW_VORBIS_LAST_PKT_GRANULE_POS;
    param[3] = VORBIS_DEC_RAW_VORBIS_LAST_PKT_GRANULE_POS;
#if 0
    param[4] = XA_VORBISDEC_CONFIG_PARAM_OGG_MAX_PAGE_SIZE;
    param[5] = VORBIS_DEC_OGG_MAX_PAGE_SIZE;
    param[6] = XA_VORBISDEC_CONFIG_PARAM_RUNTIME_MEM;
    param[7] = VORBIS_DEC_RUNTIME_MEM;
#endif
    return xaf_comp_set_config(p_decoder, 2, &param[0]);
}

static XAF_ERR_CODE xrpm_aac_setup(void *p_decoder)
{
    int param[2];

    param[0] = XA_AACDEC_CONFIG_PARAM_PCM_WDSZ;
    param[1] = AAC_DEC_PCM_WIDTH;

    return (xaf_comp_set_config(p_decoder, 1, &param[0]));
}

static XAF_ERR_CODE xrpm_mp3_setup(void *p_decoder)
{
    int param[2];

    param[0] = XA_MP3DEC_CONFIG_PARAM_PCM_WDSZ;
    param[1] = MP3_DEC_PCM_WIDTH;

    return xaf_comp_set_config(p_decoder, 1, &param[0]);
}

//component parameters
#define OPUS_ENC_PCM_WIDTH       16
#define OPUS_ENC_SAMPLE_RATE     16000
#define OPUS_ENC_CHANNELS        1

#define OPUS_ENC_APPLICATION		XA_OPUS_APPLICATION_VOIP
#define OPUS_ENC_BITRATE			20000
#define OPUS_ENC_MAX_PAYLOAD		1500
#define OPUS_ENC_COMPLEXITY			10
#define OPUS_ENC_RESET_STATE		0
#define OPUS_ENC_FRAME_SIZE			320
static int xrpm_opus_enc_setup(void *p_encoder)
{
    int param[20];

    param[0] = XA_OPUS_ENC_CONFIG_PARAM_PCM_WIDTH;
    param[1] = OPUS_ENC_PCM_WIDTH;

    param[2] = XA_OPUS_ENC_CONFIG_PARAM_SAMPLE_RATE;
    param[3] = OPUS_ENC_SAMPLE_RATE;

    param[4] = XA_OPUS_ENC_CONFIG_PARAM_CHANNELS;
    param[5] = OPUS_ENC_CHANNELS;

    param[6] = XA_OPUS_ENC_CONFIG_PARAM_APPLICATION;
    param[7] = OPUS_ENC_APPLICATION;

    param[8] = XA_OPUS_ENC_CONFIG_PARAM_BITRATE;
    param[9] = OPUS_ENC_BITRATE;

    param[10] = XA_OPUS_ENC_CONFIG_PARAM_FRAME_SIZE;
    param[11] = OPUS_ENC_FRAME_SIZE;

    param[12] = XA_OPUS_ENC_CONFIG_PARAM_MAX_PAYLOAD;
    param[13] = OPUS_ENC_MAX_PAYLOAD;

    param[14] = XA_OPUS_ENC_CONFIG_PARAM_COMPLEXITY;
    param[15] = OPUS_ENC_COMPLEXITY;

    param[16] = XA_OPUS_ENC_CONFIG_PARAM_SIGNAL_TYPE;
    param[17] = XA_OPUS_SIGNAL_VOICE;

    param[18] = XA_OPUS_ENC_CONFIG_PARAM_RESET_STATE;
    param[19] = OPUS_ENC_RESET_STATE;

    return(xaf_comp_set_config(p_encoder, 10, &param[0]));
}

//decoder parameters
#define OPUS_DEC_PCM_WIDTH              16
#define OPUS_DEC_SAMPLE_RATE            48000
#define OPUS_DEC_CHANNELS               6

#define ENABLE_RAW_OPUS_SET_CONFIG

#ifndef ENABLE_RAW_OPUS_SET_CONFIG
#define XA_EXT_CFG_ID_OFFSET       0
#define XA_EXT_CFG_BUF_PTR_OFFSET  1

static int xrpm_opus_dec_setup(void *p_decoder)
{
    int param[20];
    int ret;

    param[0] = XA_OPUS_DEC_CONFIG_PARAM_PCM_WIDTH;
    param[1] = OPUS_DEC_PCM_WIDTH;

    param[2] = XA_OPUS_DEC_CONFIG_PARAM_SAMPLE_RATE;
    param[3] = OPUS_DEC_SAMPLE_RATE;

    param[4] = XA_OPUS_DEC_CONFIG_PARAM_CHANNELS;
    param[5] = OPUS_DEC_CHANNELS;

    param[6] = XA_OPUS_DEC_CONFIG_PARAM_GAIN;
    param[7] = 0;

    param[8] = XA_OPUS_DEC_CONFIG_PARAM_SILK_INBANDFEC_ENABLE;
    param[9] = 0;

    param[10] = XA_OPUS_DEC_CONFIG_PARAM_NUM_STREAMS;
    param[11] = 4;

    param[12] = XA_OPUS_DEC_CONFIG_PARAM_NUM_COUPLED_STREAMS;
    param[13] = 2;

    param[14] = XA_OPUS_DEC_CONFIG_PARAM_CHAN_MAPPING;
    param[15] = 1;

    param[16] = XA_OPUS_DEC_CONFIG_PARAM_STREAM_TYPE;
    param[17] = 1;

    ret = xaf_comp_set_config(p_decoder, 9, &param[0]);
    if(ret != 0)
        return ret;
    {
#define OPUS_DEC_NUM_SET_PARAMS_EXT	1
        int param_ext[OPUS_DEC_NUM_SET_PARAMS_EXT * 2];
        xaf_ext_buffer_t ext_buf[OPUS_DEC_NUM_SET_PARAMS_EXT];
        memset(ext_buf, 0, sizeof(xaf_ext_buffer_t) * OPUS_DEC_NUM_SET_PARAMS_EXT);

        WORD8 stream_map[XA_OPUS_MAX_NUM_CHANNELS]= {0,4,1,2,3,5,0,0};

        ext_buf[0].max_data_size = sizeof(stream_map);
        ext_buf[0].valid_data_size = sizeof(stream_map);
        ext_buf[0].ext_config_flags |= XAF_EXT_PARAM_SET_FLAG(XAF_EXT_PARAM_FLAG_OFFSET_ZERO_COPY);
        //ext_buf[0].ext_config_flags &= XAF_EXT_PARAM_CLEAR_FLAG(XAF_EXT_PARAM_FLAG_OFFSET_ZERO_COPY);
        ext_buf[0].data = (UWORD8 *) stream_map;

        param_ext[0*2+XA_EXT_CFG_ID_OFFSET] = XA_OPUS_DEC_CONFIG_PARAM_STREAM_MAP;
        param_ext[0*2+XA_EXT_CFG_BUF_PTR_OFFSET] = (int) &ext_buf[0];

        ret = xaf_comp_set_config_ext(p_decoder, OPUS_DEC_NUM_SET_PARAMS_EXT, param_ext);
    }
    return ret;
}
#else //ENABLE_RAW_OPUS_SET_CONFIG
static int xrpm_opus_dec_setup(void *p_decoder)
{
    int param[20];
    int ret;

    param[0] = XA_OPUS_DEC_CONFIG_PARAM_PCM_WIDTH;
    param[1] = OPUS_DEC_PCM_WIDTH;

    param[2] = XA_OPUS_DEC_CONFIG_PARAM_SAMPLE_RATE;
    param[3] = OPUS_DEC_SAMPLE_RATE;

    param[4] = XA_OPUS_DEC_CONFIG_PARAM_CHANNELS;
    param[5] = 2;

    param[6] = XA_OPUS_DEC_CONFIG_PARAM_GAIN;
    param[7] = 0;

    param[8] = XA_OPUS_DEC_CONFIG_PARAM_SILK_INBANDFEC_ENABLE;
    param[9] = 0;

    param[10] = XA_OPUS_DEC_CONFIG_PARAM_NUM_STREAMS;
    param[11] = 1;

    param[12] = XA_OPUS_DEC_CONFIG_PARAM_NUM_COUPLED_STREAMS;
    param[13] = 1;

    param[14] = XA_OPUS_DEC_CONFIG_PARAM_CHAN_MAPPING;
    param[15] = 0;

    param[16] = XA_OPUS_DEC_CONFIG_PARAM_STREAM_TYPE;
    param[17] = 0;

    ret = xaf_comp_set_config(p_decoder, 9, &param[0]);
    return ret;
}
#endif //ENABLE_RAW_OPUS_SET_CONFIG


static XAF_ERR_CODE get_dec_config(void *p_comp, xaf_format_t *comp_format)
{
    XAF_ERR_CODE ret;
    int param[6];

    param[0] = XA_CODEC_CONFIG_PARAM_CHANNELS;
    param[2] = XA_CODEC_CONFIG_PARAM_SAMPLE_RATE;
    param[4] = XA_CODEC_CONFIG_PARAM_PCM_WIDTH;

    ret = xaf_comp_get_config(p_comp, 3, &param[0]);
    if (ret != XAF_NO_ERR)
        return ret;

    comp_format->channels    = param[1];
    comp_format->sample_rate = param[3];
    comp_format->pcm_width   = param[5];

    return XAF_NO_ERR;
}

/*******************************************************************************
 * Commands processing
 ******************************************************************************/
int xrpm_comp_create(dsp_handle_t *dsp, unsigned int *pCmdParams)
{
    xrpm_audio_component_t type;
    XAF_ERR_CODE ret;
    const char *comp_type;
    const char *dec_name;
    XAF_ERR_CODE (*comp_setup)(void *p_comp);
    int iadev_open_flag=0;
    int comp_class, cid;

    cid = pCmdParams[PARAM_INDEX_COMP_ID];
    type = pCmdParams[PARAM_INDEX_COMP_NAME];

    //dsp->buffer_out.data = (unsigned int)pCmdParams[2]; //assigned in the caller function with appropriate offset
    dsp->buffer_out[cid].size = (uint32_t)pCmdParams[PARAM_INDEX_OUT_BUF_SIZE];
    dsp->buffer_out[cid].index = 0;

    dsp->pinput_consumed[cid] = &pCmdParams[PARAM_INDEX_IN_BYTES_CONSUMED];
    dsp->poutput_produced[cid] = &pCmdParams[PARAM_INDEX_OUT_BYTES_PRODUCED];
    dsp->ninbufs[cid] = pCmdParams[PARAM_INDEX_COMP_NUM_INBUF];
    dsp->noutbufs[cid] = pCmdParams[PARAM_INDEX_COMP_NUM_OUTBUF];

    comp_class = XAF_DECODER;
    if (type == DSP_COMPONENT_MP3)
    {
        comp_type  = "audio-decoder/mp3";
        comp_setup = xrpm_mp3_setup;
        dec_name   = "mp3_dec";
    }
    else if (type == DSP_COMPONENT_AAC)
    {
        comp_type  = "audio-decoder/aac";
        comp_setup = xrpm_aac_setup;
        dec_name   = "aacplus_dec";
    }
    else if (type == DSP_COMPONENT_VORBIS)
    {
        comp_type  = "audio-decoder/vorbis";
        comp_setup = xrpm_vorbis_setup;
        dec_name   = "vorbis_dec";
    }
    else if (type == DSP_COMPONENT_PCM_GAIN)
    {
        comp_type  = "post-proc/pcm_gain";
        comp_setup = xrpm_pcm_gain_setup;
        dec_name   = "pcm_gain";
        comp_class = XAF_POST_PROC;
    }
    else if (type == DSP_COMPONENT_OPUS_ENC)
    {
        comp_type  = "audio-encoder/opus";
        comp_setup = xrpm_opus_enc_setup;
        dec_name   = "opus_enc";
        comp_class = XAF_ENCODER;
    }
    else if (type == DSP_COMPONENT_OPUS_DEC)
    {
        comp_type  = "audio-decoder/opus";
        comp_setup = xrpm_opus_dec_setup;
        dec_name   = "opus_dec";
    }
    else
    {
        DSP_PRINTF("invalid codec type: %d\r\n", (int)type);
        return -1;
    }

    if(!dsp->audio_device)
    {
        TRACE_INIT("Xtensa Audio Framework - Linux + Host v2.10");

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
        
        ret = xaf_adev_open(&dsp->audio_device, &adev_config);

        if (ret != XAF_NO_ERR)
        {
            DSP_PRINTF("xaf_adev_open failure: %d\r\n", ret);
            return -1;
        }
        DSP_PRINTF("[DSP Codec] Audio Device Ready\r\n");
        iadev_open_flag = 1;
    }

    ret = COMP_CREATE_API(dsp->audio_device, &dsp->comp_codec, comp_type, dsp->ninbufs[cid], dsp->noutbufs[cid], &dsp->dec_inbuf[cid][0], comp_class);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_comp_create failure: %d comp_type:%s @%d\r\n", ret, comp_type, __LINE__);
        goto error_cleanup;
    }

    ret = comp_setup(dsp->comp_codec);
    if (ret < 0)
    {
        DSP_PRINTF("comp_setup failure: %d comp_type:%s\r\n", ret, comp_type);
        goto error_cleanup;
    }

    DSP_PRINTF("[DSP Codec] Component %s created\r\n", comp_type);

    /* Start decoder component */
    ret = xaf_comp_process(dsp->audio_device, dsp->comp_codec, NULL, 0, XAF_START_FLAG);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_comp_process XAF_START_FLAG failure: %d comp_type:%s @%d\r\n", ret, comp_type, __LINE__);
        goto error_cleanup;
    }

    DSP_PRINTF("[DSP Component] started cid:%d\r\n", cid);

    /* Store decoder component into context to pass to processing thread */
    dsp->comp[cid] = dsp->comp_codec;
    dsp->ncomps++;

    /* Delete previous cleanup thread if valid */
    if(iadev_open_flag)
    {
        xos_event_clear(&dsp->pipeline_event, XOS_EVENT_BITS_ALL);
        if (xos_thread_get_state(&dsp->cleanup_thread) != XOS_THREAD_STATE_INVALID)
        {
            xos_thread_delete(&dsp->cleanup_thread);
            memset((void *)&dsp->cleanup_thread, 0, sizeof(XosThread));
        }
    }

	/* Start Input buffer notification thread */
    if(dsp->ninbufs[cid])
    {
        /* Initialize playback state */
        dsp->file_playing[cid] = true;
        dsp->eof[cid]          = false;
        dsp->request_data[cid] = 3;

        dsp->thread_args[0] = cid;
        dsp->thread_args[1] = (unsigned int)dsp;
        xos_thread_create(&dsp->buffer_thread[cid], NULL, DSP_BufferThreadInput, (void *)dsp->thread_args, "DSP_BufferThreadInput", buffer_stack[cid],
                      STACK_SIZE_COMP, 7, 0, 0);
    }
    else if (dsp->noutbufs[cid])
    {
        dsp->file_playing[cid] = true;
    }

    return 0;

error_cleanup:
    ret = xaf_adev_close(dsp->audio_device, XAF_ADEV_FORCE_CLOSE);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_adev_close failure: %d\r\n", ret);
    }
    else
    {
        DSP_PRINTF("[DSP Codec] Audio device closed\r\n\r\n");
    }

    /* Return error to DSP app so it can be returned to ARM core */
    return -1;
}

int xrpm_file_dec_create(dsp_handle_t *dsp, unsigned int *pCmdParams, int cid)
{
    xaf_comp_status comp_status;
    int comp_info[4];
    uint32_t read_length;
    XAF_ERR_CODE ret;
    int nconnects;
    
    nconnects = dsp->connect_info[0];

    /* Initialize buffer read/write functions */
    dsp->audio_read  = DSP_AudioReadRing;
    dsp->audio_write = DSP_AudioWrite;
	
    //for(cid=0; cid < dsp->ncomps; cid++)
    {
    if(dsp->ninbufs[cid])
    {
        dsp->comp_codec = dsp->comp[cid];
        /* Feed input to decoder component until initialization is complete */
        read_length = 0;
        do
        {
            read_length = DSP_AudioReadRing(dsp, dsp->dec_inbuf[cid][0], XAF_INBUF_SIZE, cid);
            if (read_length)
            {
                ret = xaf_comp_process(dsp->audio_device, dsp->comp_codec, dsp->dec_inbuf[cid][0], read_length, XAF_INPUT_READY_FLAG);
                if (ret != XAF_NO_ERR)
                {
                    DSP_PRINTF("xaf_comp_process XAF_INPUT_READY_FLAG failure: %d cid:%d@%d\r\n", ret, cid, __LINE__);
                    goto error_cleanup;
                }
            }
            else
            {
                ret = xaf_comp_process(dsp->audio_device, dsp->comp_codec, dsp->dec_inbuf[cid][0], read_length, XAF_INPUT_OVER_FLAG);
                 if (ret != XAF_NO_ERR)
                 {
                     DSP_PRINTF("xaf_comp_process XAF_INPUT_OVER_FLAG failure: %d cid:%d@%d\r\n", ret, cid, __LINE__);
                     goto error_cleanup;
                 }
             }
            ret = xaf_comp_get_status(dsp->audio_device, dsp->comp_codec, &comp_status, &comp_info[0]);
            if (ret != XAF_NO_ERR)
            {
                DSP_PRINTF("xaf_comp_get_status failure: %d cid:%d@%d\r\n", ret, cid, __LINE__);
                goto error_cleanup;
            }
 
            if (comp_status == XAF_INIT_DONE || comp_status == XAF_EXEC_DONE)
            {
                break;
            }
        }while (read_length);

        if (comp_status != XAF_INIT_DONE)
        {
            DSP_PRINTF("ERROR: Failed to initialize decoder component: %d cid:%d@%d\r\n", comp_status, cid, __LINE__);
            goto error_cleanup;
        }
        DSP_PRINTF("[DSP Codec] component initialized cid:%d:\r\n", cid);

        dsp->thread_args[0] = cid;
        dsp->thread_args[1] = (unsigned int)dsp;

        /* Start processing thread */
        xos_thread_create(&dsp->dec_thread[cid], NULL, DSP_ProcessThread, (void *)dsp->thread_args, "DSP_ProcessThread", dec_stack[cid],
                          STACK_SIZE_COMP, 5, 0, 0);

        dsp->comp_thread_state[cid] = 1;
    }//if(ninbufs[cid])
    }//for()

#if 1
    if(nconnects)
    {
        /* Start connect thread */
        xos_thread_create(&dsp->connect_thread, NULL, DSP_ConnectThread, (void *)dsp, "DSP_ConnectThread", connect_stack,
                          STACK_SIZE_COMP, 5, 0, 0);
    }
#else
    unsigned int dst_cid_prev = -1;
    while(nconnects)
    {
        pCmdParams = (unsigned int*)&dsp->connect_info[dsp->connect_info[1]];
        unsigned int src_cid = pCmdParams[0];
        unsigned int src_port= pCmdParams[1];
        unsigned int dst_cid = pCmdParams[2];
        unsigned int dst_port= pCmdParams[3];
        unsigned int nbufs = pCmdParams[4];
        dsp->connect_info[0]--;
        dsp->connect_info[1] += 5;
        nconnects--;
        if((cid == src_cid) || (dst_cid_prev == src_cid))
        {
        ret = xaf_connect(dsp->comp[src_cid], src_port, dsp->comp[dst_cid], dst_port, nbufs);
        if (ret != XAF_NO_ERR)
        {
            DSP_PRINTF("xaf_connect failure: %d cid:%d, port:%d->cid:%d, port:%d nbufs:%d\r\n", ret, src_cid, src_port, dst_cid, dst_port, nbufs);
            goto error_cleanup;
        }

        ret = xaf_comp_get_status(dsp->audio_device, dsp->comp[dst_cid], &comp_status, &comp_info[0]);
        if (ret != XAF_NO_ERR)
        {
            DSP_PRINTF("xaf_comp_get_status failure: %d cid:%d\r\n", ret, dst_cid);
            goto error_cleanup;
        }
        if (comp_status != XAF_INIT_DONE)
        {
            DSP_PRINTF("ERROR: Failed to initialize decoder component: %d cid:%d@%d\r\n", comp_status, dst_cid, __LINE__);
            goto error_cleanup;
        }
        DSP_PRINTF("[DSP Codec] connected component initialized cid:%d\r\n", dst_cid);

        if(dsp->noutbufs[dst_cid] && !dsp->ninbufs[dst_cid])
        {
            dsp->thread_args[0] = dst_cid;
            dsp->thread_args[1] = (unsigned int)dsp;

            /* Start processing thread */
            xos_thread_create(&dsp->dec_thread[dst_cid], NULL, DSP_ProcessThread, (void *)dsp->thread_args, "DSP_ProcessThread", dec_stack[dst_cid],
                          STACK_SIZE_COMP, 5, 0, 0);
        }
        }//if(cid == src_cid)
        dst_cid_prev = dst_cid;
    }//while(nconnects);
#endif

    if(0)
    {
    xaf_format_t dec_format;
    get_dec_config(dsp->comp_codec, &dec_format);
    DSP_PRINTF("  [DSP Codec] Setting decode playback format:\r\n");
    DSP_PRINTF("  Sample rate: %d\r\n", dec_format.sample_rate);
    DSP_PRINTF("  Bit Width  : %d\r\n", dec_format.pcm_width);
    DSP_PRINTF("  Channels   : %d\r\n", dec_format.channels);

    if(dsp->ninbufs[cid] || dsp->noutbufs[cid])
    {
        dsp->thread_args[0] = cid;
        dsp->thread_args[1] = (unsigned int)dsp;

        /* Start processing thread */
        xos_thread_create(&dsp->dec_thread[cid], NULL, DSP_ProcessThread, (void *)dsp->thread_args, "DSP_ProcessThread", dec_stack[cid],
                          STACK_SIZE_COMP, 5, 0, 0);
    }
    }//if(0)

    return 0;

error_cleanup:
    ret = xaf_adev_close(dsp->audio_device, XAF_ADEV_FORCE_CLOSE);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_adev_close failure: %d\r\n", ret);
    }
    else
    {
        DSP_PRINTF("[DSP Codec] Audio device closed\r\n\r\n");
    }

    /* Return error to DSP app so it can be returned to ARM core */
    return -1;
}

int xrpm_file_dec_close(void *arg, int wake_value)
{
    XAF_ERR_CODE ret;
    int32_t exitcode, cid;
    dsp_handle_t *dsp = (dsp_handle_t *)arg;

    /* Wait for processing thread to complete before exiting. */
    for(cid=0;cid < DSP_NUM_COMP_IN_GRAPH_MAX; cid++)
    {
        xos_thread_join(&dsp->dec_thread[cid], &exitcode);
        xos_thread_delete(&dsp->dec_thread[cid]);

        /* Clean up and shut down XAF */
        if(dsp->comp[cid])
        {
            ret = xaf_comp_delete(dsp->comp[cid]);
            dsp->ncomps--;
            if (ret != XAF_NO_ERR)
            {
                DSP_PRINTF("xaf_comp_delete failure: %d\r\n", ret);
                return -1;
            }
        }

        /* Wait for buffer request thread to complete before exiting. */
        xos_thread_join(&dsp->buffer_thread[cid], &exitcode);
        xos_thread_delete(&dsp->buffer_thread[cid]);
    }

    /* Wait for buffer request thread to complete before exiting. */
    xos_thread_join(&dsp->connect_thread, &exitcode);
    xos_thread_delete(&dsp->connect_thread);

    ret = xaf_adev_close(dsp->audio_device, XAF_ADEV_NORMAL_CLOSE);
    if (ret != XAF_NO_ERR)
    {
        DSP_PRINTF("xaf_adev_close failure: %d\r\n", ret);
        return -1;
    }
    DSP_PRINTF("[DSP Codec] Audio device closed\r\n\r\n");
    memset(dsp, 0, sizeof(*dsp));

    return 0;
}


