//------------------------------------------------------------------------------
//  imgui-sapp.cc including demo multi-touch with imgui, and frame timing info
//------------------------------------------------------------------------------
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol_imgui.h"


static struct {
    uint64_t last_time;
    double min_raw_frame_time;
    double max_raw_frame_time;
    double avg_raw_frame_time;
    double min_rounded_frame_time;
    double max_rounded_frame_time;
    double avg_rounded_frame_time;
    size_t num_samples;
    float counter;
    sg_pass_action pass_action;
    sapp_touchpoint touches_init[SAPP_MAX_TOUCHPOINTS];
    sapp_touchpoint touches_drag[SAPP_MAX_TOUCHPOINTS];
    bool touch_active[SAPP_MAX_TOUCHPOINTS];
    int id_active_down = -1;
    int consecutive_events;
} state;

#define MAX_CONSECUTIVE_EVENTS 5

static void reset_minmax_frametimes(void) {
    state.max_raw_frame_time = 0;
    state.min_raw_frame_time = 1000.0;
    state.max_rounded_frame_time = 0;
    state.min_rounded_frame_time = 1000.0;
    state.num_samples = 0;
}


static bool show_test_window = false;

static sg_pass_action pass_action;

void init(void) {
    // setup sokol-gfx, sokol-time and sokol-imgui
    sg_desc desc = { };
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);
    stm_setup(); // sokol_time.h

    // use sokol-imgui with all default-options (we're not doing
    // multi-sampled rendering or using non-default pixel formats)
    simgui_desc_t simgui_desc = { };
    simgui_desc.logger.func = slog_func;
    simgui_setup(&simgui_desc);

    // initial clear color
    pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action.colors[0].clear_value = { 0.0f, 0.5f, 0.7f, 1.0f };
}

void frame(void) {
    double raw_frame_time = stm_sec(stm_laptime(&state.last_time));
    double rounded_frame_time = sapp_frame_duration();
    if (raw_frame_time > 0) {
        if (raw_frame_time < state.min_raw_frame_time) {
            state.min_raw_frame_time = raw_frame_time;
        }
        if (raw_frame_time > state.max_raw_frame_time) {
            state.max_raw_frame_time = raw_frame_time;
        }
    }
    if (rounded_frame_time > 0) {
        if (rounded_frame_time < state.min_rounded_frame_time) {
            state.min_rounded_frame_time = rounded_frame_time;
        }
        if (rounded_frame_time > state.max_rounded_frame_time) {
            state.max_rounded_frame_time = rounded_frame_time;
        }
    }
    state.avg_raw_frame_time = 
        (state.avg_raw_frame_time * state.num_samples + raw_frame_time) / (state.num_samples + 1);
    state.avg_rounded_frame_time = 
        (state.avg_rounded_frame_time * state.num_samples + rounded_frame_time) / (state.num_samples + 1);
    state.num_samples++;

    const int width = sapp_width();
    const int height = sapp_height();
    simgui_new_frame({ width, height, sapp_frame_duration(), sapp_dpi_scale() });

    static bool once = false;
    if (!once) {
        once = true;
        ImGui::GetStyle().ScaleAllSizes(10.0f);
    }

    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    ImGui::SetNextWindowPos((ImVec2){ 10, 10 }, ImGuiCond_Once, (ImVec2){0,0});
    ImGui::SetNextWindowSize((ImVec2){ 800, 0 }, ImGuiCond_Once);
    ImGui::Begin("Controls", 0, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar);
    static float f1 = 0.0f;
    static float f2 = 0.0f;
    ImGui::Text("Hello, world!");
    ImGui::SliderFloat("float1", &f1, 0.0f, 1.0f);
    ImGui::SliderFloat("float2", &f2, 0.0f, 1.0f);
    ImGui::ColorEdit3("clear color", &pass_action.colors[0].clear_value.r);
    if (ImGui::Button("Test Window")) show_test_window ^= 1;
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("w: %d, h: %d, dpi_scale: %.1f", sapp_width(), sapp_height(), sapp_dpi_scale());
    if (ImGui::Button(sapp_is_fullscreen() ? "Switch to windowed" : "Switch to fullscreen")) {
        sapp_toggle_fullscreen();
    }
    ImGui::Text("raw frame time:     (min: %.3f, max: %.3f, avg: %.3f, fps: %.1f) %.3fms",
        state.min_raw_frame_time * 1000.0,
        state.max_raw_frame_time * 1000.0,
        state.avg_raw_frame_time * 1000.0,
        1.0 / state.avg_raw_frame_time,
        raw_frame_time * 1000.0
        );
    ImGui::Text("rounded frame time: (min: %.3f, max: %.3f, avg: %.3f, fps: %.1f) %.3fms",
        state.min_rounded_frame_time * 1000.0,
        state.max_rounded_frame_time * 1000.0,
        state.avg_rounded_frame_time * 1000.0,
        1.0 / state.avg_rounded_frame_time,
        rounded_frame_time * 1000.0
        );
    if (ImGui::Button("Reset min/max times", (ImVec2){0,0})) {
        reset_minmax_frametimes();
    }
    ImGui::End();

    if (show_test_window) {
        ImGui::SetNextWindowPos(ImVec2(512, 0), ImGuiCond_Once);
        ImGui::ShowDemoWindow(); // it does not want to obey next window pos
    }

    // the sokol_gfx draw pass
    sg_pass pass = {};
    pass.action = pass_action;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    simgui_shutdown();
    sg_shutdown();
}

static bool is_touch_event(sapp_event_type t) {
    switch (t) {
        case SAPP_EVENTTYPE_TOUCHES_BEGAN:
        case SAPP_EVENTTYPE_TOUCHES_MOVED:
        case SAPP_EVENTTYPE_TOUCHES_ENDED:
        case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
            return true;
        default:
            return false;
    }
}

// void mylog(const char * str, uint32_t lineOrNumber) {
//     slog_func("mylog", 2, 0, str, lineOrNumber, nullptr, nullptr);
// }

void input(const sapp_event* event) {
    const sapp_event& ev = *event;
    if (is_touch_event(event->type)) {
        if (event->type == SAPP_EVENTTYPE_TOUCHES_BEGAN) {
            for (int i = 0; i < ev.num_touches; i++) {
                int id = ev.touches[i].identifier;
                if (!state.touch_active[id]) {
                    state.touches_init[id] = ev.touches[i];
                    state.touches_drag[id] = ev.touches[i];
                    state.touch_active[id] = true;

                    // raise the last active touch
                    if (state.id_active_down != -1) {
                        // mylog("BEGIN: Raising last id", state.id_active_down);
                        sapp_event ev_touch_up = ev;
                        ev_touch_up.type = SAPP_EVENTTYPE_TOUCHES_ENDED;
                        ev_touch_up.touches[0] = state.touches_drag[state.id_active_down];
                        simgui_handle_event(&ev_touch_up);
                    }
                    // lower the new active touch
                    {
                        sapp_event ev_touch_dn = ev;
                        ev_touch_dn.touches[0] = ev.touches[i];
                        simgui_handle_event(&ev_touch_dn);
                        // mylog("BEGIN: Lowering new active touch id", id);
                    }
                    state.id_active_down = id;
                    state.consecutive_events = 0;
                }
            }
        }
        if (event->type == SAPP_EVENTTYPE_TOUCHES_MOVED) {
            if (ev.num_touches == 1 || state.consecutive_events < MAX_CONSECUTIVE_EVENTS) {
            // First do the move event for the touch that is already down
                for (int i = 0; i < ev.num_touches; i++) {
                    int id = ev.touches[i].identifier;
                    if (id == state.id_active_down) {
                        // mylog("MOVED: Moving last id", state.id_active_down);
                        sapp_event ev_touch_move = ev;
                        ev_touch_move.touches[0] = ev.touches[i];
                        state.touches_drag[id] = ev.touches[i];
                        simgui_handle_event(&ev_touch_move);
                        state.consecutive_events++;
                    }
                }
            }
            // Now for any other events, raise and lower
            if (state.id_active_down == -1 ||  state.consecutive_events >= MAX_CONSECUTIVE_EVENTS) {
                int orig_id_active_down = state.id_active_down;
                for (int i = 0; i < ev.num_touches; i++) {
                    int id = ev.touches[i].identifier;
                    if (id != orig_id_active_down) {
                        // raise the active down at the new position
                        if (state.id_active_down != -1) {
                            // mylog("MOVED: Raising last id", state.id_active_down);
                            sapp_event ev_touch_up = ev;
                            ev_touch_up.type = SAPP_EVENTTYPE_TOUCHES_ENDED;
                            ev_touch_up.touches[0] = state.touches_drag[state.id_active_down];
                            simgui_handle_event(&ev_touch_up);
                        }
                        // restore the other touch active down to either original position (trick to maintain drag distance?), or use last position if that's janky
                        {
                            sapp_event ev_touch_dn = ev;
                            ev_touch_dn.type = SAPP_EVENTTYPE_TOUCHES_BEGAN;
                            // ev_touch_dn.touches[0] = state.touches_init[id]; // original - it's necessary or else we'll probably drag the window around
                            // ev_touch_dn.touches[0] = ev.touches[i]; // new position
                            // ev_touch_dn.touches[0] = state.touches_drag[id]; // last position

                            // hack for POC: Assume all controls are horizontal
                            ev_touch_dn.touches[0] = {
                                .pos_x = state.touches_drag[id].pos_x, // last x position
                                .pos_y = state.touches_init[id].pos_y // original y position
                            };

                            simgui_handle_event(&ev_touch_dn);
                        }
                        // now move it to the new position
                        {
                            sapp_event ev_touch_move = ev;
                            ev_touch_move.touches[0] = ev.touches[i];
                            simgui_handle_event(&ev_touch_move);
                            state.touches_drag[id] = ev.touches[i];
                        }
                        // mylog("MOVED: Lowered and moved id", id);
                        // and mark this as the active down touch for the next one
                        state.id_active_down = id;
                        state.consecutive_events = 0;
                    }
                }
            }
        }
        if (event->type == SAPP_EVENTTYPE_TOUCHES_ENDED || event->type == SAPP_EVENTTYPE_TOUCHES_CANCELLED) {
            for (int i = 0; i < ev.num_touches; i++) {
                int id = ev.touches[i].identifier;
                if (id == state.id_active_down) {
                    // mylog("ENDED/CANCELED: raising id", id);
                    sapp_event ev_touch_up = ev;
                    ev_touch_up.touches[0] = ev.touches[i];
                    simgui_handle_event(&ev_touch_up);
                    state.id_active_down = -1;
                }
                state.touch_active[id] = false;
            }
        }
    } else {
        simgui_handle_event(event);
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sapp_desc desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = input;
    desc.window_title = "Dear ImGui (sokol-app)";
    desc.ios_keyboard_resizes_canvas = false;
    desc.icon.sokol_default = true;
    desc.enable_clipboard = true;
    desc.logger.func = slog_func;
    desc.width = 1080;
    desc.height = 1080;
    desc.swap_interval = 0; // Note: mod sokol_app.h to allow this
    return desc;
}

