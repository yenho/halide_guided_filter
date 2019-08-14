NDK_CXX=/opt/android-ndk-r20/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang++
HALIDE_ROOT=$(HOME)/workspace/Halide
HALIDE_BUILD=$(HOME)/workspace/Halide/halide_build

CXX=clang++ -O3
CXX_FLAGS=-fno-rtti -std=c++11 -I ../include/ -I $(HALIDE_ROOT)/tools
CXX_LIBS=-L $(HALIDE_BUILD)/bin/ -lHalide -ltinfo -lpthread -ldl -ljpeg -lpng
CXX_GF_LIBS=-L $(HALIDE_BUILD)/bin/ -lpthread -ldl -ljpeg -lpng
NDK_CXX_GF_LIBS=-L $(HALIDE_BUILD)/bin/ -ldl -llog

all: gf gf_jit divmap_gen gf_generator

gf: gf.cpp aot/guided_filter.a aot/guided_filter.h gf_cfg.h
	$(CXX) $(CXX_FLAGS) -I aot -o gf gf.cpp aot/guided_filter.a  $(CXX_GF_LIBS)

divmap_gen: divmap_gen.cpp gf_cfg.h
	$(CXX) -o divmap_gen divmap_gen.cpp
	./divmap_gen

gf_jit: gf_jit.cpp
	$(CXX) $(CXX_FLAGS) -o gf_jit gf_jit.cpp $(CXX_LIBS)

android_gf: gf.cpp aot_ndk_arm64/guided_filter.a aot_ndk_arm64/guided_filter.h
	$(NDK_CXX) $(CXX_FLAGS) -I aot_ndk_arm64 -o android_gf gf.cpp aot_ndk_arm64/guided_filter.a  $(NDK_CXX_GF_LIBS)

android_hvx_gf: gf.cpp aot_ndk_arm64_hvx/guided_filter.a aot_ndk_arm64_hvx/guided_filter.h
	$(NDK_CXX) $(CXX_FLAGS) -D_AOT_HVX_ -I aot_ndk_arm64_hvx -o android_hvx_gf gf.cpp aot_ndk_arm64_hvx/guided_filter.a  $(NDK_CXX_GF_LIBS)

android_hvx-v66_gf: gf.cpp aot_ndk_arm64_hvx-v66/guided_filter.a aot_ndk_arm64_hvx-v66/guided_filter.h
	$(NDK_CXX) $(CXX_FLAGS) -D_AOT_HVX_ -D_AOT_HVX_V66_ -I aot_ndk_arm64_hvx-v66 -I aot_ndk_arm64_hvx-v66/android_ReleaseG_aarch64 -L aot_ndk_arm64_hvx/android_ReleaseG_aarch64 -o android_hvx-v66_gf gf.cpp aot_ndk_arm64_hvx/guided_filter.a  $(NDK_CXX_GF_LIBS) -lcdsprpc

aot/guided_filter.a: gf_generator
	LD_LIBRARY_PATH=$(HALIDE_BUILD)/bin/ ./gf_generator -g guided_filter -o ./aot auto_schedule=false target=host

aot_ndk_arm64_hvx/guided_filter.a: gf_generator
	LD_LIBRARY_PATH=$(HALIDE_BUILD)/bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm64_hvx auto_schedule=false target=arm-64-android-hvx_128

aot_ndk_arm64_hvx-v66/guided_filter.a: gf_generator
	LD_LIBRARY_PATH=$(HALIDE_BUILD)/bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm64_hvx-v66 auto_schedule=false target=arm-64-android-hvx_v66-hvx_128


aot_ndk_arm64/guided_filter.a: gf_generator
	LD_LIBRARY_PATH=$(HALIDE_BUILD)/bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm64 auto_schedule=false target=arm-64-android
#	LD_LIBRARY_PATH=$(HALIDE_BUILD)/bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm64 auto_schedule=true target=arm-64-android	

aot_ndk_arm32/guided_filter.a: gf_generator
	LD_LIBRARY_PATH=$(HALIDE_BUILD)/bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm64 auto_schedule=true target=arm-32-android-hvx_128


gf_generator: gf_generator.cpp gf_cfg.h
	$(CXX) $(CXX_FLAGS) -o gf_generator gf_generator.cpp $(HALIDE_ROOT)/tools/GenGen.cpp $(CXX_LIBS)

clean_aot:
	rm -f aot/* aot_ndk_arm64/* aot_ndk_arm32/* aot_ndk_arm64_hvx/*

clean:
	rm -f divmap.bin divmap_gen gf_jit gf android_gf android_hvx_gf android_hvx-v66_gf gf_generator
