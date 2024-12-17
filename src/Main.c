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
#include "sokol_fetch.h"

#include "quad-sapp.glsl.h"
#include "util/dbgui.h"
#include "util/fileutil.h"

// Define binding slots
#define SLOT_tex 0
#define SLOT_smp 1

typedef struct
{
    float x, y, z;    // position
    float u, v;	      // uv
    float r, g, b, a; // color
} vertex_t;

typedef
{
    vertex_t vertices[4];
    uint16_t indices[6];
}
quad_t;

static struct
{
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    uint8_t file_buffer[1024 * 1024 * 32]; // Adjust the size as needed
} state;

// Forward declarations
static void
fetch_callback(const sfetch_response_t*);

void
init(void)
{
    sg_setup(&(sg_desc){
      .environment = sglue_environment(),
      .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // Initialize Sokol Fetch
    sfetch_setup(&(sfetch_desc_t){
      .max_requests = 16,
      .num_channels = 1,
      .logger.func = slog_func,
    });

    // pass action for clearing the framebuffer to some color
    state.pass_action = (sg_pass_action){
	.colors[0] = { .load_action = SG_LOADACTION_CLEAR,
		       .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };

    /* Allocate an image handle, but don't actually initialize the image yet,
       this happens later when the asynchronous file load has finished.
       Any draw calls containing such an "incomplete" image handle
       will be silently dropped.
    */
    state.bind.images[IMG_tex] = sg_alloc_image();

    // a sampler object
    state.bind.samplers[SMP_smp] = sg_make_sampler(&(sg_sampler_desc){
      .min_filter = SG_FILTER_LINEAR,
      .mag_filter = SG_FILTER_LINEAR,
    });

    // a vertex buffer
    // clang-format off
    vertex_t vertices[] = {
        // Positions           // UVs        // Colors
        {-0.5f,  0.5f, 0.0f,   0.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f}, // Top-left
        { 0.5f,  0.5f, 0.0f,   1.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f}, // Top-right
        { 0.5f, -0.5f, 0.0f,   1.0f, 0.0f,   1.0f, 1.0f, 1.0f, 1.0f}, // Bottom-right
        {-0.5f, -0.5f, 0.0f,   0.0f, 0.0f,   1.0f, 1.0f, 1.0f, 1.0f}, // Bottom-left
    };
    // clang-format on
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
      .data = SG_RANGE(vertices), .label = "quad-vertices" });

    // an index buffer with 2 triangles
    uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };
    state.bind.index_buffer =
      sg_make_buffer(&(sg_buffer_desc){ .type = SG_BUFFERTYPE_INDEXBUFFER,
					.data = SG_RANGE(indices),
					.label = "quad-indices" });

    // a shader (use separate shader sources here
    sg_shader shd = sg_make_shader(quad_shader_desc(sg_query_backend()));

    // clang-format off
    // a pipeline state object
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = { .attrs = { 
            [ATTR_quad_position].format = SG_VERTEXFORMAT_FLOAT3,
            [ATTR_quad_texcoord0].format = SG_VERTEXFORMAT_FLOAT2,
            [ATTR_quad_color0].format = SG_VERTEXFORMAT_FLOAT4,
        }},
        .cull_mode = SG_CULLMODE_NONE,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .label = "quad-pipeline"
    });


    // default pass action
    state.pass_action = (sg_pass_action){
	.colors[0] = { 
        .load_action = SG_LOADACTION_CLEAR,
        .clear_value = { 1.0f, 0.0f, 1.0f, 1.0f },
    }
    };
    // clang-format on

    /* start loading the PNG file, we don't need the returned handle since
      we can also get that inside the fetch-callback from the response
      structure.
       - NOTE that we're not using the user_data member, since all required
	 state is in a global variable anyway
   */
    char path_buf[512];
    sfetch_send(&(sfetch_request_t){
      .path =
	fileutil_get_path("resources/uv_test.png", path_buf, sizeof(path_buf)),
      .callback = fetch_callback,
      .buffer = SFETCH_RANGE(state.file_buffer) });
}

/* The fetch-callback is called by sokol_fetch.h when the data is loaded,
   or when an error has occurred.
*/
static void
fetch_callback(const sfetch_response_t* response)
{
    if (response->fetched) {
	/* the file data has been fetched, since we provided a big-enough
	   buffer we can be sure that all data has been loaded here
	*/
	int png_width, png_height, num_channels;
	const int desired_channels = 4;
	stbi_uc* pixels = stbi_load_from_memory(response->data.ptr,
						(int)response->data.size,
						&png_width,
						&png_height,
						&num_channels,
						desired_channels);
	if (pixels) {
	    // ok, time to actually initialize the sokol-gfx texture
	    state.bind.images[SLOT_tex] = sg_alloc_image();
	    sg_init_image(
	      state.bind.images[SLOT_tex],
	      &(sg_image_desc){ .width = png_width,
				.height = png_height,
				.pixel_format = SG_PIXELFORMAT_RGBA8,
				.data.subimage[0][0] = {
				  .ptr = pixels,
				  .size = (size_t)(png_width * png_height * 4),
				} });
	    stbi_image_free(pixels);

	} else if (response->failed) {
	    // if loading the file failed, set clear color to red
	    state.pass_action = (sg_pass_action){
		.colors[0] = { .load_action = SG_LOADACTION_CLEAR,
			       .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } }
	    };

	    printf("fetch_callback: %s\n",
		   response->failed ? "failed" : "succeeded");
	}
    }
}

void
frame(void)
{
    // pump the sokol-fetch message queues, and invoke response callbacks
    sfetch_dowork();

    sg_begin_pass(&(sg_pass){ .action = state.pass_action,
			      .swapchain = sglue_swapchain() });

    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);

    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();
}

void
cleanup(void)
{
    __dbgui_shutdown();
    sfetch_shutdown();
    sg_shutdown();
}

sapp_desc
sokol_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    return (sapp_desc){
	.init_cb = init,
	.frame_cb = frame,
	.cleanup_cb = cleanup,
	.event_cb = __dbgui_event,
	.width = 640,
	.height = 360,
	.window_title = "sokol_game_01",
	.icon.sokol_default = true,
	.logger.func = slog_func,
    };
}
