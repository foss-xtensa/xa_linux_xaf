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
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include "xrp_api.h"
#include "xrpm_msg.h"
#include "host_main.h"
#include "hihat_pcm.h"
#include "string.h"
#include "hihat_ref_pcm_gain.h"

#define CHECK_ERROR(api) \
    do { \
      (api); \
      assert(status == XRP_STATUS_SUCCESS); \
      status = -1; \
    } while (0)

/*
 * List of commands supported.
 */
void commandMenu(void)
{
    //help command
    fprintf(stdout,"User commands:\n\n");
    
    fprintf(stdout, "help      : List of all commands,\n");
    fprintf(stdout, "exit      : Exit both host and dsp programs,\n");
    fprintf(stdout, "version   : Query DSP for XAF/Vorbis Library version,\n");
    fprintf(stdout, "gain      : Run PCM gain on DSP with I/O buffers from host,\n");
    fprintf(stdout, "file      : Perform audio file decode and playback on DSP,\n");
    fprintf(stdout, "            USAGE  :file [list|stop|<audio_file>],\n");
    fprintf(stdout, "                    file  list <path>    :  List audio files from the specified directory\n");
    fprintf(stdout, "                                         :  By default if no path is provided, it list files from ../test/test_inp/ directory \n");
    fprintf(stdout, "                    file  <audio_file>   :  Decode the audio_file and write into file output \n");
    fprintf(stdout, "                    file  stop           :  Stop file decoding  \n");  
    fprintf(stdout, ">>");  
    fflush(stdout);
}

/*
 * custom string copy avoiding line feed.
 */
void strcpy_lf(char *s, char *t)
{
    while ((*t != '\n'))
    {
         *s = *t;
          s++;
          t++;
    }
}

void setup_filetype(const char *dot,formatCommand *format_command)
{
    if(!(dot && strncmp(dot + 1, "mp3", 3)))
    {
        format_command->file_type = DSP_COMPONENT_MP3;
        printf("File format: MP3\n>>");
        fflush(stdout);
    }
    else if(!(dot && strncmp(dot + 1, "aac", 3)))
    {
        format_command->file_type = DSP_COMPONENT_AAC;
        printf("File format: AAC\n>>");
        fflush(stdout);
    }
    else if(!(dot && strncmp(dot + 1, "ogg", 3)))
    {
        format_command->file_type = DSP_COMPONENT_VORBIS;        
        printf("File format: VORBIS\n>>");
        fflush(stdout);
    }
    else
    {
        format_command->file_type = -1;//Invalid file format. 
                                       //Control should not come here
    }				
}
/*
 * Parse and decode user input from command line args.
 */
int shellParseLine(struct hostArgs *cmd_args, formatCommand *format_command)
{
    int result=0;
    DIR *d;
    struct dirent *dir;
    const char *dot;
    int count=0;
    char cwd[USERINPUT_MAX_SIZE];

    if (strcmp(cmd_args->argv[COMMAND_INDEX], "gain") == 0)
    {
        format_command->argcount = cmd_args->argc;
        format_command->cmd = HOST_CMD_GAIN;
    }
    else if (strcmp(cmd_args->argv[COMMAND_INDEX], "version") == 0)
    {
        format_command->argcount = cmd_args->argc;
        format_command->cmd = HOST_CMD_VERSION;
    }
    else if(strcmp(cmd_args->argv[COMMAND_INDEX], "help") == 0)
    {
        commandMenu();
        format_command->cmd = HOST_CMD_HELP;
    }
    else if(strcmp(cmd_args->argv[COMMAND_INDEX], "exit") == 0)
    {
        format_command->argcount = cmd_args->argc;
        format_command->cmd = HOST_CMD_EXIT;
    }
    else if((strcmp(cmd_args->argv[COMMAND_INDEX], "file") == 0) &&
            (cmd_args->argc > 2) && (cmd_args->argc <= 4))
    {
        if ((strcmp(cmd_args->argv[2], "list") == 0))
        {
            format_command->argcount = cmd_args->argc;
            format_command->cmd = HOST_CMD_LIST;
           
            if(getcwd(cwd,sizeof(cwd)) == NULL){
                perror("getcwd() error");
                return 1;
            }
                     
            if ( cmd_args->argc == 4 )
            {
                int err = chdir(cmd_args->argv[3]);
                if ( err == -1 )
                {
                    printf(" Invalid file list path \r\n");
                    return 1;
                }
            }
            else
            {
                chdir("../test/test_inp/");
            }

            d=opendir(".");
            if(d==NULL)
            {
                return 1; // if was not able return
            }

            if(d)
            {
                printf("Available audio files:\r\n");
                while((dir = readdir(d)) != NULL)
                {
                    /* Skip root directory and directories*/
                    if( strcmp(dir->d_name,".")!=0 && strcmp(dir->d_name,"..")!=0)
                    {
                        /* Check file for supported audio extension */
                        dot = strrchr(dir->d_name, '.');
                        if ((dot && strncmp(dot + 1, "mp3", 3) == 0) ||
                            (dot && strncmp(dot + 1, "aac", 3) == 0) ||
                            (dot && strncmp(dot + 1, "ogg", 3) == 0))
                        {
                            count++;
                            printf("File %d:: %s\n",count,dir->d_name);
                        }
                    }
                }//end while
                closedir(d);//Close directory handle
                if (!count)
                {
                    printf("  (No supported files available)\r\n");
                }
                printf("\n>>");
                fflush(stdout);
            }
            
            chdir(cwd);
            
            if ( getcwd(cwd, sizeof(cwd)) == NULL ) 
            {
                perror("getcwd() error");
                return 1;
            }            
            
        }
        else if ((strcmp(cmd_args->argv[2], "stop") == 0))
        {
            format_command->argcount = cmd_args->argc;
            format_command->cmd = HOST_CMD_STOP;
            if(!file_playing)
            {
                printf("No file is playing to STOP.\n");
                return 1;
            }
        }
        else
        {
            if(( access(cmd_args->argv[2], F_OK ) != -1))
            {
                dot = strrchr(cmd_args->argv[2], '.');
                if ((dot && strncmp(dot + 1, "mp3", 3) != 0) &&
                    (dot && strncmp(dot + 1, "aac", 3) != 0) &&
                    (dot && strncmp(dot + 1, "ogg", 3) != 0))
                {
                    printf("Wrong user input file :: %s :: Type --help-- for more info!!\n",cmd_args->argv[2]);
                    return 1;
                }
                else if(!dot)
                {
                    printf("Wrong user input file :: %s :: Type --help-- for more info!!\n",cmd_args->argv[2]);
                    return 1;
                }
                else
                {
                    setup_filetype(dot,format_command);
                }
                strcpy(format_command->file_path,cmd_args->argv[2]);//Copy hardcoded path to file_path
            }
            else
            {
                dot = strrchr(cmd_args->argv[2], '.');
                if ((dot && strncmp(dot + 1, "mp3", 3) != 0) &&
                    (dot && strncmp(dot + 1, "aac", 3) != 0) &&
                    (dot && strncmp(dot + 1, "ogg", 3) != 0))
                {
                    printf("Wrong user input file :: %s :: Type --help-- for more info!!\n",cmd_args->argv[2]);
                    return 1;
                }
                else if(!dot)
                {
                    printf("Wrong user input file :: %s :: Type --help-- for more info!!\n",cmd_args->argv[2]);
                    return 1;
                }
                else
                {
                    setup_filetype(dot,format_command);            
                }
                if(getcwd(cwd,sizeof(cwd)) != NULL){
                    //printf("current working dir: %s\n",cwd);
                }
                else
                {
                    perror("getcwd() error");
                    return 1;
                }
                chdir("../test/test_inp/");

                if(getcwd(cwd,sizeof(cwd)) != NULL){
                    //printf("current working dir: %s\n",cwd);
                }
                else
                {
                    perror("getcwd() error");
                    return 1;
                }
                strcat(cwd,"/");
                strcat(cwd,cmd_args->argv[2]);
                strcpy(format_command->file_path,cwd);

                if(!( access(format_command->file_path, F_OK ) != -1))
                {
                    // file doesn't exist
                    printf("File does not exist :: %s :: Type --help-- for more info!!\n",cmd_args->argv[2]);
                    return 1;
                }
                chdir("../../build/");//Getting back to build directory
                if(getcwd(cwd,sizeof(cwd)) != NULL){
                    //printf("LINE:: %d current working dir: %s\n",__LINE__,cwd);
                }
                else
                {
                    perror("getcwd() error");
                    return 1;
                }
            }

            format_command->argcount = cmd_args->argc;
            format_command->cmd = HOST_CMD_FILE;
        }
    }
    else
    {
        result = 1;//Set if error
    }

    return(result);
}

/*
 * Shell thread listening for user and console commands.
 */
void *shell_thread(void *args)
{
    int result;
    char user_input[USERINPUT_MAX_SIZE];
    hostArg cmd_args = *(hostArg *)args;
    formatCommand format_command = {0};

    while(1)
    {
        if(cmd_args.argc>1)
        {
            result = shellParseLine(&cmd_args, &format_command);
            cmd_args.argc = 0;

            if(!result) //check if decodecommand failed
            {
                if((format_command.cmd != HOST_CMD_HELP) && (format_command.cmd != HOST_CMD_LIST))
                {
                    result = write(shell_host_pipe[1],&format_command,sizeof(format_command));
                    if (result != sizeof(format_command))
                    {
                       perror ("write");
                       exit (2);
                    }
                }
                // Handling special case to exit thread after command is processed.
                memset(&format_command, 0, sizeof(format_command));

                format_command.argcount = 1;
                format_command.cmd = HOST_CMD_EXIT;
                //EXIT FOR SHELLHOST THREAD
                result = write(shell_host_pipe[1],&format_command,sizeof(format_command));
                if (result != sizeof(format_command))
                {
                   perror ("write");
                   exit (2);
                }
                //EXIT FOR HOSTSHELL THREAD
                break;//exit loop
            }
            else
            {
                printf("%s:%s:%d :: CommandLine decode failed\n",__FILE__,__func__,__LINE__);
            }
        }
        else
        {
            while(1)
            {
                //Process command user input through MENU
                memset(&cmd_args, 0, sizeof(cmd_args));
                result = shellCommand(user_input,&format_command,&cmd_args);
                if(!result) //check if decodecommand failed
                {
                    if((format_command.cmd != HOST_CMD_HELP) && (format_command.cmd != HOST_CMD_LIST))
                    {
                        result = write(shell_host_pipe[1],&format_command,sizeof(format_command));
                        if (result != sizeof(format_command))
                        {
                           perror ("write");
                           exit (2);
                        }
                    }
                    if(format_command.cmd == HOST_CMD_EXIT)
                    {
                        break;
                    }
                }
                else
                {
                    //If it comes here then wrong command
                    printf("\n>>");
                    fflush(stdout);
                }
            }
        }
        if(format_command.cmd == HOST_CMD_EXIT)
        {
            break;
        }
    }
    return 0;
}

/*
 * HostDSP thread listening for decoded commands from shell thread.
 */
void *hostdsp_thread(void* arg)
{

    fd_set set;
    struct timeval tv;
    int retval;
    int error = 0;
    struct formatCommands format_command;
    //Delay in starting the reading from the pipe
    fdmax_sh = shell_host_pipe[0];
    while(1)
    {
        int    result;
        /* Watch stdin (fd 0) to see when it has input. */
        FD_ZERO(&set);
        FD_SET(shell_host_pipe[0], &set);
        
        /* Wait up to tv_usec micro seconds. */
        tv.tv_sec = 0;
        tv.tv_usec = 1000; //0.001 seconds
        retval = select(fdmax_sh+1, &set, NULL, NULL, &tv);

        if (retval == -1) //Error in select
        {
            perror("select()");
        }
        else if (FD_ISSET(shell_host_pipe[0], &set))   //pipe is set
        {
            result = read (shell_host_pipe[0],&format_command,sizeof(format_command));
            if (result != sizeof(format_command))
            {
                perror("read mismatch");
                exit(3);
            }

            if(format_command.cmd != HOST_CMD_HELP)
            {
                error = host_dsp_start(0,format_command);
            }

            if((format_command.cmd == HOST_CMD_EXIT) || (standalone_exit))
            {
                //Exit MENU loop if EXIT command is entered.
                xrp_exit();
                break;
            }
            if (error)
            {
                printf("CTRL: Fail\n");
            }
        }

    }
    return 0;
}



/*
 * Parse and decode user input from console.
 */
int shellCommand(char user_input[],formatCommand *format_command,hostArg *args)
{
    unsigned int counter=1;
    //char *argv[SHELL_BUFFER_SIZE];
    if(!fgets(user_input, USERINPUT_MAX_SIZE, stdin))
    {
        printf("Line: %d Function: %s io error\n", __LINE__,__func__);
        return 1;
    }
    if(strlen(user_input) <= USERINPUT_MAX_SIZE)
    {
        char delim[] = " ";
        char *ptr = strtok(user_input, delim);
        while(ptr != NULL)
        {
            strcpy_lf(args->argv[counter],ptr);
            ptr = strtok(NULL, delim);
            counter++;
            args->argc = counter;
        }
    }
    else
    {
        strcpy(user_input,"invalid");
    }

    return shellParseLine(args, format_command);
}

/*
 * Initial default setting for command headers.
 */
static void initMessage(struct xrpm_message *msg)
{
    /* Common field for command */
    msg->head.type = XRPM_MessageTypeRequest;
    msg->head.majorVersion = XRPM_VERSION_MAJOR;
    msg->head.minorVersion = XRPM_VERSION_MINOR;
    msg->param[0] = 0;
    msg->param[2] = 0;
}

/*
 * To set version command settings.
 */
void shellVersion(struct xrpm_message *msg)
{
    msg->head.category = XRPM_MessageCategory_GENERAL;
    msg->head.command  = XRPM_Command_ECHO;
}

/*
 * To set gain command settings.
 */
void shellGain(struct xrpm_message *msg)
{
    int input_size = sizeof(XRPM_INPBUFFER_PCMGAIN);

    if ( input_size > AUDIO_MAX_INPUT_BUFFER )
        input_size = AUDIO_MAX_INPUT_BUFFER;

    msg->head.category = XRPM_MessageCategory_AUDIO;
    msg->head.command  = XRPM_Command_GAIN;
    /* Param 0 PCM input buffer offset*/
    /* Param 1 PCM input buffer size*/
    /* Param 2 PCM output buffer offset*/
    /* Param 3 PCM output buffer size*/
    /* Param 4 PCM sampling rate, default 44100*/
    /* Param 5 PCM number of channels, only 1 or 2 supported, default 1*/
    /* Param 6 PCM sample width, default 16*/
    /* Param 7 Gain control index, default is 4, range is 0 to 6 -> {0db, -6db, -12db, -18db, 6db, 12db, 18db}*/
    /* Param 8 return parameter, actual read bytes*/
    /* Param 9 return parameter, actual written bytes*/
    msg->param[0] = (unsigned int)( (uintptr_t)(&msg->s_audioInput[0])-(uintptr_t)(msg));
    msg->param[1] = input_size;
    msg->param[2] = (unsigned int)( (uintptr_t)(&msg->s_audioOutput[0])-(uintptr_t)(msg));
    msg->param[3] = AUDIO_MAX_OUTPUT_BUFFER;
    msg->param[4]  = 44100; //sample reate
    msg->param[5]  = 2;
    msg->param[6]  = 16;
    msg->param[7]  = GAIN_CTRL_IDX; // default 0
    memcpy(msg->s_audioInput, XRPM_INPBUFFER_PCMGAIN, input_size);
}

void shellExit(struct xrpm_message *msg)
{
    msg->head.category = XRPM_MessageCategory_GENERAL;
    msg->head.command  = XRPM_Command_EXIT;
}

void shellStop(struct xrpm_message *msg)
{
    msg->head.category = XRPM_MessageCategory_AUDIO;
    msg->head.command  = XRPM_Command_STOP;
}

void shellFileStart(struct xrpm_message *msg,formatCommand format_command)
{
    if (!file_playing) {
        size_t bytes_read;
        
#ifdef DEBUG_LOG        
        printf("\n[Host] getFileStartParams\n");
#endif      
        msg->head.category = XRPM_MessageCategory_AUDIO;
        msg->head.command = XRPM_Command_FileStart;
        /* Param 0 Encoded input buffer offset*/
        /* Param 1 Encoded input buffer size*/
        /* Param 2 EOF (true/false) */
        /* Param 3 Audio codec component type */
        msg->param[0] = (unsigned int)( (uintptr_t)(&msg->s_audioInput[0])-(uintptr_t)(msg));
        msg->param[1] = FILE_PLAYBACK_INITIAL_READ_SIZE;
        if(format_command.file_type == DSP_COMPONENT_MP3)
        {
            msg->param[3] = DSP_COMPONENT_MP3;
        }
        else if(format_command.file_type == DSP_COMPONENT_AAC)
        {
            msg->param[3] = DSP_COMPONENT_AAC;
        }
        else if(format_command.file_type == DSP_COMPONENT_VORBIS)
        {
            msg->param[3] = DSP_COMPONENT_VORBIS;
        }
        else
        {
            msg->param[3] = -1; //Control should not come here.
                                //If control visits this part then
                                //there is a problem.
        }
        /* File open in read mode*/
        fp = fopen(format_command.file_path, "rb");
        if ( fp == NULL )
        {
            printf("\n [Host-Error] : File open failed \n");
            return;
        }
        /* Seek to the beginning of the file */
        fseek(fp, 0, SEEK_SET);

        bytes_read = fread(msg->s_audioInput, 1, FILE_PLAYBACK_INITIAL_READ_SIZE, fp);
#ifdef DEBUG_LOG           
        printf("[Host]bytes_read:%ld\r\n",bytes_read);
#endif        
        /* Set EOF if file smaller than initial read block size */
        if (bytes_read < FILE_PLAYBACK_INITIAL_READ_SIZE)
        {
            msg->param[2] = 1;
            msg->param[1] = bytes_read;
        }
        file_playing = true;
    }
    else
    {
        printf("File is already playing\r\n");

    }
}

void shellFileData(struct xrpm_message *msg,int stop_flag)
{
    if (file_playing)
    {
        size_t bytes_read;
#ifdef DEBUG_LOG         
        printf("\n[Host]getFileDataParams\n");
#endif        
        msg->head.category = XRPM_MessageCategory_AUDIO;
        msg->head.command = XRPM_Command_FileData;
        /* Param 0 Encoded input buffer offset*/
        /* Param 1 Encoded input buffer size*/
        /* Param 2 EOF (true/false) */
        /* Param 3 Audio codec component type */
        msg->param[0] = (unsigned int)( (uintptr_t)(&msg->s_audioInput[0])-(uintptr_t)(msg));
        msg->param[1] = FILE_PLAYBACK_INITIAL_READ_SIZE;
        msg->param[2] = 0;
        bytes_read = fread(msg->s_audioInput, 1, FILE_PLAYBACK_INITIAL_READ_SIZE, fp);
#ifdef DEBUG_LOG                     
        printf("[Host]bytes_read:%ld\r\n",bytes_read);
#endif        
        if ( (bytes_read == 0) || (bytes_read < FILE_PLAYBACK_INITIAL_READ_SIZE) || (stop_flag == HOST_CMD_STOP))
        {
#ifdef DEBUG_LOG             
            printf("[Host]File read End\r\n");
#endif            
            /* Set EOF param if final segment of file is sent. */
            msg->param[2] = 1;
            if(stop_flag == HOST_CMD_STOP)
            {
                bytes_read = 0;
            }
           // *notify_shell = false;
        }
        msg->param[1] = bytes_read;

    }
    else
    {
        printf("[Host]File is not started\r\n");

    }
}


/*
 * Set Headers and params for specific codec.
 */
void handleShellCommand(struct xrpm_message *msg,formatCommand format_command)
{
    switch(format_command.cmd)
    {
        case HOST_CMD_GAIN:
        {
            shellGain(msg);
            break;
        }
        case HOST_CMD_VERSION:
        {
            shellVersion(msg);
            break;
        }
        case HOST_CMD_EXIT:
        {
            shellExit(msg);
            break;
        }
        case HOST_CMD_STOP:
        {
            shellStop(msg);
            break;
        }
        case HOST_CMD_FILE:
        {
            shellFileStart(msg,format_command);
            break;
        }
        case HOST_CMD_HELP:
            break;
        case HOST_CMD_LIST:
            break;
        default:
            printf("HOST :: Command not supported::%d",format_command.cmd);
    }
}


/*
 * Start building message to pass on DSP side.
 */
int host_dsp_start(int devid,formatCommand format_command)
{
    struct xrp_device *device;
    struct xrp_queue *queue;
    struct xrp_buffer_group *group;
    struct xrp_buffer *buf;
    struct xrpm_message *msg;
    enum xrp_status status = -1;
    int cmd = 0xdeadbeef;
    int error = 0;
    int i;
    struct formatCommands fCommand_MenuMode;
    exit_flag = false;
    //struct callBacks cbHostShell;
    device = xrp_open_device(devid, &status);
    assert(status == XRP_STATUS_SUCCESS);
    status = -1;
    queue = xrp_create_queue(device, &status);
    assert(status == XRP_STATUS_SUCCESS);
    status = -1;

    group = xrp_create_buffer_group(&status);
    assert(status == XRP_STATUS_SUCCESS);
    status = -1;

    buf = xrp_create_buffer(device, sizeof(struct xrpm_message), NULL, &status);
    assert(status == XRP_STATUS_SUCCESS);
    status = -1;

    msg = (struct xrpm_message*)xrp_map_buffer(buf, 0, sizeof(struct xrpm_message), XRP_READ_WRITE, &status);
    assert(status == XRP_STATUS_SUCCESS);
    status = -1;

    initMessage(msg);

    handleShellCommand(msg,format_command);

    xrp_unmap_buffer(buf, msg, &status);
    assert(status == XRP_STATUS_SUCCESS);
    status = -1;

    xrp_add_buffer_to_group(group, buf, XRP_READ_WRITE, &status);
    assert(status == XRP_STATUS_SUCCESS);
    status = -1;
    cmd = sizeof(struct xrpm_message);
    xrp_run_command_sync(queue, &cmd, sizeof(cmd), NULL, 0, group, &status);
    assert(status == XRP_STATUS_SUCCESS);

    status = -1;
    msg = (struct xrpm_message*)xrp_map_buffer(buf, 0, sizeof(struct xrpm_message), XRP_READ_WRITE, &status);
    assert(status == XRP_STATUS_SUCCESS);

    do
    {
        /* Process return data from DSP*/
        switch (msg->head.category)
        {
            case XRPM_MessageCategory_GENERAL:
                switch (msg->head.command)
                {
                    /* echo returns version info of key components*/
                    case XRPM_Command_ECHO:
                        fprintf(stdout, "Linux + XTSC HiFix with XAF version : %d.%d\n",HOST2DSP_MAJOR_VERSION,HOST2DSP_MINOR_VERSION);
                        fprintf(stdout, "Component versions from DSP:\n");
                        fprintf(stdout, "Audio Framework version %d.%d \n", msg->param[0] >> 16, msg->param[0] & 0xFF);
                        fprintf(stdout, "Audio Framework API version %d.%d\n", msg->param[1] >> 16, msg->param[1] & 0xFF);
                        fprintf(stdout, "VORBIS Decoder Lib version %d.%d\n", msg->param[2] >> 16, msg->param[2] & 0xFF);
                        fprintf(stdout, "\n>>");
                        fflush(stdout);
                        break;
                    case XRPM_Command_EXIT:
                        printf("HOST :: EXIT COMMAND ACK FROM DSP:\r\n");
                        break;
                    default:
                        printf("Incoming unknown message command %d from category %d \r\n", msg->head.command,
                               msg->head.category);
                }
                break;
            case XRPM_MessageCategory_AUDIO:
                switch (msg->head.command)
                {

                    case XRPM_Command_GAIN:
                    {
                        int tot_size = sizeof(XRPM_REFBUFFER_PCM_GAIN);
                        unsigned char *pOut = (unsigned char *)msg->s_audioOutput;

                        int failed = 0;
                        for( i = 0; i < tot_size; i++) {
                            if ( pOut[i] != XRPM_REFBUFFER_PCM_GAIN[i] )
                            {
                                failed = 1;
                            }
                        }

                        if ( failed ) {
                            printf("\nFAILED - Output mismatches with reference \n\n>>");
                        }
                        else {
                            printf("\nPASSED - Output matches with reference \n\n>>");                            
                        }
                        fflush(stdout);
                    }
                        break;
                   case XRPM_Command_FileStart:
                        if (msg->error != XRPM_Status_Success)
                        {
                            printf("DSP file playback start failed! return error = %d\r\n", msg->error);
                            file_playing = false;
                        }
                        else
                        {
                            printf("[Host]DSP file playback start\r\n");
                        }

                        break;

                    case XRPM_Command_FileData:
                    {
                        int result = 0;

                        struct timeval tv2;
             
                        fd_set rfds2;
                        FD_ZERO(&rfds2);
                        FD_SET(shell_host_pipe[0], &rfds2);
                        tv2.tv_sec = 0;
                        tv2.tv_usec = 10;
                        memset(&fCommand_MenuMode, 0, sizeof(formatCommand));

                        int retval = select(fdmax_sh+1, &rfds2, NULL, NULL, &tv2);
                        if (retval == -1) //Error in select
                        {
                            perror("select()");
                        }
                        else if (FD_ISSET(shell_host_pipe[0], &rfds2))   //pipe is set
                        {
                            result = read (shell_host_pipe[0],&fCommand_MenuMode,sizeof(fCommand_MenuMode));

                            if (result != sizeof(fCommand_MenuMode))
                            {
                                perror("read mismatch");
                                exit(3);
                            }
                        }
                        else
                        {
                            //printf ("Line:%d Function:%s No Data since YY:0 seconds\n",__LINE__,__func__);
                        }

                        shellFileData(msg,fCommand_MenuMode.cmd);
                        if(fCommand_MenuMode.cmd == HOST_CMD_STOP)
                        {
                            printf("STOP Command Success \n");
                        }
                        if(fCommand_MenuMode.cmd == HOST_CMD_EXIT)
                        {
                            standalone_exit = 1;
                        }
                        xrp_unmap_buffer(buf, msg, &status);
                        assert(status == XRP_STATUS_SUCCESS);
                        status = -1;

                        cmd = sizeof(struct xrpm_message);
                        xrp_run_command_sync(queue, &cmd, sizeof(cmd), NULL, 0, group, &status);
                        assert(status == XRP_STATUS_SUCCESS);

                        status = -1;
                        msg = (struct xrpm_message*)xrp_map_buffer(buf, 0, sizeof(struct xrpm_message), XRP_READ_WRITE, &status);
                        assert(status == XRP_STATUS_SUCCESS);
                        if(msg->head.command == XRPM_Command_FileEnd)
                        {
                            printf("\nDSP file playback complete\r\n\n>>");
                            fflush(stdout);
                            file_playing = false;
                            result = fclose(fp);
                            if (result)
                            {
                                printf("Failed to close file\r\n");
                            }
                        }
                        break;
                    }
                    case XRPM_Command_FileEnd:
                    {
                        int result;
                        printf("DSP file playback complete\r\n\n>>");
                        fflush(stdout);
                        file_playing = false;
                        result = fclose(fp);
                        if (result)
                        {
                            printf("Failed to close file\r\n");
                        }
                        printf("\n\n File end and exit flag is SET!! \n\n");
                        break;
                    }
                    default:
                        printf("Incoming unknown message command %d from category %d \r\n", msg->head.command,
                               msg->head.category);
                }
                break;
            default:
                printf("Incoming unknown message command %d from category %d \r\n", msg->head.command,
                msg->head.category);
        }
    }while((msg->head.command == XRPM_Command_FileData));
    xrp_release_buffer_group(group);
    xrp_release_buffer(buf);
    xrp_release_queue(queue);
    xrp_release_device(device);

    return error;
}


//three host threads to handle shell command,dsp communication and ack thread 
#define NUM_THREADS 2  

int main(int argc, char **argv)
{
    int                i;
    pthread_t        thread_id[NUM_THREADS];

    fprintf(stdout, "\n==============================================================================\n");    
    fprintf(stdout, "\nLinux + XTSC HiFix with XAF version\t: %d.%d\r\n",HOST2DSP_MAJOR_VERSION, HOST2DSP_MINOR_VERSION);
    fprintf(stdout, "Host process id\t\t\t\t: %ld \n\n", (long)getpid() );
    fprintf(stdout, "==============================================================================\n");
    
    if (argc == 1)
    {
        commandMenu();
    }
    
    hostArg     cmd_args = {0};
    cmd_args.argc = argc;
    
    for( i = 0; i < argc; i++) {
        strcpy(cmd_args.argv[i], argv[i]); // valid
    }
    
    /*Create shell pipe*/
    if( pipe(shell_host_pipe) == -1 ) {
        printf("Pipe creation Failed\n" );
        return 0; //Error, quit
    }

    pthread_create(&thread_id[0], NULL, shell_thread, &cmd_args);
    pthread_create(&thread_id[1], NULL, hostdsp_thread, NULL);

    if( pthread_join(thread_id[0], NULL) != 0 )
        printf("can′t join with thread_id[0]");

    if( pthread_join(thread_id[1], NULL) != 0 )
        printf("can′t join with thread_id[1]");
    
    return 0;
}
