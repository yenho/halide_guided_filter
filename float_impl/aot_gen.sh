#!/bin/sh -x
rm -rf ./$2/aot
mkdir -p ./$2/aot
if [ -z "$1" ] 
then
    export HL_AOT_TARGET=host
else
    export HL_AOT_TARGET=$1
fi

LD_LIBRARY_PATH=../bin/ ./gf_generator -g guided_filter -o ./$2/aot input_I.type=uint8 input_P.type=uint8 output.type=uint8 auto_schedule=true target=$HL_AOT_TARGET 
#LD_LIBRARY_PATH=../bin/ ./gf_generator -g guided_filter -o ./$2/aot input_I.type=uint8 input_P.type=uint8 output.type=uint8 target=$HL_AOT_TARGET 
LD_LIBRARY_PATH=../bin/ ./gf_generator -g fast_guided_filter -o ./$2/aot input_I.type=uint8 input_P.type=uint8 output.type=uint8 auto_schedule=true target=$HL_AOT_TARGET 
#LD_LIBRARY_PATH=../bin/ ./gf_generator -g fast_guided_filter -o ./$2/aot input_I.type=uint8 input_P.type=uint8 output.type=uint8 target=$HL_AOT_TARGET 
LD_LIBRARY_PATH=../bin/ ./gf_generator -g fast_guided_filter_skip -o ./$2/aot input_I.type=uint8 input_P.type=uint8 output.type=uint8 mean_a.type=float32 mean_b.type=float32 auto_schedule=true target=$HL_AOT_TARGET 
#LD_LIBRARY_PATH=../bin/ ./gf_generator -g fast_guided_filter_skip -o ./$2/aot input_I.type=uint8 input_P.type=uint8 output.type=uint8 mean_a.type=float32 mean_b.type=float32 target=$HL_AOT_TARGET 

