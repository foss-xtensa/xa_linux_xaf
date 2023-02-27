
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
int xrpm_file_dec_create(dsp_handle_t *dsp, unsigned int *pCmdParams, int cid);
int xrpm_capturer_gain_renderer_init(unsigned int *pCmdParams, bool i2s);
int xrpm_comp_create(dsp_handle_t *dsp, unsigned int *pCmdParams);

/*******************************************************************************
 * Variables
 ******************************************************************************/



dsp_handle_t dsp;

//static uint8_t dsp_thread_stack[DSP_THREAD_STACK_SIZE];

int readBufferSize, writeBufferSize;
int readBufferPtr, writeBufferPtr;
int num_bytes_write, num_bytes_read;
volatile uint32_t gcid_list[DSP_NUM_COMP_IN_GRAPH_MAX];

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
    int cid=0;

    xos_mutex_create(&dsp->audioMutex, XOS_MUTEX_WAIT_PRIORITY, 0);
    
    memset((void *)&dsp->request_data, 0, sizeof(dsp->request_data));
    memset((void *)&dsp->response_data, 0, sizeof(dsp->response_data));
    
    for(cid=0; cid< DSP_NUM_COMP_IN_GRAPH_MAX;cid++)
    {
        gcid_list[cid] = 0;
        dsp->audioBuffer[cid] = circularbuf_create(AUDIO_BUFFER_SIZE);
        if (!dsp->audioBuffer[cid])
        {
            DSP_PRINTF("[DSP_XAF_Init] circularbuffer allocation failed cid[%d]\r\n", cid);
        }
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
    int cid;
    //@Temp Fix: Wait to pull data from host based on DSP request..
    while(1){
        for(cid=0;cid<DSP_NUM_COMP_IN_GRAPH_MAX;cid++)
        {
            if(!gcid_list[cid]) continue;

            /* ...output 1st then input */
            if(dsp->response_data[cid] == 1)
            {
                msg->head.type         = XRPM_MessageTypeRequest;
                msg->head.majorVersion = XRPM_VERSION_MAJOR;
                msg->head.minorVersion = XRPM_VERSION_MINOR;
            
                msg->head.category = XRPM_MessageCategory_AUDIO;
                msg->head.command  = XRPM_Command_FileDataOut;
                msg->head.cid = cid;
                *(uint32_t*)dsp->poutput_produced[cid] = dsp->buffer_out[cid].index;

	            dsp->response_data[cid] = 3; //marker to indicate that the buffer is not read from the host yet
                printf("\n[DSP0] Output buffer Ready cid:%d\n", cid);
                return;
            }
            else if(dsp->response_data[cid] == 2)
            {
                msg->head.type         = XRPM_MessageTypeRequest;
                msg->head.majorVersion = XRPM_VERSION_MAJOR;
                msg->head.minorVersion = XRPM_VERSION_MINOR;
            
                msg->head.category = XRPM_MessageCategory_AUDIO;
                //msg->head.command  = XRPM_Command_FileEnd;
                msg->head.command  = XRPM_Command_FileEndOut;
                msg->head.cid = cid;
                *(uint32_t*)dsp->poutput_produced[cid] = dsp->buffer_out[cid].index;

	            dsp->response_data[cid] = 0; //marker to indicate that the buffer is not read from the host yet
                if(!dsp->ninbufs[cid])
                {
                    gcid_list[cid] = 0;
                }
                printf("\n[DSP0] Output buffer EOF cid:%d\n", cid);
                return;
            }
        }//for(;cid;)

        for(cid=0;cid<DSP_NUM_COMP_IN_GRAPH_MAX;cid++)
        {
            if(!gcid_list[cid]) continue;

            if(dsp->request_data[cid] == 1){
                printf("\n[DSP0] Input buffer_needed cid:%d\n", cid);
                msg->head.type         = XRPM_MessageTypeRequest;
                msg->head.majorVersion = XRPM_VERSION_MAJOR;
                msg->head.minorVersion = XRPM_VERSION_MINOR;
            
                msg->head.category = XRPM_MessageCategory_AUDIO;
                msg->head.command  = XRPM_Command_FileDataIn;
                msg->head.cid = cid;
                dsp->request_data[cid] = 0;
                return;
            }
            else if(dsp->request_data[cid] == 2 && (!dsp->file_playing[cid]))
            {
                msg->head.type         = XRPM_MessageTypeRequest;
                msg->head.majorVersion = XRPM_VERSION_MAJOR;
                msg->head.minorVersion = XRPM_VERSION_MINOR;
            
                msg->head.category = XRPM_MessageCategory_AUDIO;
                msg->head.command  = XRPM_Command_FileEnd;
                msg->head.cid = cid;
                dsp->request_data[cid] = 0;
                if(dsp->eofOutput[cid] || !dsp->noutbufs[cid])
                {
                    gcid_list[cid] = 0;
                }
                printf("\n[DSP0] Input buffer_needed EOF cid:%d\n", cid);
                return;
            }
            else if(dsp->request_data[cid] == 3)
            {
                msg->head.type         = XRPM_MessageTypeRequest;
                msg->head.majorVersion = XRPM_VERSION_MAJOR;
                msg->head.minorVersion = XRPM_VERSION_MINOR;
            
                msg->head.category = XRPM_MessageCategory_AUDIO;
                msg->head.command  = XRPM_Command_FileStart;
                msg->head.cid = cid;
                dsp->request_data[cid] = 0;
                printf("\n[DSP0] Input buffer_needed FileStart cid:%d\n", cid);
                return;
            }
        }//for
        xos_thread_sleep_msec(5);
    }//while(1)
}
static int handleMSG_AUDIO(dsp_handle_t *dsp, xrpm_message *msg)
{
    int i, cid = msg->head.cid; /* head.cid should be 0 for CompCreate */

    for (i=0;i<DSP_NUM_COMP_IN_GRAPH_MAX;i++)
    {
        if(!gcid_list[i]) continue;
	    if(dsp->response_data[i] == 3) 
        {
            dsp->response_data[i] = 0; //reset the marker to indicate that the output buffer is read
            dsp->buffer_out[i].index = 0;
        }                                                                                                                                
    }

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
            msg->param[10] = msg->param[PARAM_INDEX_IN_BUF_OFFSET];
            msg->param[11] = msg->param[PARAM_INDEX_IN_BUF_SIZE];
            msg->param[12] = msg->param[PARAM_INDEX_IN_EOF];
            msg->param[13] = msg->param[PARAM_INDEX_OUT_BUF_OFFSET];
            msg->param[14] = msg->param[PARAM_INDEX_OUT_BUF_SIZE];
            msg->param[15] = msg->param[PARAM_INDEX_COMP_SAMPLE_RATE];
            msg->param[16] = msg->param[PARAM_INDEX_COMP_CHANNELS];
            msg->param[17] = msg->param[PARAM_INDEX_COMP_PCM_WIDTH];
#ifdef DEBUG_LOG
            DSP_PRINTF(
                "Input buffer addr 0x%X, size %d, output buffer addr 0x%X, size %d, sampling rate %d, num \
				of channels %d, pcm sample width %d, gain control index %d\r\n",
                msg->param[PARAM_INDEX_IN_BUF_OFFSET], msg->param[PARAM_INDEX_IN_BUF_SIZE], msg->param[PARAM_INDEX_OUT_BUF_OFFSET], msg->param[PARAM_INDEX_OUT_BUF_SIZE], msg->param[PARAM_INDEX_COMP_SAMPLE_RATE], msg->param[PARAM_INDEX_COMP_CHANNELS], msg->param[PARAM_INDEX_COMP_PCM_WIDTH],
                msg->param[PARAM_INDEX_COMP_PCM_GAIN_INDEX]);
#endif
            if ((msg->param[PARAM_INDEX_IN_BUF_OFFSET] == 0) || (msg->param[PARAM_INDEX_IN_BUF_SIZE] == 0) || (msg->param[PARAM_INDEX_OUT_BUF_OFFSET] == 0) || (msg->param[PARAM_INDEX_OUT_BUF_SIZE] == 0) ||
                (msg->param[PARAM_INDEX_COMP_SAMPLE_RATE] == 0) || (msg->param[PARAM_INDEX_COMP_CHANNELS] == 0) || (msg->param[PARAM_INDEX_COMP_PCM_WIDTH] == 0))
            {
                msg->head.type = XRPM_MessageTypeNotification;
                msg->error     = XRPM_Status_InvalidParameter;
            }
            else
            {
				dsp->eof[cid] = 1;//we pass whole buffer
                msg->error = xrpm_pcm_gain(dsp, (unsigned int *)&msg->param[PARAM_INDEX_IN_BUF_OFFSET]);
            }
            break;

        case XRPM_Command_CompConnect:
            if ((msg->param[PARAM_INDEX_IN_BUF_OFFSET] == 0) || (msg->param[PARAM_INDEX_IN_BUF_SIZE] == 0) || msg->head.cid)
            {
                msg->head.type = XRPM_MessageTypeNotification;
                msg->error     = XRPM_Status_InvalidParameter;
            }
            /* Check to see if file already playing */
            else
            {
                dsp->connect_info[0] = msg->param[PARAM_INDEX_CONNECT_NCONNECTS];
                dsp->connect_info[1] = 2; //connect_info_offset
                memcpy(&dsp->connect_info[2], &msg->param[PARAM_INDEX_CONNECT_COMP_ID_SRC], sizeof(msg->param[PARAM_INDEX_IN_BUF_OFFSET])*5*msg->param[PARAM_INDEX_CONNECT_NCONNECTS]);
                msg->error = 0;

                wait_msg_request_data(dsp, msg);
            }    
            break;

        case XRPM_Command_CompCreate:
            /* Param 0 File buffer */
            /* Param 1 File buffer size */
            if ((msg->param[PARAM_INDEX_IN_BUF_OFFSET] == 0) || (msg->param[PARAM_INDEX_IN_BUF_SIZE] == 0) || msg->head.cid)
            {
                msg->head.type = XRPM_MessageTypeNotification;
                msg->error     = XRPM_Status_InvalidParameter;
            }
            /* Check to see if file already playing */
            else
            {
                cid = msg->param[PARAM_INDEX_COMP_ID];
                if (dsp->file_playing[cid])
                {
                    msg->error = XRPM_Status_InvalidState;
                }
                else
                {
                    circularbuf_clear(dsp->audioBuffer[cid]);
                    dsp->buffer_out[cid].data = (char*)((unsigned int)msg + msg->param[PARAM_INDEX_OUT_BUF_OFFSET]); /* ...start of buffer, offset adjusted */
	                dsp->response_data[cid] = 0;
	                dsp->request_data[cid] = 0;
                    msg->param[PARAM_INDEX_OUT_BYTES_PRODUCED] = 0;
                    dsp->eofOutput[cid] = 0;

                    msg->error = xrpm_comp_create(dsp, (unsigned int *)&msg->param[PARAM_INDEX_IN_BUF_OFFSET]);

                    if(msg->param[PARAM_INDEX_COMP_NUM_INBUF] || msg->param[PARAM_INDEX_COMP_NUM_OUTBUF]) 
                    {
                        gcid_list[cid] = 1;
                    }
                }
            }           
            break;

        case XRPM_Command_FileStart:
            /* Param 0 File buffer */
            /* Param 1 File buffer size */
            if ((msg->param[PARAM_INDEX_IN_BUF_OFFSET] == 0) || (msg->param[PARAM_INDEX_IN_BUF_SIZE] == 0))
            {
                msg->head.type = XRPM_MessageTypeNotification;
                msg->error     = XRPM_Status_InvalidParameter;
            }
            /* Check to see if file already playing */
#if 0
            else if (dsp->file_playing[cid])
            {
                msg->error = XRPM_Status_InvalidState;
            }
#endif
            else
            {                
                /* Write initial data chunk to the circularbuffer */
                DSP_AudioWriteRing(dsp, (char *)msg->param[PARAM_INDEX_IN_BUF_OFFSET], msg->param[PARAM_INDEX_IN_BUF_SIZE], cid);
                /* Initialize pipeline */
                dsp->eof[cid] = msg->param[PARAM_INDEX_IN_EOF];
                dsp->ipc_waiting = false;
	            dsp->response_data[cid] = 0;
	            dsp->request_data[cid] = 0;
                dsp->eofOutput[cid] = 0;
                msg->error = xrpm_file_dec_create(dsp, (unsigned int *)&msg->param[PARAM_INDEX_IN_BUF_OFFSET], cid);
                
                wait_msg_request_data(dsp, msg);
            }           
            break;
        case XRPM_Command_FileDataIn:
            /* Param 0 File buffer */
            /* Param 1 File buffer size */
            /* Param 2 File EOF status (boolean true/false) */			
            if (msg->param[PARAM_INDEX_IN_BUF_OFFSET] == 0)
            {				
                msg->head.type = XRPM_MessageTypeNotification;
                msg->error     = XRPM_Status_InvalidParameter;
            }
            else
            {				
                DSP_AudioWriteRing(dsp, (char *)msg->param[PARAM_INDEX_IN_BUF_OFFSET], msg->param[PARAM_INDEX_IN_BUF_SIZE], cid);
                /* Signal EOF to processing thread if set by remote */
                if (msg->param[PARAM_INDEX_IN_EOF])
                {
										
                    dsp->eof[cid] = true;
					DSP_SendFileEnd(dsp, cid);
                }
                wait_msg_request_data(dsp, msg);
            }

            dsp->ipc_waiting = false;            
            
            
            break;	

        case XRPM_Command_FileDataOut:
            /* Param 3 File buffer */
            /* Param 4 File buffer size */
            if (msg->param[PARAM_INDEX_OUT_BUF_OFFSET] == 0)
            {
                msg->head.type = XRPM_MessageTypeNotification;
                msg->error     = XRPM_Status_InvalidParameter;
            }
            else
            {
                wait_msg_request_data(dsp, msg);
            }
            break;	

		case XRPM_Command_FileStop:
            /* File must be already playing */
            if (!dsp->file_playing[cid])
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

    
	/* Process request */
    status = DSP_MSG_Process(&dsp, msg);

    return 0;
}

