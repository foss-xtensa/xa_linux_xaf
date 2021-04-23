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
/* File contains constants shared between AP and DP sides */

#define XF_CFG_CORES_NUM_DSP                1

/* DSP object sizes */

#if defined(HAVE_FREERTOS)
#define XF_DSP_OBJ_SIZE_CORE_DATA           412
#define XF_DSP_OBJ_SIZE_DSP_LOCAL_POOL      100
#define XF_DSP_OBJ_SIZE_CORE_RO_DATA        256
#define XF_DSP_OBJ_SIZE_CORE_RW_DATA        256
#elif defined(HAVE_XOS)
#define XF_DSP_OBJ_SIZE_CORE_DATA           552
#define XF_DSP_OBJ_SIZE_DSP_LOCAL_POOL      216
#define XF_DSP_OBJ_SIZE_CORE_RO_DATA        256
#define XF_DSP_OBJ_SIZE_CORE_RW_DATA        256
#else
#error Unrecognized OS
#endif


/*******************************************************************************
 * Global configuration parameters (changing is to be done carefully)
 ******************************************************************************/

/* ...maximum in ports for mimo class */
#define XF_CFG_MAX_IN_PORTS             4

/* ...maximum out ports for mimo class */
#define XF_CFG_MAX_OUT_PORTS            4

/* ...maximal size of scratch memory is 56 KB */
#define XF_CFG_CODEC_SCRATCHMEM_SIZE    (56 << 10)

