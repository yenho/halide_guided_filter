CXX=clang++ -O3
NDK_CXX=/opt/android-ndk-r20/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang++
HALIDE_ROOT=../../
CXX_FLAGS=-fno-rtti -std=c++11 -I ../include/ -I $(HALIDE_ROOT)/tools
CXX_LIBS=-L ../bin/ -lHalide -ltinfo -lpthread -ldl -ljpeg -lpng
CXX_GF_LIBS=-L ../bin/ -lpthread -ldl -ljpeg -lpng
NDK_CXX_GF_LIBS=-L ../bin/ -ldl -llog

all: gf gf_jit divmap_gen gf_generator

gf: gf.cpp aot/guided_filter.a aot/guided_filter.h
	$(CXX) $(CXX_FLAGS) -I aot -o gf gf.cpp aot/guided_filter.a  $(CXX_GF_LIBS)

gf_ocl: gf.cpp aot_ocl/guided_filter.a aot_ocl/guided_filter.h
	$(CXX) $(CXX_FLAGS) -D_AOT_OCL_ -I aot -o gf gf.cpp aot_ocl/guided_filter.a  $(CXX_GF_LIBS)

divmap_gen: divmap_gen.cpp
	$(CXX) -o divmap_gen divmap_gen.cpp

gf_jit: gf_jit.cpp
	$(CXX) $(CXX_FLAGS) -o gf_jit gf_jit.cpp $(CXX_LIBS)

android_gf: gf.cpp aot_ndk_arm64/guided_filter.a aot_ndk_arm64/guided_filter.h
	$(NDK_CXX) $(CXX_FLAGS) -I aot_ndk_arm64 -o android_gf gf.cpp aot_ndk_arm64/guided_filter.a  $(NDK_CXX_GF_LIBS)

android_hvx_gf: gf.cpp aot_ndk_arm64_hvx/guided_filter.a aot_ndk_arm64_hvx/guided_filter.h
	$(NDK_CXX) $(CXX_FLAGS) -D_AOT_HVX_ -I aot_ndk_arm64_hvx -o android_hvx_gf gf.cpp aot_ndk_arm64_hvx/guided_filter.a  $(NDK_CXX_GF_LIBS)

aot_ocl/guided_filter.a: gf_generator
	LD_LIBRARY_PATH=../bin/ ./gf_generator -g guided_filter -o ./aot_ocl auto_schedule=false target=host-opencl

aot/guided_filter.a: gf_generator
	LD_LIBRARY_PATH=../bin/ ./gf_generator -g guided_filter -o ./aot auto_schedule=false target=host	

aot_ndk_arm64_hvx/guided_filter.a: gf_generator
	LD_LIBRARY_PATH=../bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm64_hvx auto_schedule=false target=arm-64-android-hvx_64

aot_ndk_arm64/guided_filter.a: gf_generator
	LD_LIBRARY_PATH=../bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm64 auto_schedule=true target=arm-64-android
#	LD_LIBRARY_PATH=../bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm64 auto_schedule=true target=arm-64-android	
#	LD_LIBRARY_PATH=../bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm64 MA.type=float32 MB.type=float32 output.type=float32 auto_schedule=true target=arm-64-android
#	LD_LIBRARY_PATH=../bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm32 MA.type=float32 MB.type=float32 output.type=float32 auto_schedule=true target=arm-32-android

aot_ndk_arm32/guided_filter.a: gf_generator
	LD_LIBRARY_PATH=../bin/ ./gf_generator -g guided_filter -o ./aot_ndk_arm64 auto_schedule=false target=arm-32-android-hvx_128


gf_generator: gf_generator.cpp
	$(CXX) $(CXX_FLAGS) -o gf_generator gf_generator.cpp $(HALIDE_ROOT)/tools/GenGen.cpp $(CXX_LIBS)

clean_aot:
	rm -f aot/* aot_ndk_arm64/* aot_ndk_arm32/* aot_ndk_arm64_hvx/*

clean:
	rm -f divmap_gen gf_jit gf android_gf android_hvx_gf gf_generator 
