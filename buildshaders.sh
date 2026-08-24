#!/bin/bash

shaderList="../shaders/list.txt"

while IFS= read -r shaderFile || [ -n "$shaderFile" ]; do
    echo "Compiling shader: $shaderFile"
    ../slang/bin/slangc ../shaders/$shaderFile.slang -target spirv -emit-spirv-directly -fvk-use-entrypoint-name -o ../build/shaders/$shaderFile.spv
done < "$shaderList"