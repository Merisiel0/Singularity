SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/shaders"

# vertex shader
glslc $SCRIPT_DIR/uncompiled/vert.vert --target-env=vulkan1.3 -o $SCRIPT_DIR/vert.spv

# fragment shaders
glslc $SCRIPT_DIR/uncompiled/singularity.frag --target-env=vulkan1.3 -o $SCRIPT_DIR/singularity.spv
glslc $SCRIPT_DIR/uncompiled/laser-dance.frag --target-env=vulkan1.3 -o $SCRIPT_DIR/laser-dance.spv
glslc $SCRIPT_DIR/uncompiled/cold.frag --target-env=vulkan1.3 -o $SCRIPT_DIR/cold.spv
glslc $SCRIPT_DIR/uncompiled/blue-moon-river.frag --target-env=vulkan1.3 -o $SCRIPT_DIR/blue-moon-river.spv
glslc $SCRIPT_DIR/uncompiled/self-reflect.frag --target-env=vulkan1.3 -o $SCRIPT_DIR/self-reflect.spv
glslc $SCRIPT_DIR/uncompiled/twinkling-tunnel.frag --target-env=vulkan1.3 -o $SCRIPT_DIR/twinkling-tunnel.spv

glslc $SCRIPT_DIR/uncompiled/test.frag --target-env=vulkan1.3 -o $SCRIPT_DIR/test.spv