/*
 * Copyright (c) 2023, Max Rodriguez. All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license. You should have received a copy of this license along
 * with this source code in a file named "COPYING".
 *
 * @file example.cxx
 * @author Max Rodriguez
 * @date 2023-06-05
 */

#include "example.hxx"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#ifdef HAVE_PANDA
#include <panda3d-webgl/pandaFramework.h>
#include <panda3d-webgl/pandaSystem.h>
#include <panda3d-webgl/windowProperties.h>
#include <panda3d-webgl/genericAsyncTask.h>
#include <panda3d-webgl/asyncTaskManager.h>
#include <panda3d-webgl/load_prc_file.h>
#include <panda3d-webgl/cardMaker.h>
#include <panda3d-webgl/pandaNode.h>
#include <panda3d-webgl/nodePath.h>
#include <panda3d-webgl/pgButton.h>
#include <panda3d-webgl/mouseButton.h>
#endif

using namespace astron;

void MyApp::run(std::string astron_addr)
{
    connect(astron_addr, (uint32_t)0x5d7939, "v0.0.0");
#ifdef HAVE_PANDA
    // have panda call our astron implementation every frame
    taskMgr->add(new GenericAsyncTask("Calls poll_datagram()", &astron_poll, this));
#else
    poll_forever(); // have the library create an emscripten main loop for you
#endif
}

#ifdef HAVE_PANDA
AsyncTask::DoneStatus MyApp::astron_poll(GenericAsyncTask *task, void *data)
{
    MyApp* app = static_cast<MyApp*>(data);
    app->poll_till_empty();
    return AsyncTask::DS_cont; // repeat next panda frame
}

/* The following Panda task is a hack to resize
 * the screen to the browser screen dimensions. */

AsyncTask::DoneStatus MyApp::resize_panda_window(GenericAsyncTask *task, void *data)
{
    const int frames = task->get_elapsed_frames();
    if (frames % 5 != 0) return AsyncTask::DS_cont; // only check every 5 frames

    MyApp* app = static_cast<MyApp*>(data);
    double b_width;
    double b_height;
    emscripten_get_element_css_size("canvas", &b_width, &b_height);

    if ((b_width != app->_win_width) || (b_height != app->_win_height))
    {
        app->_win_width = b_width;
        app->_win_height = b_height;

        WindowProperties winProps = WindowProperties();
        winProps.set_size(int(b_width), int(b_height));
        app->p_window->get_graphics_window()->request_properties(winProps);
    }
    return AsyncTask::DS_cont;
}

AsyncTask::DoneStatus MyApp::update_a2d_nodes_to_aspect_ratio(GenericAsyncTask *task, void *data)
{
    MyApp* app = static_cast<MyApp*>(data);

    // get graphics window dimensions for aspect ratio
    int x_size = app->p_window->get_graphics_window()->get_sbs_left_x_size();
    int y_size = app->p_window->get_graphics_window()->get_sbs_left_y_size();
    float aspect_ratio = static_cast<float>(x_size) / static_cast<float>(y_size);

    app->a2dLeft = aspect_ratio * -1.0f;
    app->a2dRight = aspect_ratio;

    // update their corresponding coordinates
    app->a2dTopCenter.set_pos(0.0f, 0.0f, app->a2dTop);
    app->a2dBottomCenter.set_pos(0.0f, 0.0f, app->a2dBottom);
    app->a2dLeftCenter.set_pos(app->a2dLeft, 0.0f, 0.0f);
    app->a2dRightCenter.set_pos(app->a2dRight, 0.0f, 0.0f);
    app->a2dTopLeft.set_pos(app->a2dLeft, 0.0f, app->a2dTop);
    app->a2dTopRight.set_pos(app->a2dRight, 0.0f, app->a2dTop);
    app->a2dBottomLeft.set_pos(app->a2dLeft, 0.0f, app->a2dBottom);
    app->a2dBottomRight.set_pos(app->a2dRight, 0.0f, app->a2dBottom);

    return AsyncTask::DS_cont;
}

void MyApp::gui_button_toggle_fullscreen_callback(const Event *ev, void *data)
{
    MyApp* app = static_cast<MyApp*>(data);

    const bool fullscreen_enabled = app->p_window->get_graphics_window()->is_fullscreen();

    WindowProperties winProps = WindowProperties();
    winProps.set_fullscreen(!fullscreen_enabled); // toggle fullscreen
    app->p_window->get_graphics_window()->request_properties(winProps);
};
#endif // HAVE_PANDA


int main(int argc, char* argv[])
{
    /* Disable color output. Not supported on most browsers. (Only Chrome afaik) */
    g_logger->set_color_enabled(false);

    /* It is VITAL that you dynamically allocate `MyApp` via `new`.
     * If you allocate it on the stack, it will be cleared from memory as
     * soon as the emscripten main loop starts at `poll_forever()`.
     * You can allocate it on the stack if you call `poll_datagram()`
     * in your own loop that is in the scope where `MyApp` is defined.
     */
    MyApp *app = new MyApp();

#ifdef HAVE_PANDA
    // Load PRC engine configuration file
    load_prc_file("engine-config.prc");
    // Initialize Panda libraries
    init_libOpenALAudio();
    init_libpnmimagetypes();
    init_libwebgldisplay();

    // Open a new framework & window
    app->m_framework.open_framework(argc, argv);
    app->m_framework.set_window_title("Panda Window"); // not ported on webgl port yet
    app->p_window = app->m_framework.open_window();
    app->p_window->enable_keyboard();
    app->camera = app->p_window->get_camera_group(); // get the camera and store it

    app->camera.set_pos(0, 0, 60);
    app->camera.look_at(0, 0, 0);

    /* Create our own 8 node paths (children of aspect2d) to represent the edges / corners of
     * the panda window, adjusted every frame by the window's aspect ratio.
     * This way, GUI elements that should be anchored to an edge / corner of the window
     * at all times should be parented to their corresponding a2dxxx node.
     *
     * These nodes are created by ShowBase in Panda's Python DIRECT API, but not in C++... So recreated it myself.
     */

    NodePath aspect2d = app->p_window->get_aspect_2d();

    // Attach new a2dxxx nodes to aspect2d
    app->a2dTopCenter = aspect2d.attach_new_node("a2dTopCenter");
    app->a2dBottomCenter = aspect2d.attach_new_node("a2dBottomCenter");
    app->a2dLeftCenter = aspect2d.attach_new_node("a2dLeftCenter");
    app->a2dRightCenter = aspect2d.attach_new_node("a2dRightCenter");
    app->a2dTopLeft = aspect2d.attach_new_node("a2dTopLeft");
    app->a2dTopRight = aspect2d.attach_new_node("a2dTopRight");
    app->a2dBottomLeft = aspect2d.attach_new_node("a2dBottomLeft");
    app->a2dBottomRight = aspect2d.attach_new_node("a2dBottomRight");

    // Add Panda task to update the scaling and coordinates of these nodes per frame
    taskMgr->add(new GenericAsyncTask("update a2d helper nodes coordinates",
                                      &MyApp::update_a2d_nodes_to_aspect_ratio, app));

    // Load the GUI elements
    PT(PGButton) toggle_fullscreen;
    toggle_fullscreen = new PGButton("toggle_fullscreen_button");
    toggle_fullscreen->setup("Toggle Fullscreen", 0);

    NodePath toggle_fullscreen_np = app->a2dTopRight.attach_new_node(toggle_fullscreen);
    toggle_fullscreen_np.set_scale(0.05);
    toggle_fullscreen_np.set_pos(-0.7, 0, -0.07);

    // Add button press callback
    app->m_framework.define_key(toggle_fullscreen->get_click_event(MouseButton::one()), "button press",
                                &MyApp::gui_button_toggle_fullscreen_callback, app);

    // Load the scene (panda's downloader pulls them via HTTP)
    NodePath scene = app->p_window->load_model(app->m_framework.get_models(), "resources/map.bam");
    scene.reparent_to(app->p_window->get_render());

    // Start our Panda tasks
    taskMgr->add(new GenericAsyncTask("Resizes Panda Window", &MyApp::resize_panda_window, app));
#endif

    app->run("127.0.0.1:8080"); // websocket proxy on loop back interface (proxying to client agent)

#ifdef HAVE_PANDA
    // start the framework main loop
    app->m_framework.main_loop();
    app->m_framework.close_framework();
#endif
    return 0;
}