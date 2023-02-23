
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

/* ...debugging facility */
#include "xf-debug.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define DSP_EVENT_STOP (1 << 0)

#define DSP_NUM_COMP_IN_GRAPH_MAX   16
#define STACK_SIZE_COMP (4 * 1024)

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
    void *comp[DSP_NUM_COMP_IN_GRAPH_MAX];
    XosThread dec_thread[DSP_NUM_COMP_IN_GRAPH_MAX];
    XosThread buffer_thread[DSP_NUM_COMP_IN_GRAPH_MAX];
    XosThread cleanup_thread;
    XosThread connect_thread;
    XosEvent pipeline_event;

    /* Audio buffer read/write function pointers for processing thread */
    uint32_t (*audio_read)(struct _dsp_handle_t *dsp, char *data, uint32_t size, uint32_t cid);
    uint32_t (*audio_write)(struct _dsp_handle_t *dsp, char *data, uint32_t size, uint32_t cid);

    /* Memory buffer playback variables */
    dsp_buffer_t buffer_in[DSP_NUM_COMP_IN_GRAPH_MAX];
    dsp_buffer_t buffer_in2;
    dsp_buffer_t buffer_out[DSP_NUM_COMP_IN_GRAPH_MAX];

    /* File playback circular buffer variables */
    circular_buf_t *audioBuffer[DSP_NUM_COMP_IN_GRAPH_MAX];
    XosMutex audioMutex;
    XosMutex rpmsgMutex;

    /* File playback state management variables */
    volatile bool eof[DSP_NUM_COMP_IN_GRAPH_MAX];
    volatile bool ipc_waiting;
    volatile bool file_playing[DSP_NUM_COMP_IN_GRAPH_MAX];
    
    volatile int request_data[DSP_NUM_COMP_IN_GRAPH_MAX];
    volatile int response_data[DSP_NUM_COMP_IN_GRAPH_MAX];
    volatile bool eofOutput[DSP_NUM_COMP_IN_GRAPH_MAX];

    volatile void *pinput_consumed[DSP_NUM_COMP_IN_GRAPH_MAX];
    volatile void *poutput_produced[DSP_NUM_COMP_IN_GRAPH_MAX];

    int cid[DSP_NUM_COMP_IN_GRAPH_MAX];
    int ninbufs[DSP_NUM_COMP_IN_GRAPH_MAX];
    int noutbufs[DSP_NUM_COMP_IN_GRAPH_MAX];
    void *dec_inbuf[DSP_NUM_COMP_IN_GRAPH_MAX][2];
    int connect_info[5*DSP_NUM_COMP_IN_GRAPH_MAX + 2]; /* ...[0] number of connects, [1] current connect info, [2..6] 5 connect elements per connect */
    int comp_thread_state[DSP_NUM_COMP_IN_GRAPH_MAX];   /* ... to prevent recreating a duplicate thread if in a generic loop */
    unsigned int thread_args[2]; /* ...to prevent stack variable access after the parent thread creates child thread and exits */

    int ncomps;
} dsp_handle_t;

typedef struct xaf_format_s {
    uint32_t             sample_rate;
    uint32_t             channels;
    uint32_t             pcm_width;
    uint32_t             input_length;
    uint32_t             output_length;
    uint64_t             output_produced;
} xaf_format_t;
/*******************************************************************************
 * API Function Prototypes
 ******************************************************************************/
void *DSP_Malloc(int32_t size, int32_t id);
void DSP_Free(void *ptr, int32_t id);

uint32_t DSP_AudioRead(dsp_handle_t *dsp, char *data, uint32_t size, uint32_t cid);
uint32_t DSP_AudioWrite(dsp_handle_t *dsp, char *data, uint32_t size, uint32_t cid);
uint32_t DSP_AudioReadRing(dsp_handle_t *dsp, char *data, uint32_t size, uint32_t cid);
uint32_t DSP_AudioWriteRing(dsp_handle_t *dsp, char *data, uint32_t size, uint32_t cid);

int DSP_ProcessThread(void *arg, int wake_value);
int DSP_BufferThreadInput(void *arg, int wake_value);
void DSP_SendFileEnd(dsp_handle_t *dsp, uint32_t cid);
int DSP_ConnectThread(void *arg, int wake_value);

#ifndef XA_DISABLE_EVENT
extern uint32_t g_enable_error_channel_flag;
#endif

#ifndef XA_DISABLE_EVENT
#define COMP_CREATE_API(p_adev, pp_comp, _comp_id, _num_input_buf, _num_output_buf, _pp_inbuf, _comp_type) ({\
        XAF_ERR_CODE __ret;\
        xaf_comp_config_t comp_config;\
        xaf_comp_config_default_init(&comp_config);\
		comp_config.error_channel_ctl = g_enable_error_channel_flag;\
		comp_config.comp_id = _comp_id;\
		comp_config.comp_type = _comp_type;\
		comp_config.num_input_buffers = _num_input_buf;\
		comp_config.num_output_buffers = _num_output_buf;\
		comp_config.pp_inbuf = (pVOID (*)[XAF_MAX_INBUFS])_pp_inbuf;\
        __ret = xaf_comp_create(p_adev, pp_comp, &comp_config);\
        __ret;\
    })

#else 

#define COMP_CREATE_API(p_adev, pp_comp, _comp_id, _num_input_buf, _num_output_buf, _pp_inbuf, _comp_type) ({\
        XAF_ERR_CODE __ret;\
        xaf_comp_config_t comp_config;\
        xaf_comp_config_default_init(&comp_config);\
		comp_config.comp_id = _comp_id;\
		comp_config.comp_type = _comp_type;\
		comp_config.num_input_buffers = _num_input_buf;\
		comp_config.num_output_buffers = _num_output_buf;\
		comp_config.pp_inbuf = (pVOID (*)[XAF_MAX_INBUFS])_pp_inbuf;\
        __ret = xaf_comp_create(p_adev, pp_comp, &comp_config);\
        __ret;\
    })
#endif

#endif /* __XRPM_UTILS_H__ */
