#
# Copyright (c) 2015-2020 Cadence Design Systems, Inc.
# 
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#


.PHONY: all install clean dspfw

ROOTDIR = ..
CODEC_NAME = af_hostless

XA_RTOS ?= xos
XF_TRACE ?= 0

ifneq (,$(findstring RF-2015.2, $(XTENSA_SYSTEM)))
# RF.2 toolchain
  CFLAGS += -D__TOOLS_RF2__
endif

ifneq ($(XF_TRACE), 0)
  CFLAGS += -DXF_TRACE=$(XF_TRACE)
endif

ifeq ($(XA_RTOS),xos)
  INCLUDES += -I$(ROOTDIR)/include/sysdeps/xos/include 
  CFLAGS += -DHAVE_XOS
endif

INCLUDES += -I$(ROOTDIR)/../../common/include 

ifeq ($(XA_RTOS),freertos)
  FREERTOS_BASE ?= $(ROOTDIR)/FreeRTOS
  INCLUDES += -I$(ROOTDIR)/include/sysdeps/freertos/include \
			  -I$(FREERTOS_BASE)/include \
			  -I$(FREERTOS_BASE)/portable/XCC/Xtensa \
			  -I$(FREERTOS_BASE)/demos/cadence/sim/common/config_files
  CFLAGS += -DHAVE_FREERTOS
endif

vpath %.c $(ROOTDIR)/algo/hifi-dpf/src
vpath %.c $(ROOTDIR)/algo/host-apf/src
vpath %.c $(ROOTDIR)/test/src
vpath %.c $(ROOTDIR)/test/plugins
vpath %.c $(ROOTDIR)/test/plugins/cadence/pcm_gain
vpath %.c $(ROOTDIR)/test/plugins/cadence/vorbis_dec
vpath %.c $(ROOTDIR)/test/plugins/cadence/renderer

CFLAGS += -DHAVE_XOS=1
CFLAGS += -DXA_PCM_GAIN=1
CFLAGS += -DXA_VORBIS_DECODER=1
CFLAGS += -DXA_RENDERER=1
CFLAGS += -DFIO_LOCAL_FS
CFLAGS += -DXAF_PROFILE
CFLAGS += -DXRP_CODE_INTG_CHG
  
DSPOBJS =           \
    xf-main.o       \
    xf-msgq.o

LIBISROBJS =       	\
    xf-ipi.o		\
    xf-isr.o    	\
    xa-class-renderer.o    \
    xa-class-capturer.o	   \


COREOBJS =          \
    rbtree.o        \
    xf-core.o       \
    xf-io.o         \
    xf-mem.o        \
    xf-msg.o        \
    xf-sched.o      \

    
AUDIOOBJS =                \
    xa-class-base.o        \
    xa-class-audio-codec.o \
    xa-class-mixer.o       \
    xa-class-mimo-proc.o   \
	

HOSTOBJS += \
    xf-proxy.o \
    xf-trace.o \
    xaf-api.o \
    xf-msgq1.o
    
TESTOBJS += \
    xa-factory.o \
    xa-pcm-gain.o \
    xa-vorbis-decoder.o \
    xa-renderer.o \
    xaf-fio-test.o \
    xaf-clk-test.o \
    xaf-mem-test.o \
    xaf-hosted-main-dsp.o \
    xaf-hosted-utils.o \
    xaf-hosted-pcm-gain.o \
    circularbuffer.o \
    xaf-hosted-file-dec.o
    

LIBO2OBJS = $(DSPOBJS) $(COREOBJS) $(AUDIOOBJS) 
LIBOSOBJS = $(TESTOBJS)

INCLUDES += \
    -I$(ROOTDIR)/algo/hifi-dpf/include \
    -I$(ROOTDIR)/algo/hifi-dpf/include/audio \
    -I$(ROOTDIR)/algo/hifi-dpf/include/lib \
    -I$(ROOTDIR)/algo/hifi-dpf/include/sys/xos-msgq \
    -I$(ROOTDIR)/algo/host-apf/include \
    -I$(ROOTDIR)/algo/xa_af_hostless/include \
    -I$(ROOTDIR)/include \
    -I$(ROOTDIR)/include/audio \
    -I$(ROOTDIR)/algo/host-apf/include/sys/xos-msgq

INCLUDES += -I$(ROOTDIR)/algo/hifi-dpf/include/sys/xos-msgq/iss

INCLUDES += \
    -I$(ROOTDIR)/test/include \
    -I$(ROOTDIR)/test/include/audio    



FW: $(CODEC_NAME) 

include $(ROOTDIR)/build/common.mk

