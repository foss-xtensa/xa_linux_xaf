# Assumes XTENSA_CORE=AE_HiFi4_LE5
# Set PATH to `xt-run --show-config=tools`/bin:$PATH
#
# 1) make clean
# 2) make xrp_setup
#    This copies over the XRP host source files from XtensaTools
# 3) make build 
#    This generates the XTSC model and the MP LSPs
# 4) make stmp-xrp 
#    This generates the XRP libraries for the host
# 5) make run
#    Builds the binaries for the host and the 2 DSPs and runs them
#
XTENSA_TOOLS = `xt-run --show-config=xttools`

# Set below for internal builds only
ifeq ($(TEN4_INTERNAL),1)
  TCTOOLS=-tctools $(XTENSA_TOOLS)/../tctools
endif

DSP_DIR = ../dsp
HOST_DIR = ../host/src
XA_FRMWK_HW_REL_DIR = ../dsp/xaf/build
SYSBUILD_DIR = sysbuild_dir
SUBSYS_YML = subsys.yml
SUBSYS_XLD = subsys.xld

LSP0=${SYSBUILD_DIR}/sys-x86_64-linux/subsys.xld/package/cores/DSP0/xtensa-elf/lib/sim

CACHE_ATTR=0x44441144
DSP0_COMM_BASE_MAGIC=0x30000000

HOST_XRP_SRC=xrp_host/lib/xrp_src
DSP_CC=xt-clang

HOST_BIN=host_application_test
DSP0_BIN=xa_dsp_firmware

INCLUDE=../common/include
HOST_INCLUDE=../host/include
HOST_LDFLAGS = \
  -Linstall/host/lib \
  -lxrp-linux-sim -lpthread -lfdt -lrt \
  install/host/lib/xrp.dts.o

HOST_CFLAGS += -I $(INCLUDE) -I $(HOST_INCLUDE) -I $(HOST_XRP_SRC) -O2 -Wall -Werror

GAIN_FLAGS ?= GAIN_0DB_REF

HOST_CFLAGS += -D$(GAIN_FLAGS)=1

DSP0_LDFLAGS = -lxrp-dsp -lxrp-dsp-hw-simple-xos -lxrp-common \
                -Wl,--defsym,_memmap_cacheattr_reset=$(CACHE_ATTR) \
                -Wl,--defsym,xrp_dsp_comm_base_magic=$(DSP0_COMM_BASE_MAGIC) \
                -mlsp=$(LSP0)

DSP_CFLAGS = -I $(INCLUDE) -O2 -Wall -Werror

all: stmp-xrp $(HOST_BIN) $(DSP0_BIN)

# Copy over the XRP host sources
xrp_setup: 
	mkdir -p $(HOST_XRP_SRC)
	cp -rf $(XTENSA_TOOLS)/xtensa-elf/src/xrp/xrp_api.h $(HOST_XRP_SRC)
	cp -rf $(XTENSA_TOOLS)/xtensa-elf/src/xrp/xrp-common $(HOST_XRP_SRC)
	cp -rf $(XTENSA_TOOLS)/xtensa-elf/src/xrp/xrp-host $(HOST_XRP_SRC)
	cp -rf $(XTENSA_TOOLS)/xtensa-elf/src/xrp/xrp-kernel $(HOST_XRP_SRC)

stmp-xrp:
	rm -rf build install
	mkdir -p build install
	cd build && \
	  CC=gcc CXX=g++  \
		cmake ..  \
      -DCMAKE_INSTALL_PREFIX=../install && \
  make VERBOSE=1 install && \
  cd ..
	rm -rf build
	touch stmp-xrp

build: $(SUBSYS_YML) $(SUBSYS_XLD)
	$(XTENSA_TOOLS)/libexec/xt-sysbuilder -subsys $(SUBSYS_YML) -swtools $(XTENSA_TOOLS) -build $(SYSBUILD_DIR)
	$(XTENSA_TOOLS)/libexec/xt-mbuild -root $(SYSBUILD_DIR) $(TCTOOLS) -socfile $(SUBSYS_XLD) -preclean -build sw -lsps=sim
	cp -f $(SYSBUILD_DIR)/xrp/SubSystem.dts xrp_host/xrp.dts
	cp -f $(SYSBUILD_DIR)/xrp/XRP_Controller.lua ${SYSBUILD_DIR}/xtsc-run/TestBenchMaster.vec

$(HOST_BIN): $(HOST_DIR)/host_main.c
	gcc $(HOST_CFLAGS) $< $(HOST_LDFLAGS) -o $@

LIBS := -Wl,--start-group \
        $(XA_FRMWK_HW_REL_DIR)/xa_af_hostless.a \
        $(XA_FRMWK_HW_REL_DIR)/../test/plugins/cadence/vorbis_dec/lib/xa_vorbis_dec.a \
        -Wl,--end-group 
    
$(DSP0_BIN): $(DSP_DIR)/dsp_main.c 
	$(DSP_CC) --xtensa-core=${XTENSA_CORE} $(DSP_CFLAGS) $< $(DSP0_LDFLAGS)  $(LIBS) -lxos -o $@

define run_pre
  rm -rf run_sim.sh; \
	cat run_prologue.sh > run_sim.sh; \
  echo "./$(1) gain  $(2) & pids[0]=\$$!" >> run_sim.sh; \
  echo "sleep 1" >> run_sim.sh
endef 

run: all
	$(call run_pre,$(HOST_BIN))
	@echo "xtsc-run \
           --set_xtsc_parm=turbo=true \
           --set_xtsc_parm=turbo_max_relaxed_cycles=10000000 \
           --define=DSP0_BINARY=$(DSP0_BIN) \
           --define=SHAREDRAM_NAME=SharedRAM_L.\$${pids[0]} \
           --define=SYSTEMRAM_DELAY=1 \
           --define=SYSTEMROM_DELAY=1 \
           --include=${SYSBUILD_DIR}/xtsc-run/SubSystem.inc " >> run_sim.sh
	@echo "wait" >> run_sim.sh
	chmod 755 run_sim.sh
	./run_sim.sh

clean:
	rm -rf build install stmp-xrp $(HOST_BIN) $(DSP0_BIN) $(SYSBUILD_DIR) xrp_host/xrp.dts run_sim.sh
	rm -rf $(HOST_XRP_SRC)/xrp_api.h
	rm -rf $(HOST_XRP_SRC)/xrp-common
	rm -rf $(HOST_XRP_SRC)/xrp-host
	rm -rf $(HOST_XRP_SRC)/xrp-kernel

