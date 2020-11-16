%VK_SDK_PATH%/Bin32/glslc.exe pbr.vert -o pbr.vert.spv
%VK_SDK_PATH%/Bin32/glslc.exe pbr.frag -o pbr.frag.spv
%VK_SDK_PATH%/Bin32/glslc.exe skybox.vert -o skybox.vert.spv
%VK_SDK_PATH%/Bin32/glslc.exe skybox.frag -o skybox.frag.spv
%VK_SDK_PATH%/Bin32/glslc.exe equirect2cube.comp -o equirect2cube.comp.spv
%VK_SDK_PATH%/Bin32/glslc.exe irmap.comp -o irmap.comp.spv
%VK_SDK_PATH%/Bin32/glslc.exe spbrdf.comp -o spbrdf.comp.spv
%VK_SDK_PATH%/Bin32/glslc.exe spmap.comp -o spmap.comp.spv
pause
