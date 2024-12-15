//------------------------------------------------------------------------------
//  quad-sapp.c
//  Simple 2D rendering with vertex- and index-buffer.
//------------------------------------------------------------------------------
// #include "quad-sapp.glsl.h"

#include <stdio.h>

#define CLOCK_MONOTONIC 0
#define _POSIX_C_SOURCE 199309L
#define SOKOL_TIME_IMPL
#include <sokol_time.h>

#define SOKOL_APP_IMPL
#define SOKOL_GLCORE
#include <sokol_app.h>

#define SOKOL_GFX_IMPL
#include <sokol_gfx.h>

#include "dbgui.h"
#include "quad-sapp.glsl.h"

#define SOKOL_GLUE_IMPL
#include "sokol_glue.h"

#define SOKOL_LOG_IMPL
#include "sokol_log.h"

static struct
{
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
} state;

void
init(void)
{
    sg_setup(&(sg_desc){
      .environment = sglue_environment(),
      .logger.func = slog_func,
    });
    __dbgui_setup(sapp_sample_count());

    // a vertex buffer
    // clang-format off
    float vertices[] = {
        // positions            colors
        -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,
    };
    // clang-format on
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
      .data = SG_RANGE(vertices), .label = "quad-vertices" });

    // an index buffer with 2 triangles
    uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };
    state.bind.index_buffer =
      sg_make_buffer(&(sg_buffer_desc){ .type  = SG_BUFFERTYPE_INDEXBUFFER,
					.data  = SG_RANGE(indices),
					.label = "quad-indices" });

    // a shader (use separate shader sources here
    sg_shader shd = sg_make_shader(quad_shader_desc(sg_query_backend()));

    // a pipeline state object
    state.pip = sg_make_pipeline(
      &(sg_pipeline_desc){ .shader     = shd,
			   .index_type = SG_INDEXTYPE_UINT16,
			   .layout = { .attrs = { [ATTR_quad_position].format =
						    SG_VERTEXFORMAT_FLOAT3,
						  [ATTR_quad_color0].format =
						    SG_VERTEXFORMAT_FLOAT4 } },
			   .label  = "quad-pipeline" });

    // default pass action
    state.pass_action = (sg_pass_action){
	.colors[0] = { .load_action = SG_LOADACTION_CLEAR,
		       .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f } }
    };
}

void
frame(void)
{
    sg_begin_pass(&(sg_pass){ .action	 = state.pass_action,
			      .swapchain = sglue_swapchain() });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 6, 1);
    __dbgui_draw();
    sg_end_pass();
    sg_commit();
}

void
cleanup(void)
{
    __dbgui_shutdown();
    sg_shutdown();
}

sapp_desc
sokol_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    return (sapp_desc){
	.init_cb	    = init,
	.frame_cb	    = frame,
	.cleanup_cb	    = cleanup,
	.event_cb	    = __dbgui_event,
	.width		    = 640,
	.height		    = 360,
	.window_title	    = "solo_game_01",
	.icon.sokol_default = true,
	.logger.func	    = slog_func,
    };
}
