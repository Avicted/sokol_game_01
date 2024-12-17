//------------------------------------------------------------------------------
//  quad-sapp.c
//  Simple 2D rendering with vertex- and index-buffer.
//------------------------------------------------------------------------------
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define CLOCK_MONOTONIC 0
#define _POSIX_C_SOURCE 199309L
#define SOKOL_TIME_IMPL
#include <sokol_time.h>

#define SOKOL_APP_IMPL
#define SOKOL_GLCORE
#include <sokol_app.h>

#define SOKOL_GFX_IMPL
#include <sokol_gfx.h>

#define SOKOL_GLUE_IMPL
#include "sokol_glue.h"

#define SOKOL_LOG_IMPL
#include "sokol_log.h"

#define SOKOL_FETCH_IMPL
#include "quad-sapp.glsl.h"
#include "sokol_fetch.h"
#include "util/dbgui.h"
#include "util/fileutil.h"

// Define binding slots
#define SLOT_tex 0
#define SLOT_smp 1

typedef struct
{
    float x, y, z;     // position
    float u, v;        // uv
    float r, g, b, a;  // color
} vertex_t;

typedef struct
{
    float x, y;
} vec2_t;

typedef struct
{
    vertex_t vertices[4];
    uint16_t indices[6];
} quad_t;

#define MAX_QUADS 128

typedef struct
{
    quad_t quads[MAX_QUADS];
    int    num_quads;
} draw_frame_t;

draw_frame_t draw_frame;

static struct
{
    sg_pass_action pass_action;
    sg_pipeline    pip;
    sg_bindings    bind;
    uint8_t        file_buffer[1024 * 1024 * 32];  // Adjust the size as needed

    quad_t quad;
} state;

// Forward declarations
static void fetch_callback(const sfetch_response_t *);

void init(void)
{
    sg_setup(&(sg_desc){
        .buffer_pool_size = 128,  // Increase the buffer pool size
        .environment      = sglue_environment(),
        .logger.func      = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // Initialize Sokol Fetch
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 16,
        .num_channels = 1,
        .logger.func  = slog_func,
    });

    // Allocate an image handle
    state.bind.images[IMG_tex] = sg_alloc_image();

    // Create a sampler object
    state.bind.samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    });

    // Create a vertex buffer without initializing it with data
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .usage = SG_USAGE_DYNAMIC, .size = sizeof(vertex_t) * 4, .label = "quad-vertices"});

    // Create an index buffer with 2 triangles
    uint16_t indices[]      = {0, 1, 2, 0, 2, 3};
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER, .data = SG_RANGE(indices), .label = "quad-indices"});

    // clang-format off
    // Create the quad
    vertex_t vertices[] = {
	    // Positions            // UVs        // Colors
	    { -0.5f,  0.5f, 0.0f,   0.0f, 0.0f,   1.0f, 1.0f, 1.0f, 1.0f}, // Top-left
	    {  0.5f,  0.5f, 0.0f,   1.0f, 0.0f,   1.0f, 1.0f, 1.0f, 1.0f}, // Top-right
	    {  0.5f, -0.5f, 0.0f,   1.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f}, // Bottom-right
	    { -0.5f, -0.5f, 0.0f,   0.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f}, // Bottom-left
    };

    // clang-format on
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .usage = SG_USAGE_DYNAMIC,
        .size  = sizeof(vertices),
    });

    state.quad.vertices[0] = vertices[0];
    state.quad.vertices[1] = vertices[1];
    state.quad.vertices[2] = vertices[2];
    state.quad.vertices[3] = vertices[3];

    state.quad.indices[0] = indices[0];
    state.quad.indices[1] = indices[1];
    state.quad.indices[2] = indices[2];
    state.quad.indices[3] = indices[3];
    state.quad.indices[4] = indices[4];
    state.quad.indices[5] = indices[5];

    // Create a shader
    sg_shader shd = sg_make_shader(quad_shader_desc(sg_query_backend()));

    // Create a pipeline state object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader     = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout     = {.attrs =
                           {
                           [ATTR_quad_position].format  = SG_VERTEXFORMAT_FLOAT3,
                           [ATTR_quad_texcoord0].format = SG_VERTEXFORMAT_FLOAT2,
                           [ATTR_quad_color0].format    = SG_VERTEXFORMAT_FLOAT4,
                       }},
        .cull_mode  = SG_CULLMODE_NONE,
        .depth      = {.compare = SG_COMPAREFUNC_LESS_EQUAL, .write_enabled = true},
        .label      = "quad-pipeline"});

    // Default pass action
    state.pass_action = (sg_pass_action){.colors[0] = {
                                             .load_action = SG_LOADACTION_CLEAR,
                                             .clear_value = {0.0f, 0.0f, 0.0f, 1.0f},
                                         }};

    // Start loading the PNG file
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
        .path     = fileutil_get_path("resources/uv_test.png", path_buf, sizeof(path_buf)),
        .callback = fetch_callback,
        .buffer   = SFETCH_RANGE(state.file_buffer)});
}

/* The fetch-callback is called by sokol_fetch.h when the data is loaded,
   or when an error has occurred.
*/
static void fetch_callback(const sfetch_response_t *response)
{
    if (response->fetched)
    {
        /* the file data has been fetched, since we provided a big-enough
           buffer we can be sure that all data has been loaded here
        */
        int       png_width, png_height, num_channels;
        const int desired_channels = 4;
        stbi_uc  *pixels           = stbi_load_from_memory(response->data.ptr,
                                                (int)response->data.size,
                                                &png_width,
                                                &png_height,
                                                &num_channels,
                                                desired_channels);
        if (pixels)
        {
            // ok, time to actually initialize the sokol-gfx texture
            state.bind.images[SLOT_tex] = sg_alloc_image();
            sg_init_image(state.bind.images[SLOT_tex],
                          &(sg_image_desc){.width               = png_width,
                                           .height              = png_height,
                                           .pixel_format        = SG_PIXELFORMAT_RGBA8,
                                           .data.subimage[0][0] = {
                                               .ptr  = pixels,
                                               .size = (size_t)(png_width * png_height * 4),
                                           }});
            stbi_image_free(pixels);
        }
        else if (response->failed)
        {
            // if loading the file failed, set clear color to red
            state.pass_action =
                (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR,
                                               .clear_value = {1.0f, 0.0f, 0.0f, 1.0f}}};

            printf("fetch_callback: %s\n", response->failed ? "failed" : "succeeded");
        }
    }
}

void update(void)
{
    static vec2_t vel = {0.0025f, 0.004f};
    static vec2_t pos = {0.0f, 0.0f};

    // Bounce off the walls, take the quad size into account
    if ((pos.x > 0.5f) || (pos.x < -0.5f))
    {
        vel.x = -vel.x;
    }
    if ((pos.y > 0.5f) || (pos.y < -0.5f))
    {
        vel.y = -vel.y;
    }

    pos.x += vel.x;
    pos.y += vel.y;

    state.quad.vertices[0].x = -0.5f + pos.x;
    state.quad.vertices[0].y = 0.5f + pos.y;

    state.quad.vertices[1].x = 0.5f + pos.x;
    state.quad.vertices[1].y = 0.5f + pos.y;

    state.quad.vertices[2].x = 0.5f + pos.x;
    state.quad.vertices[2].y = -0.5f + pos.y;

    state.quad.vertices[3].x = -0.5f + pos.x;
    state.quad.vertices[3].y = -0.5f + pos.y;

    sg_update_buffer(state.bind.vertex_buffers[0],
                     &(sg_range){.ptr = state.quad.vertices, .size = sizeof(state.quad.vertices)});
}

void frame(void)
{
    update();

    // pump the sokol-fetch message queues, and invoke response callbacks
    sfetch_dowork();

    sg_begin_pass(&(sg_pass){.action = state.pass_action, .swapchain = sglue_swapchain()});

    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);

    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();
}

void cleanup(void)
{
    __dbgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    return (sapp_desc){
        .init_cb            = init,
        .frame_cb           = frame,
        .cleanup_cb         = cleanup,
        .event_cb           = __dbgui_event,
        .width              = 640,
        .height             = 360,
        .window_title       = "sokol_game_01",
        .icon.sokol_default = true,
        .logger.func        = slog_func,
    };
}
