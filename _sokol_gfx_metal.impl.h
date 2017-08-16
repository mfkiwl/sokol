/*
    Sokol Metal rendering backend.
*/
#ifndef SOKOL_IMPL_GUARD
#error "Please do not include *.impl.h files directly"
#endif

#if !__has_feature(objc_arc)
#error "Please enable ARC when using the Metal backend"
#endif

/* memset() */
#include <string.h>
#import <Metal/Metal.h>

#ifdef __cplusplus
extern "C" {
#endif
    
enum {
    _SG_MTL_NUM_INFLIGHT_FRAMES = 2,
    _SG_MTL_DEFAULT_UB_SIZE = 4 * 1024 * 1024,
    /* FIXME: 256 on macOS, 16 on iOS! */
    _SG_MTL_UB_ALIGN = 256,
};

/*-- enum translation functions ----------------------------------------------*/
_SOKOL_PRIVATE MTLLoadAction _sg_mtl_load_action(sg_action a) {
    switch (a) {
        case SG_ACTION_CLEAR:       return MTLLoadActionClear;
        case SG_ACTION_LOAD:        return MTLLoadActionLoad;
        case SG_ACTION_DONTCARE:    return MTLLoadActionDontCare;
        default:
            SOKOL_UNREACHABLE;
            return 0;
    }
}

_SOKOL_PRIVATE MTLResourceOptions _sg_mtl_buffer_resource_options(sg_usage usg) {
    // FIXME: managed mode only on MacOS
    switch (usg) {
        case SG_USAGE_IMMUTABLE:
            return MTLResourceStorageModeShared;
        case SG_USAGE_DYNAMIC:
        case SG_USAGE_STREAM:
            return MTLCPUCacheModeWriteCombined|MTLResourceStorageModeManaged;
        default:
            SOKOL_UNREACHABLE;
            return 0;
    }
}

_SOKOL_PRIVATE MTLVertexStepFunction _sg_mtl_step_function(sg_vertex_step step) {
    switch (step) {
        case SG_VERTEXSTEP_PER_VERTEX:      return MTLVertexStepFunctionPerVertex;
        case SG_VERTEXSTEP_PER_INSTANCE:    return MTLVertexStepFunctionPerInstance;
        default:
            SOKOL_UNREACHABLE;
            return 0;
    }
}

_SOKOL_PRIVATE MTLVertexFormat _sg_mtl_vertex_format(sg_vertex_format fmt) {
    switch (fmt) {
        case SG_VERTEXFORMAT_FLOAT:     return MTLVertexFormatFloat;
        case SG_VERTEXFORMAT_FLOAT2:    return MTLVertexFormatFloat2;
        case SG_VERTEXFORMAT_FLOAT3:    return MTLVertexFormatFloat3;
        case SG_VERTEXFORMAT_FLOAT4:    return MTLVertexFormatFloat4;
        case SG_VERTEXFORMAT_BYTE4:     return MTLVertexFormatChar4;
        case SG_VERTEXFORMAT_BYTE4N:    return MTLVertexFormatChar4Normalized;
        case SG_VERTEXFORMAT_UBYTE4:    return MTLVertexFormatUChar4;
        case SG_VERTEXFORMAT_UBYTE4N:   return MTLVertexFormatUChar4Normalized;
        case SG_VERTEXFORMAT_SHORT2:    return MTLVertexFormatShort2;
        case SG_VERTEXFORMAT_SHORT2N:   return MTLVertexFormatShort2Normalized;
        case SG_VERTEXFORMAT_SHORT4:    return MTLVertexFormatShort4;
        case SG_VERTEXFORMAT_SHORT4N:   return MTLVertexFormatShort4Normalized;
        case SG_VERTEXFORMAT_UINT10_N2: return MTLVertexFormatUInt1010102Normalized;
        default:
            SOKOL_UNREACHABLE;
            return 0;
    }
}

_SOKOL_PRIVATE MTLPrimitiveTopologyClass _sg_mtl_primitive_topology_class(sg_primitive_type t) {
    switch (t) {
        case SG_PRIMITIVETYPE_POINTS:
            return MTLPrimitiveTopologyClassPoint;
        case SG_PRIMITIVETYPE_LINES:
        case SG_PRIMITIVETYPE_LINE_STRIP:
            return MTLPrimitiveTopologyClassLine;
        case SG_PRIMITIVETYPE_TRIANGLES:
        case SG_PRIMITIVETYPE_TRIANGLE_STRIP:
            return MTLPrimitiveTopologyClassTriangle;
        default:
            SOKOL_UNREACHABLE;
            return 0;
    }
}

_SOKOL_PRIVATE MTLPrimitiveType _sg_mtl_primitive_type(sg_primitive_type t) {
    switch (t) {
        case SG_PRIMITIVETYPE_POINTS:           return MTLPrimitiveTypePoint;
        case SG_PRIMITIVETYPE_LINES:            return MTLPrimitiveTypeLine;
        case SG_PRIMITIVETYPE_LINE_STRIP:       return MTLPrimitiveTypeLineStrip;
        case SG_PRIMITIVETYPE_TRIANGLES:        return MTLPrimitiveTypeTriangle;
        case SG_PRIMITIVETYPE_TRIANGLE_STRIP:   return MTLPrimitiveTypeTriangleStrip;
        default: SOKOL_UNREACHABLE; break;
    }
}

_SOKOL_PRIVATE MTLPixelFormat _sg_mtl_rendertarget_color_format(sg_pixel_format fmt) {
    switch (fmt) {
        case SG_PIXELFORMAT_RGBA8:          return MTLPixelFormatBGRA8Unorm;    /* not a bug */
        case SG_PIXELFORMAT_RGBA32F:        return MTLPixelFormatRGBA32Float;
        case SG_PIXELFORMAT_RGBA16F:        return MTLPixelFormatRGBA16Float;
        case SG_PIXELFORMAT_R10G10B10A2:    return MTLPixelFormatRGB10A2Unorm;
        default:                            return MTLPixelFormatInvalid;
    }
}

_SOKOL_PRIVATE MTLPixelFormat _sg_mtl_rendertarget_depth_format(sg_pixel_format fmt) {
    switch (fmt) {
        case SG_PIXELFORMAT_DEPTH:
            return MTLPixelFormatDepth32Float;
        case SG_PIXELFORMAT_DEPTHSTENCIL:
            /* NOTE: Depth24_Stencil8 isn't universally supported! */
            return MTLPixelFormatDepth32Float_Stencil8;
        default:
            return MTLPixelFormatInvalid;
    }
}

_SOKOL_PRIVATE MTLPixelFormat _sg_mtl_rendertarget_stencil_format(sg_pixel_format fmt) {
    switch (fmt) {
        case SG_PIXELFORMAT_DEPTHSTENCIL:
            return MTLPixelFormatDepth32Float_Stencil8;
        default:
            return MTLPixelFormatInvalid;
    }
}

_SOKOL_PRIVATE MTLColorWriteMask _sg_mtl_color_write_mask(sg_color_mask m) {
    MTLColorWriteMask mtl_mask = MTLColorWriteMaskNone;
    if (m & SG_COLORMASK_R) {
        mtl_mask |= MTLColorWriteMaskRed;
    }
    if (m & SG_COLORMASK_G) {
        mtl_mask |= MTLColorWriteMaskGreen;
    }
    if (m & SG_COLORMASK_B) {
        mtl_mask |= MTLColorWriteMaskBlue;
    }
    if (m & SG_COLORMASK_A) {
        mtl_mask |= MTLColorWriteMaskAlpha;
    }
    return mtl_mask;
}

_SOKOL_PRIVATE MTLBlendOperation _sg_mtl_blend_op(sg_blend_op op) {
    switch (op) {
        case SG_BLENDOP_ADD:                return MTLBlendOperationAdd;
        case SG_BLENDOP_SUBTRACT:           return MTLBlendOperationSubtract;
        case SG_BLENDOP_REVERSE_SUBTRACT:   return MTLBlendOperationReverseSubtract;
        default: SOKOL_UNREACHABLE; return 0;
    }
}

_SOKOL_PRIVATE MTLBlendFactor _sg_mtl_blend_factor(sg_blend_factor f) {
    switch (f) {
        case SG_BLENDFACTOR_ZERO:                   return MTLBlendFactorZero;
        case SG_BLENDFACTOR_ONE:                    return MTLBlendFactorOne;
        case SG_BLENDFACTOR_SRC_COLOR:              return MTLBlendFactorSourceColor;
        case SG_BLENDFACTOR_ONE_MINUS_SRC_COLOR:    return MTLBlendFactorOneMinusSourceColor;
        case SG_BLENDFACTOR_SRC_ALPHA:              return MTLBlendFactorSourceAlpha;
        case SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA:    return MTLBlendFactorOneMinusSourceAlpha;
        case SG_BLENDFACTOR_DST_COLOR:              return MTLBlendFactorDestinationColor;
        case SG_BLENDFACTOR_ONE_MINUS_DST_COLOR:    return MTLBlendFactorOneMinusDestinationColor;
        case SG_BLENDFACTOR_DST_ALPHA:              return MTLBlendFactorDestinationAlpha;
        case SG_BLENDFACTOR_ONE_MINUS_DST_ALPHA:    return MTLBlendFactorOneMinusDestinationAlpha;
        case SG_BLENDFACTOR_SRC_ALPHA_SATURATED:    return MTLBlendFactorSourceAlphaSaturated;
        case SG_BLENDFACTOR_BLEND_COLOR:            return MTLBlendFactorBlendColor;
        case SG_BLENDFACTOR_ONE_MINUS_BLEND_COLOR:  return MTLBlendFactorOneMinusBlendColor;
        case SG_BLENDFACTOR_BLEND_ALPHA:            return MTLBlendFactorBlendAlpha;
        case SG_BLENDFACTOR_ONE_MINUS_BLEND_ALPHA:  return MTLBlendFactorOneMinusBlendAlpha;
        default: SOKOL_UNREACHABLE; return 0;
    }
}

_SOKOL_PRIVATE MTLCompareFunction _sg_mtl_compare_func(sg_compare_func f) {
    switch (f) {
        case SG_COMPAREFUNC_NEVER:          return MTLCompareFunctionNever;
        case SG_COMPAREFUNC_LESS:           return MTLCompareFunctionLess;
        case SG_COMPAREFUNC_EQUAL:          return MTLCompareFunctionEqual;
        case SG_COMPAREFUNC_LESS_EQUAL:     return MTLCompareFunctionLessEqual;
        case SG_COMPAREFUNC_GREATER:        return MTLCompareFunctionGreater;
        case SG_COMPAREFUNC_NOT_EQUAL:      return MTLCompareFunctionNotEqual;
        case SG_COMPAREFUNC_GREATER_EQUAL:  return MTLCompareFunctionGreaterEqual;
        case SG_COMPAREFUNC_ALWAYS:         return MTLCompareFunctionAlways;
        default: SOKOL_UNREACHABLE; return 0;
    }
}

_SOKOL_PRIVATE MTLStencilOperation _sg_mtl_stencil_op(sg_stencil_op op) {
    switch (op) {
        case SG_STENCILOP_KEEP:         return MTLStencilOperationKeep;
        case SG_STENCILOP_ZERO:         return MTLStencilOperationZero;
        case SG_STENCILOP_REPLACE:      return MTLStencilOperationReplace;
        case SG_STENCILOP_INCR_CLAMP:   return MTLStencilOperationIncrementClamp;
        case SG_STENCILOP_DECR_CLAMP:   return MTLStencilOperationDecrementClamp;
        case SG_STENCILOP_INVERT:       return MTLStencilOperationInvert;
        case SG_STENCILOP_INCR_WRAP:    return MTLStencilOperationIncrementWrap;
        case SG_STENCILOP_DECR_WRAP:    return MTLStencilOperationDecrementWrap;
        default: SOKOL_UNREACHABLE; return 0;
    }
}

_SOKOL_PRIVATE MTLCullMode _sg_mtl_cull_mode(sg_cull_mode m) {
    switch (m) {
        case SG_CULLMODE_NONE:  return MTLCullModeNone;
        case SG_CULLMODE_FRONT: return MTLCullModeFront;
        case SG_CULLMODE_BACK:  return MTLCullModeBack;
        default: SOKOL_UNREACHABLE; return 0;
    }
}

_SOKOL_PRIVATE MTLWinding _sg_mtl_winding(sg_face_winding w) {
    switch (w) {
        case SG_FACEWINDING_CW:     return MTLWindingClockwise;
        case SG_FACEWINDING_CCW:    return MTLWindingCounterClockwise;
        default: SOKOL_UNREACHABLE; return 0;
    }
}

_SOKOL_PRIVATE MTLIndexType _sg_mtl_index_type(sg_index_type t) {
    switch (t) {
        case SG_INDEXTYPE_UINT16:   return MTLIndexTypeUInt16;
        case SG_INDEXTYPE_UINT32:   return MTLIndexTypeUInt32;
        default: SOKOL_UNREACHABLE; return 0;
    }
}

_SOKOL_PRIVATE NSUInteger _sg_mtl_index_size(sg_index_type t) {
    switch (t) {
        case SG_INDEXTYPE_NONE:     return 0;
        case SG_INDEXTYPE_UINT16:   return 2;
        case SG_INDEXTYPE_UINT32:   return 4;
        default: SOKOL_UNREACHABLE; return 0;
    }
}

/*-- a pool for all Metal resource object, with deferred release queue -------*/
static uint32_t _sg_mtl_pool_size;
static NSMutableArray* _sg_mtl_pool;
static uint32_t _sg_mtl_free_queue_top;
static uint32_t* _sg_mtl_free_queue;
static uint32_t _sg_mtl_release_queue_front;
static uint32_t _sg_mtl_release_queue_back;
typedef struct {
    uint32_t frame_index;
    uint32_t pool_index;
} _sg_mtl_release_item;
static _sg_mtl_release_item* _sg_mtl_release_queue;

_SOKOL_PRIVATE void _sg_mtl_init_pool(const sg_desc* desc) {
    _sg_mtl_pool_size =
        2 * _sg_select(desc->buffer_pool_size, _SG_DEFAULT_BUFFER_POOL_SIZE) +
        3 * _sg_select(desc->image_pool_size, _SG_DEFAULT_IMAGE_POOL_SIZE) +
        4 * _sg_select(desc->shader_pool_size, _SG_DEFAULT_SHADER_POOL_SIZE) +
        2 * _sg_select(desc->pipeline_pool_size, _SG_DEFAULT_PIPELINE_POOL_SIZE) +
        _sg_select(desc->pass_pool_size, _SG_DEFAULT_PASS_POOL_SIZE);
    /* an id array which holds strong references to MTLResource objects */
    _sg_mtl_pool = [NSMutableArray arrayWithCapacity:_sg_mtl_pool_size];
    NSNull* null = [NSNull null];
    for (uint32_t i = 0; i < _sg_mtl_pool_size; i++) {
        [_sg_mtl_pool addObject:null];
    }
    SOKOL_ASSERT([_sg_mtl_pool count] == _sg_mtl_pool_size);
    /* a queue of currently free slot indices */
    _sg_mtl_free_queue_top = 0;
    _sg_mtl_free_queue = SOKOL_MALLOC(_sg_mtl_pool_size * sizeof(int));
    for (int i = _sg_mtl_pool_size-1; i >= 0; i--) {
        _sg_mtl_free_queue[_sg_mtl_free_queue_top++] = (uint32_t)i;
    }
    /* a circular queue which holds release items (frame index
       when a resource is to be released, and the resource's
       pool index
    */
    _sg_mtl_release_queue_front = 0;
    _sg_mtl_release_queue_back = 0;
    _sg_mtl_release_queue = SOKOL_MALLOC(_sg_mtl_pool_size * sizeof(_sg_mtl_release_item));
    for (uint32_t i = 0; i < _sg_mtl_pool_size; i++) {
        _sg_mtl_release_queue[i].frame_index = 0;
        _sg_mtl_release_queue[i].pool_index = 0xFFFFFFFF;
    }
}

_SOKOL_PRIVATE void _sg_mtl_destroy_pool() {
    SOKOL_FREE(_sg_mtl_release_queue);  _sg_mtl_release_queue = 0;
    SOKOL_FREE(_sg_mtl_free_queue);     _sg_mtl_free_queue = 0;
    _sg_mtl_pool = nil;
}

/*  add an MTLResource to the pool, return pool index or 0xFFFFFFFF if input was 'nil' */
_SOKOL_PRIVATE uint32_t _sg_mtl_add_resource(id res) {
    if (nil == res) {
        return 0xFFFFFFFF;
    }
    SOKOL_ASSERT(_sg_mtl_free_queue_top > 0);
    const uint32_t slot_index = _sg_mtl_free_queue[--_sg_mtl_free_queue_top];
    SOKOL_ASSERT([NSNull null] == _sg_mtl_pool[slot_index]);
    _sg_mtl_pool[slot_index] = res;
    return slot_index;
}

/*  mark an MTLResource for release, this will put the resource into the
    deferred-release queue, and the resource will then be releases N frames later,
    the special pool index 0xFFFFFFFF will be ignored (this means that a nil
    value was provided to _sg_mtl_add_resource()
*/
_SOKOL_PRIVATE void _sg_mtl_release_resource(uint32_t frame_index, uint32_t pool_index) {
    if (pool_index == 0xFFFFFFFF) {
        return;
    }
    SOKOL_ASSERT((pool_index >= 0) && (pool_index < _sg_mtl_pool_size));
    SOKOL_ASSERT([NSNull null] != _sg_mtl_pool[pool_index]);
    int slot_index = _sg_mtl_release_queue_front++;
    if (_sg_mtl_release_queue_front >= _sg_mtl_pool_size) {
        /* wrap-around */
        _sg_mtl_release_queue_front = 0;
    }
    /* release queue full? */
    SOKOL_ASSERT(_sg_mtl_release_queue_front != _sg_mtl_release_queue_back);
    SOKOL_ASSERT(0 == _sg_mtl_release_queue[slot_index].frame_index);
    _sg_mtl_release_queue[slot_index].frame_index = frame_index;
    _sg_mtl_release_queue[slot_index].pool_index = pool_index;
}

/* run garbage-collection pass on all resources in the release-queue */
_SOKOL_PRIVATE void _sg_mtl_garbage_collect(uint32_t frame_index) {
    const uint32_t safe_release_frame_index = frame_index + _SG_MTL_NUM_INFLIGHT_FRAMES + 1;
    while (_sg_mtl_release_queue_back != _sg_mtl_release_queue_front) {
        if (_sg_mtl_release_queue[_sg_mtl_release_queue_back].frame_index < safe_release_frame_index) {
            /* don't need to check further, release-items past this are too young */
            break;
        }
        /* safe to release this resource */
        const uint32_t pool_index = _sg_mtl_release_queue[_sg_mtl_release_queue_back].pool_index;
        SOKOL_ASSERT(pool_index < _sg_mtl_pool_size);
        SOKOL_ASSERT(_sg_mtl_pool[pool_index] != [NSNull null]);
        _sg_mtl_pool[pool_index] = [NSNull null];
        /* reset the release queue slot and advance the back index */
        _sg_mtl_release_queue[_sg_mtl_release_queue_back].frame_index = 0;
        _sg_mtl_release_queue[_sg_mtl_release_queue_back].pool_index = 0xFFFFFFFF;
        _sg_mtl_release_queue_back++;
        if (_sg_mtl_release_queue_back >= _sg_mtl_pool_size) {
            /* wrap-around */
            _sg_mtl_release_queue_back = 0;
        }
    }
}

/*-- Metal backend resource structs ------------------------------------------*/
typedef struct {
    _sg_slot slot;
    int size;
    sg_buffer_type type;
    sg_usage usage;
    uint32_t upd_frame_index;
    int num_slots;
    int active_slot;
    uint32_t mtl_buf[_SG_MTL_NUM_INFLIGHT_FRAMES];  /* index intp _sg_mtl_pool */
} _sg_buffer;

_SOKOL_PRIVATE void _sg_init_buffer(_sg_buffer* buf) {
    SOKOL_ASSERT(buf);
    memset(buf, 0, sizeof(_sg_buffer));
}

typedef struct {
    _sg_slot slot;
    sg_image_type type;
    bool render_target;
    uint16_t width;
    uint16_t height;
    uint16_t depth;
    uint16_t num_mipmaps;
    sg_usage usage;
    sg_pixel_format pixel_format;
    int sample_count;
} _sg_image;

_SOKOL_PRIVATE void _sg_init_image(_sg_image* img) {
    SOKOL_ASSERT(img);
    memset(img, 0, sizeof(_sg_image));
}

typedef struct {
    uint16_t size;
} _sg_uniform_block;

typedef struct {
    sg_image_type type;
} _sg_shader_image;

typedef struct {
    uint16_t num_uniform_blocks;
    uint16_t num_images;
    _sg_uniform_block uniform_blocks[SG_MAX_SHADERSTAGE_UBS];
    _sg_shader_image images[SG_MAX_SHADERSTAGE_IMAGES];
    uint32_t mtl_lib;
    uint32_t mtl_func;
} _sg_shader_stage;

typedef struct {
    _sg_slot slot;
    uint32_t mtl_lib;
    _sg_shader_stage stage[SG_NUM_SHADER_STAGES];
} _sg_shader;

_SOKOL_PRIVATE void _sg_init_shader(_sg_shader* shd) {
    SOKOL_ASSERT(shd);
    memset(shd, 0, sizeof(_sg_shader));
}

typedef struct {
    _sg_slot slot;
    _sg_shader* shader;
    sg_shader shader_id;
    MTLPrimitiveType mtl_prim_type;
    sg_index_type index_type;
    NSUInteger mtl_index_size;
    MTLIndexType mtl_index_type;
    MTLCullMode mtl_cull_mode;
    MTLWinding mtl_winding;
    float blend_color[4];
    uint32_t mtl_stencil_ref;
    uint32_t mtl_rps;
    uint32_t mtl_dss;
} _sg_pipeline;

_SOKOL_PRIVATE void _sg_init_pipeline(_sg_pipeline* pip) {
    SOKOL_ASSERT(pip);
    memset(pip, 0, sizeof(_sg_pipeline));
}

typedef struct {
    _sg_image* image;
    sg_image image_id;
    int mip_level;
    int slice;
} _sg_attachment;

typedef struct {
    _sg_slot slot;
    _sg_attachment color_atts[SG_MAX_COLOR_ATTACHMENTS];
    _sg_attachment ds_att;
} _sg_pass;

_SOKOL_PRIVATE void _sg_init_pass(_sg_pass* pass) {
    SOKOL_ASSERT(pass);
    memset(pass, 0, sizeof(_sg_pass));
}

/*-- main Metal backend state and functions ----------------------------------*/
static bool _sg_mtl_valid;
static bool _sg_mtl_in_pass;
static bool _sg_mtl_pass_valid;
static int _sg_mtl_cur_width;
static int _sg_mtl_cur_height;
static _sg_pipeline* _sg_mtl_cur_pipeline;
static sg_pipeline _sg_mtl_cur_pipeline_id;
static _sg_buffer* _sg_mtl_cur_indexbuffer;
static sg_buffer _sg_mtl_cur_indexbuffer_id;
static const void*(*_sg_mtl_renderpass_descriptor_cb)(void);
static const void*(*_sg_mtl_drawable_cb)(void);
static uint32_t _sg_mtl_frame_index;
static uint32_t _sg_mtl_cur_frame_rotate_index;
static id<MTLDevice> _sg_mtl_device;
static id<MTLCommandQueue> _sg_mtl_cmd_queue;
static id<MTLCommandBuffer> _sg_mtl_cmd_buffer;
static id<MTLRenderCommandEncoder> _sg_mtl_cmd_encoder;
static uint32_t _sg_mtl_ub_size;
static uint32_t _sg_mtl_cur_ub_offset;
static uint8_t* _sg_mtl_cur_ub_base_ptr;
static id<MTLBuffer> _sg_mtl_uniform_buffers[_SG_MTL_NUM_INFLIGHT_FRAMES];
static dispatch_semaphore_t _sg_mtl_sem;

_SOKOL_PRIVATE void _sg_setup_backend(const sg_desc* desc) {
    SOKOL_ASSERT(desc);
    SOKOL_ASSERT(desc->mtl_device);
    SOKOL_ASSERT(desc->mtl_renderpass_descriptor_cb);
    SOKOL_ASSERT(desc->mtl_drawable_cb);
    _sg_mtl_init_pool(desc);
    _sg_mtl_valid = true;
    _sg_mtl_in_pass = false;
    _sg_mtl_pass_valid = false;
    _sg_mtl_cur_width = 0;
    _sg_mtl_cur_height = 0;
    _sg_mtl_cur_pipeline = 0;
    _sg_mtl_cur_pipeline_id.id = SG_INVALID_ID;
    _sg_mtl_renderpass_descriptor_cb = desc->mtl_renderpass_descriptor_cb;
    _sg_mtl_drawable_cb = desc->mtl_drawable_cb;
    _sg_mtl_frame_index = 1;
    _sg_mtl_cur_frame_rotate_index = 0;
    _sg_mtl_cur_ub_offset = 0;
    _sg_mtl_cur_ub_base_ptr = 0;
    _sg_mtl_device = CFBridgingRelease(desc->mtl_device);
    _sg_mtl_sem = dispatch_semaphore_create(_SG_MTL_NUM_INFLIGHT_FRAMES);
    _sg_mtl_cmd_queue = [_sg_mtl_device newCommandQueue];
    _sg_mtl_ub_size = _sg_select(desc->mtl_global_uniform_buffer_size, _SG_MTL_DEFAULT_UB_SIZE);
    for (int i = 0; i < _SG_MTL_NUM_INFLIGHT_FRAMES; i++) {
        _sg_mtl_uniform_buffers[i] = [_sg_mtl_device
            newBufferWithLength:_sg_mtl_ub_size
            options:MTLResourceCPUCacheModeWriteCombined|MTLResourceStorageModeManaged
        ];
    }
}

_SOKOL_PRIVATE void _sg_discard_backend() {
    SOKOL_ASSERT(_sg_mtl_valid);
    /* wait for the last frame to finish */
    for (int i = 0; i < _SG_MTL_NUM_INFLIGHT_FRAMES; i++) {
        dispatch_semaphore_wait(_sg_mtl_sem, DISPATCH_TIME_FOREVER);
    }
    _sg_mtl_valid = false;
    _sg_mtl_cmd_encoder = nil;
    _sg_mtl_cmd_buffer = nil;
    _sg_mtl_cmd_queue = nil;
    for (int i = 0; i < _SG_MTL_NUM_INFLIGHT_FRAMES; i++) {
        _sg_mtl_uniform_buffers[i] = nil;
    }
    _sg_mtl_device = nil;
    _sg_mtl_destroy_pool();
}

_SOKOL_PRIVATE bool _sg_query_feature(sg_feature f) {
    // FIXME: find out if we're running on iOS
    switch (f) {
        case SG_FEATURE_INSTANCED_ARRAYS:
        case SG_FEATURE_TEXTURE_COMPRESSION_DXT:
        case SG_FEATURE_TEXTURE_FLOAT:
        case SG_FEATURE_ORIGIN_TOP_LEFT:
        case SG_FEATURE_MSAA_RENDER_TARGETS:
        case SG_FEATURE_PACKED_VERTEX_FORMAT_10_2:
        case SG_FEATURE_MULTIPLE_RENDER_TARGET:
        case SG_FEATURE_IMAGETYPE_3D:
        case SG_FEATURE_IMAGETYPE_ARRAY:
            return true;
        default:
            return false;
    }
}

_SOKOL_PRIVATE void _sg_create_buffer(_sg_buffer* buf, const sg_buffer_desc* desc) {
    SOKOL_ASSERT(buf && desc);
    SOKOL_ASSERT(buf->slot.state == SG_RESOURCESTATE_ALLOC);
    buf->size = desc->size;
    buf->type = _sg_select(desc->type, SG_BUFFERTYPE_VERTEXBUFFER);
    buf->usage = _sg_select(desc->usage, SG_USAGE_IMMUTABLE);
    buf->upd_frame_index = 0;
    buf->num_slots = buf->usage==SG_USAGE_STREAM ? _SG_MTL_NUM_INFLIGHT_FRAMES : 1;
    buf->active_slot = 0;
    MTLResourceOptions mtl_options = _sg_mtl_buffer_resource_options(buf->usage);
    for (int slot = 0; slot < buf->num_slots; slot++) {
        id<MTLBuffer> mtl_buf;
        if (buf->usage == SG_USAGE_IMMUTABLE) {
            SOKOL_ASSERT(desc->data_ptr);
            mtl_buf = [_sg_mtl_device newBufferWithBytes:desc->data_ptr length:buf->size options:mtl_options];
        }
        else {
            mtl_buf = [_sg_mtl_device newBufferWithLength:buf->size options:mtl_options];
        }
        buf->mtl_buf[slot] = _sg_mtl_add_resource(mtl_buf);
    }
    buf->slot.state = SG_RESOURCESTATE_VALID;
}

_SOKOL_PRIVATE void _sg_destroy_buffer(_sg_buffer* buf) {
    SOKOL_ASSERT(buf);
    if (buf->slot.state == SG_RESOURCESTATE_VALID) {
        for (int slot = 0; slot < buf->num_slots; slot++) {
            _sg_mtl_release_resource(_sg_mtl_frame_index, buf->mtl_buf[slot]);
        }
    }
    _sg_init_buffer(buf);
}

_SOKOL_PRIVATE void _sg_create_image(_sg_image* img, const sg_image_desc* desc) {
    SOKOL_ASSERT(img && desc);
    SOKOL_ASSERT(img->slot.state == SG_RESOURCESTATE_ALLOC);
    // FIXME
}

_SOKOL_PRIVATE void _sg_destroy_image(_sg_image* img) {
    SOKOL_ASSERT(img);
    // FIXME
    _sg_init_image(img);
}

_SOKOL_PRIVATE id<MTLLibrary> _sg_mtl_compile_library(const char* src) {
    NSError* err = NULL;
    id<MTLLibrary> lib = [_sg_mtl_device
        newLibraryWithSource:[NSString stringWithUTF8String:src]
        options:nil
        error:&err
    ];
    if (err) {
        SOKOL_LOG([err.localizedDescription UTF8String]);
    }
    return lib;
}

_SOKOL_PRIVATE void _sg_create_shader(_sg_shader* shd, const sg_shader_desc* desc) {
    SOKOL_ASSERT(shd && desc);
    SOKOL_ASSERT(shd->slot.state == SG_RESOURCESTATE_ALLOC);
    SOKOL_ASSERT(desc->vs.entry && desc->fs.entry);

    /* copy over uniform block size */
    for (int stage_index = 0; stage_index < SG_NUM_SHADER_STAGES; stage_index++) {
        const sg_shader_stage_desc* stage_desc = (stage_index == SG_SHADERSTAGE_VS) ? &desc->vs : &desc->fs;
        _sg_shader_stage* stage = &shd->stage[stage_index];
        SOKOL_ASSERT(stage->num_uniform_blocks == 0);
        for (int ub_index = 0; ub_index < SG_MAX_SHADERSTAGE_UBS; ub_index++) {
            const sg_shader_uniform_block_desc* ub_desc = &stage_desc->uniform_blocks[ub_index];
            if (0 == ub_desc->size) {
                break;
            }
            _sg_uniform_block* ub = &stage->uniform_blocks[ub_index];
            ub->size = ub_desc->size;
            stage->num_uniform_blocks++;
        }
    }

    /* create metal libray objects and lookup entry functions */
    id<MTLLibrary> lib;
    id<MTLLibrary> vs_lib;
    id<MTLLibrary> fs_lib;
    id<MTLFunction> vs_func;
    id<MTLFunction> fs_func;
    /* common source? */
    if (desc->source) {
        lib = _sg_mtl_compile_library(desc->source);
        if (nil == lib) {
            shd->slot.state = SG_RESOURCESTATE_FAILED;
            return;
        }
        vs_func = [lib newFunctionWithName:[NSString stringWithUTF8String:desc->vs.entry]];
        fs_func = [lib newFunctionWithName:[NSString stringWithUTF8String:desc->fs.entry]];
    }
    else {
        /* separate sources? */
        if (desc->vs.source) {
            vs_lib = _sg_mtl_compile_library(desc->vs.source);
        }
        if (desc->fs.source) {
            fs_lib = _sg_mtl_compile_library(desc->fs.source);
        }
        if (nil == vs_lib || nil == fs_lib) {
            shd->slot.state = SG_RESOURCESTATE_FAILED;
            return;
        }
        vs_func = [vs_lib newFunctionWithName:[NSString stringWithUTF8String:desc->vs.entry]];
        fs_func = [fs_lib newFunctionWithName:[NSString stringWithUTF8String:desc->fs.entry]];
    }
    if (nil == vs_func) {
        SOKOL_LOG("vertex shader function not found\n");
        shd->slot.state = SG_RESOURCESTATE_FAILED;
        return;
    }
    if (nil == fs_func) {
        SOKOL_LOG("fragment shader function not found\n");
        shd->slot.state = SG_RESOURCESTATE_FAILED;
        return;
    }
    /* it is legal to call _sg_mtl_add_resource with a nil value, this will return a special 0xFFFFFFFF index */
    shd->mtl_lib = _sg_mtl_add_resource(lib);
    shd->stage[SG_SHADERSTAGE_VS].mtl_lib  = _sg_mtl_add_resource(vs_lib);
    shd->stage[SG_SHADERSTAGE_FS].mtl_lib  = _sg_mtl_add_resource(fs_lib);
    shd->stage[SG_SHADERSTAGE_VS].mtl_func = _sg_mtl_add_resource(vs_func);
    shd->stage[SG_SHADERSTAGE_FS].mtl_func = _sg_mtl_add_resource(fs_func);
    shd->slot.state = SG_RESOURCESTATE_VALID;
}

_SOKOL_PRIVATE void _sg_destroy_shader(_sg_shader* shd) {
    SOKOL_ASSERT(shd);
    if (shd->slot.state == SG_RESOURCESTATE_VALID) {
        /* it is valid to call _sg_mtl_release_resource with the special 0xFFFFFFFF index */
        _sg_mtl_release_resource(_sg_mtl_frame_index, shd->stage[SG_SHADERSTAGE_VS].mtl_func);
        _sg_mtl_release_resource(_sg_mtl_frame_index, shd->stage[SG_SHADERSTAGE_VS].mtl_lib);
        _sg_mtl_release_resource(_sg_mtl_frame_index, shd->stage[SG_SHADERSTAGE_FS].mtl_func);
        _sg_mtl_release_resource(_sg_mtl_frame_index, shd->stage[SG_SHADERSTAGE_FS].mtl_lib);
        _sg_mtl_release_resource(_sg_mtl_frame_index, shd->mtl_lib);
    }
    _sg_init_shader(shd);
}

_SOKOL_PRIVATE void _sg_create_pipeline(_sg_pipeline* pip, _sg_shader* shd, const sg_pipeline_desc* desc) {
    SOKOL_ASSERT(pip && shd && desc);
    SOKOL_ASSERT(pip->slot.state == SG_RESOURCESTATE_ALLOC);
    SOKOL_ASSERT(desc->shader.id == shd->slot.id);
    SOKOL_ASSERT(shd->slot.state == SG_RESOURCESTATE_VALID);

    pip->shader = shd;
    pip->shader_id = desc->shader;
    sg_primitive_type prim_type = _sg_select(desc->primitive_type, SG_PRIMITIVETYPE_TRIANGLES);
    pip->mtl_prim_type = _sg_mtl_primitive_type(prim_type);
    pip->index_type = _sg_select(desc->index_type, SG_INDEXTYPE_NONE);
    pip->mtl_index_size = _sg_mtl_index_size(pip->index_type);
    if (SG_INDEXTYPE_NONE != pip->index_type) {
        pip->mtl_index_type = _sg_mtl_index_type(pip->index_type);
    }
    pip->mtl_cull_mode = _sg_mtl_cull_mode(_sg_select(desc->rasterizer.cull_mode, SG_CULLMODE_NONE));
    pip->mtl_winding = _sg_mtl_winding(_sg_select(desc->rasterizer.face_winding, SG_FACEWINDING_CW));
    pip->mtl_stencil_ref = desc->depth_stencil.stencil_ref;
    for (int i = 0; i < 4; i++) {
        pip->blend_color[i] = desc->blend.blend_color[i];
    }

    /* create vertex-descriptor */
    MTLVertexDescriptor* vtx_desc = [MTLVertexDescriptor vertexDescriptor];
    for (int layout_index = 0; layout_index < SG_MAX_SHADERSTAGE_BUFFERS; layout_index++) {
        const sg_vertex_layout_desc* layout_desc = &desc->vertex_layouts[layout_index];
        if (layout_desc->stride == 0) {
            break;
        }
        const int mtl_vb_slot = layout_index + SG_MAX_SHADERSTAGE_UBS;
        vtx_desc.layouts[mtl_vb_slot].stride = layout_desc->stride;
        vtx_desc.layouts[mtl_vb_slot].stepFunction = _sg_mtl_step_function(_sg_select(layout_desc->step_func, SG_VERTEXSTEP_PER_VERTEX));
        vtx_desc.layouts[mtl_vb_slot].stepRate = _sg_select(layout_desc->step_rate, 1);
        for (int attr_index = 0; attr_index < SG_MAX_VERTEX_ATTRIBUTES; attr_index++) {
            const sg_vertex_attr_desc* attr_desc = &layout_desc->attrs[attr_index];
            if (attr_desc->format == SG_VERTEXFORMAT_INVALID) {
                break;
            }
            vtx_desc.attributes[attr_index].format = _sg_mtl_vertex_format(attr_desc->format);
            vtx_desc.attributes[attr_index].offset = attr_desc->offset;
            vtx_desc.attributes[attr_index].bufferIndex = mtl_vb_slot;
        }
    }

    /* render-pipeline descriptor */
    MTLRenderPipelineDescriptor* rp_desc = [[MTLRenderPipelineDescriptor alloc] init];
    rp_desc.vertexDescriptor = vtx_desc;
    rp_desc.vertexFunction = _sg_mtl_pool[shd->stage[SG_SHADERSTAGE_VS].mtl_func];
    rp_desc.fragmentFunction = _sg_mtl_pool[shd->stage[SG_SHADERSTAGE_FS].mtl_func];
    rp_desc.sampleCount = _sg_select(desc->rasterizer.sample_count, 1);
    rp_desc.alphaToCoverageEnabled = desc->rasterizer.alpha_to_coverage_enabled;
    rp_desc.alphaToOneEnabled = NO;
    rp_desc.rasterizationEnabled = YES;
    rp_desc.inputPrimitiveTopology = _sg_mtl_primitive_topology_class(prim_type);
    rp_desc.depthAttachmentPixelFormat = _sg_mtl_rendertarget_depth_format(_sg_select(desc->blend.depth_format, SG_PIXELFORMAT_DEPTHSTENCIL));
    rp_desc.stencilAttachmentPixelFormat = _sg_mtl_rendertarget_stencil_format(_sg_select(desc->blend.depth_format, SG_PIXELFORMAT_DEPTHSTENCIL));
    const int mrt_count = _sg_select(desc->blend.mrt_count, 1);
    for (int i = 0; i < mrt_count; i++) {
        rp_desc.colorAttachments[i].pixelFormat = _sg_mtl_rendertarget_color_format(_sg_select(desc->blend.color_format, SG_PIXELFORMAT_RGBA8));
        rp_desc.colorAttachments[i].writeMask = _sg_mtl_color_write_mask(_sg_select(desc->blend.color_write_mask, SG_COLORMASK_RGBA));
        rp_desc.colorAttachments[i].blendingEnabled = desc->blend.enabled;
        rp_desc.colorAttachments[i].alphaBlendOperation = _sg_mtl_blend_op(_sg_select(desc->blend.op_alpha, SG_BLENDOP_ADD));
        rp_desc.colorAttachments[i].rgbBlendOperation = _sg_mtl_blend_op(_sg_select(desc->blend.op_rgb, SG_BLENDOP_ADD));
        rp_desc.colorAttachments[i].destinationAlphaBlendFactor = _sg_mtl_blend_factor(_sg_select(desc->blend.dst_factor_alpha, SG_BLENDFACTOR_ZERO));
        rp_desc.colorAttachments[i].destinationRGBBlendFactor = _sg_mtl_blend_factor(_sg_select(desc->blend.dst_factor_rgb, SG_BLENDFACTOR_ZERO));
        rp_desc.colorAttachments[i].sourceAlphaBlendFactor = _sg_mtl_blend_factor(_sg_select(desc->blend.src_factor_alpha, SG_BLENDFACTOR_ONE));
        rp_desc.colorAttachments[i].sourceRGBBlendFactor = _sg_mtl_blend_factor(_sg_select(desc->blend.src_factor_rgb, SG_BLENDFACTOR_ONE));
    }
    NSError* err = NULL;
    id<MTLRenderPipelineState> mtl_rps = [_sg_mtl_device newRenderPipelineStateWithDescriptor:rp_desc error:&err];
    if (nil == mtl_rps) {
        SOKOL_ASSERT(err);
        SOKOL_LOG([err.localizedDescription UTF8String]);
        pip->slot.state = SG_RESOURCESTATE_FAILED;
        return;
    }

    /* depth-stencil-state */
    MTLDepthStencilDescriptor* ds_desc = [[MTLDepthStencilDescriptor alloc] init];
    ds_desc.depthCompareFunction = _sg_mtl_compare_func(_sg_select(desc->depth_stencil.depth_compare_func, SG_COMPAREFUNC_ALWAYS));
    ds_desc.depthWriteEnabled = desc->depth_stencil.depth_write_enabled;
    if (desc->depth_stencil.stencil_enabled) {
        const sg_stencil_state* sb = &desc->depth_stencil.stencil_back;
        ds_desc.backFaceStencil = [[MTLStencilDescriptor alloc] init];
        ds_desc.backFaceStencil.stencilFailureOperation = _sg_mtl_stencil_op(_sg_select(sb->fail_op, SG_STENCILOP_KEEP));
        ds_desc.backFaceStencil.depthFailureOperation = _sg_mtl_stencil_op(_sg_select(sb->depth_fail_op, SG_STENCILOP_KEEP));
        ds_desc.backFaceStencil.depthStencilPassOperation = _sg_mtl_stencil_op(_sg_select(sb->pass_op, SG_STENCILOP_KEEP));
        ds_desc.backFaceStencil.stencilCompareFunction = _sg_mtl_compare_func(_sg_select(sb->compare_func, SG_COMPAREFUNC_ALWAYS));
        ds_desc.backFaceStencil.readMask = desc->depth_stencil.stencil_read_mask;
        ds_desc.backFaceStencil.writeMask = desc->depth_stencil.stencil_write_mask;
        const sg_stencil_state* sf = &desc->depth_stencil.stencil_front;
        ds_desc.frontFaceStencil = [[MTLStencilDescriptor alloc] init];
        ds_desc.frontFaceStencil.stencilFailureOperation = _sg_mtl_stencil_op(_sg_select(sf->fail_op, SG_STENCILOP_KEEP));
        ds_desc.frontFaceStencil.depthFailureOperation = _sg_mtl_stencil_op(_sg_select(sf->depth_fail_op, SG_STENCILOP_KEEP));
        ds_desc.frontFaceStencil.depthStencilPassOperation = _sg_mtl_stencil_op(_sg_select(sf->pass_op, SG_STENCILOP_KEEP));
        ds_desc.frontFaceStencil.stencilCompareFunction = _sg_mtl_compare_func(_sg_select(sf->compare_func, SG_COMPAREFUNC_ALWAYS));
        ds_desc.frontFaceStencil.readMask = desc->depth_stencil.stencil_read_mask;
        ds_desc.frontFaceStencil.writeMask = desc->depth_stencil.stencil_write_mask;
    }
    id<MTLDepthStencilState> mtl_dss = [_sg_mtl_device newDepthStencilStateWithDescriptor:ds_desc];

    pip->mtl_rps = _sg_mtl_add_resource(mtl_rps);
    pip->mtl_dss = _sg_mtl_add_resource(mtl_dss);
    pip->slot.state = SG_RESOURCESTATE_VALID;
}

_SOKOL_PRIVATE void _sg_destroy_pipeline(_sg_pipeline* pip) {
    SOKOL_ASSERT(pip);
    if (pip->slot.state == SG_RESOURCESTATE_VALID) {
        _sg_mtl_release_resource(_sg_mtl_frame_index, pip->mtl_rps);
        _sg_mtl_release_resource(_sg_mtl_frame_index, pip->mtl_dss);
    }
    _sg_init_pipeline(pip);
}

_SOKOL_PRIVATE void _sg_create_pass(_sg_pass* pass, _sg_image** att_images, const sg_pass_desc* desc) {
    SOKOL_ASSERT(pass && desc);
    SOKOL_ASSERT(pass->slot.state == SG_RESOURCESTATE_ALLOC);
}

_SOKOL_PRIVATE void _sg_destroy_pass(_sg_pass* pass) {
    SOKOL_ASSERT(pass);
    // FIXME
    _sg_init_pass(pass);
}

_SOKOL_PRIVATE void _sg_begin_pass(_sg_pass* pass, const sg_pass_action* action, int w, int h) {
    SOKOL_ASSERT(action);
    SOKOL_ASSERT(!_sg_mtl_in_pass);
    SOKOL_ASSERT(_sg_mtl_cmd_queue);
    SOKOL_ASSERT(!_sg_mtl_cmd_encoder);
    SOKOL_ASSERT(_sg_mtl_renderpass_descriptor_cb);
    _sg_mtl_in_pass = true;
    _sg_mtl_cur_width = w;
    _sg_mtl_cur_height = h;

    /* if this is the first pass in the frame, create a command buffer */
    if (nil == _sg_mtl_cmd_buffer) {
        /* block until the oldest frame in flight has finished */
        dispatch_semaphore_wait(_sg_mtl_sem, DISPATCH_TIME_FOREVER);
        _sg_mtl_cmd_buffer = [_sg_mtl_cmd_queue commandBufferWithUnretainedReferences];
    }

    /* if this is first pass in frame, get uniform buffer base pointer */
    if (0 == _sg_mtl_cur_ub_base_ptr) {
        _sg_mtl_cur_ub_base_ptr = (uint8_t*)[_sg_mtl_uniform_buffers[_sg_mtl_cur_frame_rotate_index] contents];
    }

    /* initialize a render pass descriptor */
    MTLRenderPassDescriptor* pass_desc = nil;
    if (pass) {
        /* offscreen render pass */
        pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
    }
    else {
        /* default render pass, call user-provided callback to provide render pass descriptor */
        pass_desc = CFBridgingRelease(_sg_mtl_renderpass_descriptor_cb());

    }
    if (pass_desc) {
        _sg_mtl_pass_valid = true;
    }
    else {
        /* default pass descriptor will not be valid if window is minized,
           don't do any rendering in this case */
        _sg_mtl_pass_valid = false;
        return;
    }
    if (pass) {
        /* FIXME: setup pass descriptor for offscreen rendering */
    }
    else {
        /* setup pass descriptor for default rendering */
        pass_desc.colorAttachments[0].loadAction = _sg_mtl_load_action(action->colors[0].action);
        const float* c = &(action->colors[0].val[0]);
        pass_desc.colorAttachments[0].clearColor = MTLClearColorMake(c[0], c[1], c[2], c[3]);
        pass_desc.depthAttachment.loadAction = _sg_mtl_load_action(action->depth.action);
        pass_desc.depthAttachment.clearDepth = action->depth.val;
        pass_desc.stencilAttachment.loadAction = _sg_mtl_load_action(action->stencil.action);
        pass_desc.stencilAttachment.clearStencil = action->stencil.val;
    }

    /* create a render command encoder, this might return nil if window is minimized */
    _sg_mtl_cmd_encoder = [_sg_mtl_cmd_buffer renderCommandEncoderWithDescriptor:pass_desc];
    if (_sg_mtl_cmd_encoder == nil) {
        _sg_mtl_pass_valid = false;
        return;
    }

    /* bind the global uniform buffer, this only happens once per pass */
    for (int slot = 0; slot < SG_MAX_SHADERSTAGE_UBS; slot++) {
        [_sg_mtl_cmd_encoder
            setVertexBuffer:_sg_mtl_uniform_buffers[_sg_mtl_cur_frame_rotate_index]
            offset:0
            atIndex:slot];
        [_sg_mtl_cmd_encoder
            setFragmentBuffer:_sg_mtl_uniform_buffers[_sg_mtl_cur_frame_rotate_index]
            offset:0
            atIndex:slot];
    }
}

_SOKOL_PRIVATE void _sg_end_pass() {
    SOKOL_ASSERT(_sg_mtl_in_pass);
    _sg_mtl_in_pass = false;
    _sg_mtl_pass_valid = false;
    if (nil != _sg_mtl_cmd_encoder) {
        [_sg_mtl_cmd_encoder endEncoding];
        _sg_mtl_cmd_encoder = nil;
    }
}

_SOKOL_PRIVATE void _sg_commit() {
    SOKOL_ASSERT(!_sg_mtl_in_pass);
    SOKOL_ASSERT(!_sg_mtl_pass_valid);
    SOKOL_ASSERT(_sg_mtl_drawable_cb);
    SOKOL_ASSERT(nil == _sg_mtl_cmd_encoder);
    SOKOL_ASSERT(nil != _sg_mtl_cmd_buffer);

    // FIXME: didModifyRange only on MacOS
    [_sg_mtl_uniform_buffers[_sg_mtl_cur_frame_rotate_index] didModifyRange:NSMakeRange(0, _sg_mtl_cur_ub_offset)];

    /* present, commit and signal semaphore when done */
    id cur_drawable = CFBridgingRelease(_sg_mtl_drawable_cb());
    [_sg_mtl_cmd_buffer presentDrawable:cur_drawable];
    __block dispatch_semaphore_t sem = _sg_mtl_sem;
    [_sg_mtl_cmd_buffer addCompletedHandler:^(id<MTLCommandBuffer> cmd_buffer) {
        dispatch_semaphore_signal(sem);
    }];
    [_sg_mtl_cmd_buffer commit];

    /* garbage-collect resources pending for release */
    _sg_mtl_garbage_collect(_sg_mtl_frame_index);

    /* rotate uniform buffer slot */
    if (++_sg_mtl_cur_frame_rotate_index >= _SG_MTL_NUM_INFLIGHT_FRAMES) {
        _sg_mtl_cur_frame_rotate_index = 0;
    }
    _sg_mtl_frame_index++;
    _sg_mtl_cur_ub_offset = 0;
    _sg_mtl_cur_ub_base_ptr = 0;
    _sg_mtl_cmd_buffer = nil;
}

_SOKOL_PRIVATE void _sg_apply_viewport(int x, int y, int w, int h, bool origin_top_left) {
    SOKOL_ASSERT(_sg_mtl_in_pass);
    if (!_sg_mtl_pass_valid) {
        return;
    }
    SOKOL_ASSERT(_sg_mtl_cmd_encoder);
    MTLViewport vp;
    vp.originX = (double) x;
    vp.originY = (double) (origin_top_left ? y : (_sg_mtl_cur_height - (y + h)));
    vp.width   = (double) w;
    vp.height  = (double) h;
    vp.znear   = 0.0;
    vp.zfar    = 1.0;
    [_sg_mtl_cmd_encoder setViewport:vp];
}

#define _sg_mtl_min(a,b) ((a<b)?a:b)
#define _sg_mtl_max(a,b) ((a>b)?a:b)

_SOKOL_PRIVATE void _sg_apply_scissor_rect(int x, int y, int w, int h, bool origin_top_left) {
    SOKOL_ASSERT(_sg_mtl_in_pass);
    if (!_sg_mtl_pass_valid) {
        return;
    }
    SOKOL_ASSERT(_sg_mtl_cmd_encoder);
    /* clip against framebuffer rect */
    x = _sg_mtl_min(_sg_mtl_max(0, x), _sg_mtl_cur_width-1);
    y = _sg_mtl_min(_sg_mtl_max(0, y), _sg_mtl_cur_height-1);
    if ((x + w) > _sg_mtl_cur_width) {
        w = _sg_mtl_cur_width - x;
    }
    if ((y + h) > _sg_mtl_cur_height) {
        h = _sg_mtl_cur_height - y;
    }
    w = _sg_mtl_max(w, 1);
    h = _sg_mtl_max(h, 1);

    MTLScissorRect r;
    r.x = x;
    r.y = origin_top_left ? y : (_sg_mtl_cur_height - (y + h));
    r.width = w;
    r.height = h;
    [_sg_mtl_cmd_encoder setScissorRect:r];
}

_SOKOL_PRIVATE void _sg_apply_draw_state(
    _sg_pipeline* pip,
    _sg_buffer** vbs, int num_vbs, _sg_buffer* ib,
    _sg_image** vs_imgs, int num_vs_imgs,
    _sg_image** fs_imgs, int num_fs_imgs)
{
    SOKOL_ASSERT(pip);
    SOKOL_ASSERT(pip->shader);
    SOKOL_ASSERT(_sg_mtl_in_pass);
    if (!_sg_mtl_pass_valid) {
        return;
    }
    SOKOL_ASSERT(_sg_mtl_cmd_encoder);

    _sg_mtl_cur_pipeline = pip;
    _sg_mtl_cur_pipeline_id.id = pip->slot.id;
    _sg_mtl_cur_indexbuffer = ib;
    if (ib) {
        SOKOL_ASSERT(pip->index_type != SG_INDEXTYPE_NONE);
        _sg_mtl_cur_indexbuffer_id.id = ib->slot.id;
    }
    else {
        SOKOL_ASSERT(pip->index_type == SG_INDEXTYPE_NONE);
        _sg_mtl_cur_indexbuffer_id.id = SG_INVALID_ID;
    }

    /* apply state */
    const float* c = pip->blend_color;
    [_sg_mtl_cmd_encoder setBlendColorRed:c[0] green:c[1] blue:c[2] alpha:c[3]];
    [_sg_mtl_cmd_encoder setCullMode:pip->mtl_cull_mode];
    [_sg_mtl_cmd_encoder setFrontFacingWinding:pip->mtl_winding];
    [_sg_mtl_cmd_encoder setStencilReferenceValue:pip->mtl_stencil_ref];
    [_sg_mtl_cmd_encoder setRenderPipelineState:_sg_mtl_pool[pip->mtl_rps]];
    [_sg_mtl_cmd_encoder setDepthStencilState:_sg_mtl_pool[pip->mtl_dss]];

    /* apply vertex buffers */
    int vb_slot;
    for (vb_slot = 0; vb_slot < num_vbs; vb_slot++) {
        const _sg_buffer* vb = vbs[vb_slot];
        const NSUInteger mtl_vb_slot = SG_MAX_SHADERSTAGE_UBS + vb_slot;
        [_sg_mtl_cmd_encoder setVertexBuffer:_sg_mtl_pool[vb->mtl_buf[vb->active_slot]] offset:0 atIndex:mtl_vb_slot];
    }
    for (; vb_slot < SG_MAX_SHADERSTAGE_BUFFERS; vb_slot++) {
        const NSUInteger mtl_vb_slot = SG_MAX_SHADERSTAGE_UBS + vb_slot;
        [_sg_mtl_cmd_encoder setVertexBuffer:nil offset:0 atIndex:mtl_vb_slot];
    }

    /* FIXME: apply vertex shader images */
    /* FIXME: apply fragment shader images */
}

#define _sg_mtl_roundup(val, round_to) (((val)+((round_to)-1))&~((round_to)-1))

_SOKOL_PRIVATE void _sg_apply_uniform_block(sg_shader_stage stage_index, int ub_index, const void* data, int num_bytes) {
    SOKOL_ASSERT(_sg_mtl_in_pass);
    if (!_sg_mtl_pass_valid) {
        return;
    }
    SOKOL_ASSERT(_sg_mtl_cmd_encoder);
    SOKOL_ASSERT(data && (num_bytes > 0));
    SOKOL_ASSERT((stage_index >= 0) && ((int)stage_index < SG_NUM_SHADER_STAGES));
    SOKOL_ASSERT((ub_index >= 0) && (ub_index < SG_MAX_SHADERSTAGE_UBS));
    SOKOL_ASSERT((_sg_mtl_cur_ub_offset + num_bytes) <= _sg_mtl_ub_size);
    SOKOL_ASSERT((_sg_mtl_cur_ub_offset & (_SG_MTL_UB_ALIGN-1)) == 0);
    SOKOL_ASSERT(_sg_mtl_cur_pipeline && _sg_mtl_cur_pipeline->shader);
    SOKOL_ASSERT(_sg_mtl_cur_pipeline->slot.id == _sg_mtl_cur_pipeline_id.id);
    SOKOL_ASSERT(_sg_mtl_cur_pipeline->shader->slot.id == _sg_mtl_cur_pipeline->shader_id.id);
    _sg_shader* shd = _sg_mtl_cur_pipeline->shader;
    SOKOL_ASSERT(ub_index < shd->stage[stage_index].num_uniform_blocks);
    SOKOL_ASSERT(shd->stage[stage_index].uniform_blocks[ub_index].size == num_bytes);

    /* copy to global uniform buffer, record offset into cmd encoder, and advance offset */
    uint8_t* dst = &_sg_mtl_cur_ub_base_ptr[_sg_mtl_cur_ub_offset];
    memcpy(dst, data, num_bytes);
    if (stage_index == SG_SHADERSTAGE_VS) {
        [_sg_mtl_cmd_encoder setVertexBufferOffset:_sg_mtl_cur_ub_offset atIndex:ub_index];
    }
    else {
        [_sg_mtl_cmd_encoder setFragmentBufferOffset:_sg_mtl_cur_ub_offset atIndex:ub_index];
    }
    _sg_mtl_cur_ub_offset = _sg_mtl_roundup(_sg_mtl_cur_ub_offset + num_bytes, _SG_MTL_UB_ALIGN);
}

_SOKOL_PRIVATE void _sg_draw(int base_element, int num_elements, int num_instances) {
    SOKOL_ASSERT(_sg_mtl_in_pass);
    if (!_sg_mtl_pass_valid) {
        return;
    }
    SOKOL_ASSERT(_sg_mtl_cmd_encoder);
    SOKOL_ASSERT(_sg_mtl_cur_pipeline && (_sg_mtl_cur_pipeline->slot.id == _sg_mtl_cur_pipeline_id.id));
    if (SG_CULLMODE_NONE != _sg_mtl_cur_pipeline->index_type) {
        /* indexed rendering */
        SOKOL_ASSERT(_sg_mtl_cur_indexbuffer && (_sg_mtl_cur_indexbuffer->slot.id == _sg_mtl_cur_indexbuffer_id.id));
        const _sg_buffer* ib = _sg_mtl_cur_indexbuffer;
        const NSUInteger index_buffer_offset = base_element * _sg_mtl_cur_pipeline->mtl_index_size;
        [_sg_mtl_cmd_encoder drawIndexedPrimitives:_sg_mtl_cur_pipeline->mtl_prim_type
            indexCount:num_elements
            indexType:_sg_mtl_cur_pipeline->mtl_index_type
            indexBuffer:_sg_mtl_pool[ib->mtl_buf[ib->active_slot]]
            indexBufferOffset:index_buffer_offset
            instanceCount:num_instances];
    }
    else {
        /* non-indexed rendering */
        [_sg_mtl_cmd_encoder drawPrimitives:_sg_mtl_cur_pipeline->mtl_prim_type
            vertexStart:base_element
            vertexCount:num_elements
            instanceCount:num_instances];
    }
}

_SOKOL_PRIVATE void _sg_update_buffer(_sg_buffer* buf, const void* data, int data_size) {
    SOKOL_ASSERT(buf && data && (data_size > 0));
    // FIXME
}

#ifdef __cplusplus
} /* extern "C" */
#endif
