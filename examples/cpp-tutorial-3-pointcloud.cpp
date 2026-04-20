// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

///////////////////////////////////////////////////////
// librealsense tutorial #3 - Point cloud generation //
///////////////////////////////////////////////////////

// First include the librealsense C++ header file
#include <librealsense/rs.hpp>
#include <cstdio>

// Also include GLFW to allow for graphical display
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

double yaw, pitch, lastX, lastY; int ml;
static void on_mouse_button(GLFWwindow * win, int button, int action, int mods)
{
    if(button == GLFW_MOUSE_BUTTON_LEFT) ml = action == GLFW_PRESS;
}
static double clamp(double val, double lo, double hi) { return val < lo ? lo : val > hi ? hi : val; }
static void on_cursor_pos(GLFWwindow * win, double x, double y)
{
    if(ml)
    {
        yaw = clamp(yaw - (x - lastX), -120, 120);
        pitch = clamp(pitch + (y - lastY), -80, 80);
    }
    lastX = x;
    lastY = y;
}

int main() try
{
    // Turn on logging. We can separately enable logging to console or to file, and use different severity filters for each.
    rs::log_to_console(rs::log_severity::warn);
    //rs::log_to_file(rs::log_severity::debug, "librealsense.log");

    // Create a context object. This object owns the handles to all connected realsense devices.
    rs::context ctx;
    printf("There are %d connected RealSense devices.\n", ctx.get_device_count());
    if(ctx.get_device_count() == 0) return EXIT_FAILURE;

    // This tutorial will access only a single device, but it is trivial to extend to multiple devices
    rs::device * dev = ctx.get_device(0);
    printf("\nUsing device 0, an %s\n", dev->get_name());
    printf("    Serial number: %s\n", dev->get_serial());
    printf("    Firmware version: %s\n", dev->get_firmware_version());

    // Use 320x240 to improve close-range coverage and reduce USB bandwidth.
    const int stream_width = 320, stream_height = 240;
    dev->enable_stream(rs::stream::depth, stream_width, stream_height, rs::format::z16, 30);

    rs::stream texture_stream = rs::stream::infrared;
    bool use_color_texture = false;
    dev->enable_stream(texture_stream, stream_width, stream_height, rs::format::y8, 30);
    printf("Using INFRARED texture stream at %dx%d.\n", stream_width, stream_height);
    dev->start();

    // Open a GLFW window to display our output
    if(!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
    GLFWwindow * win = glfwCreateWindow(640, 480, "librealsense tutorial #3", nullptr, nullptr);
    if(!win) throw std::runtime_error("Failed to create GLFW window");
    glfwMakeContextCurrent(win);
    glfwSetWindowPos(win, 100, 100);
    glfwSetCursorPosCallback(win, on_cursor_pos);
    glfwSetMouseButtonCallback(win, on_mouse_button);
    while(!glfwWindowShouldClose(win))
    {
        // Wait for new frame data
        glfwPollEvents();
        dev->wait_for_frames();

        // Retrieve our images
        const uint16_t * depth_image = (const uint16_t *)dev->get_frame_data(rs::stream::depth);
        const uint8_t * texture_image = (const uint8_t *)dev->get_frame_data(texture_stream);

        // Retrieve camera parameters for mapping between depth and color
        rs::intrinsics depth_intrin = dev->get_stream_intrinsics(rs::stream::depth);
        rs::extrinsics depth_to_texture = dev->get_extrinsics(rs::stream::depth, texture_stream);
        rs::intrinsics texture_intrin = dev->get_stream_intrinsics(texture_stream);
        float scale = dev->get_depth_scale();

        // Set up a perspective transform in a space that we can rotate by clicking and dragging the mouse
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60, (float)640/480, 0.01f, 20.0f);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(0,0,0, 0,0,1, 0,-1,0);
        glTranslatef(0,0,+0.5f);
        glRotated(pitch, 1, 0, 0);
        glRotated(yaw, 0, 1, 0);
        glTranslatef(0,0,-0.5f);

        // We will render our depth data as a set of points in 3D space
        glPointSize(2);
        glEnable(GL_DEPTH_TEST);
        glBegin(GL_POINTS);

        for(int dy=0; dy<depth_intrin.height; ++dy)
        {
            for(int dx=0; dx<depth_intrin.width; ++dx)
            {
                // Retrieve the 16-bit depth value and map it into a depth in meters
                uint16_t depth_value = depth_image[dy * depth_intrin.width + dx];
                float depth_in_meters = depth_value * scale;

                // Skip over pixels with a depth value of zero, which is used to indicate no data
                if(depth_value == 0) continue;

                // Map from pixel coordinates in the depth image to pixel coordinates in the texture image
                rs::float2 depth_pixel = {(float)dx, (float)dy};
                rs::float3 depth_point = depth_intrin.deproject(depth_pixel, depth_in_meters);
                rs::float3 texture_point = depth_to_texture.transform(depth_point);
                rs::float2 texture_pixel = texture_intrin.project(texture_point);

                // Use the nearest texture pixel, or pure white if this point falls outside the texture image
                const int cx = (int)std::round(texture_pixel.x), cy = (int)std::round(texture_pixel.y);
                if(cx < 0 || cy < 0 || cx >= texture_intrin.width || cy >= texture_intrin.height)
                {
                    glColor3ub(255, 255, 255);
                }
                else if(use_color_texture)
                {
                    glColor3ubv(texture_image + (cy * texture_intrin.width + cx) * 3);
                }
                else
                {
                    // Infrared y8 is 1 byte per pixel; map to greyscale
                    uint8_t v = texture_image[cy * texture_intrin.width + cx];
                    glColor3ub(v, v, v);
                }

                // Emit a vertex at the 3D location of this depth pixel
                glVertex3f(depth_point.x, depth_point.y, depth_point.z);
            }
        }
        glEnd();

        glfwSwapBuffers(win);
    }
    
    return EXIT_SUCCESS;
}
catch(const rs::error & e)
{
    // Method calls against librealsense objects may throw exceptions of type rs::error
    printf("rs::error was thrown when calling %s(%s):\n", e.get_failed_function().c_str(), e.get_failed_args().c_str());
    printf("    %s\n", e.what());
    return EXIT_FAILURE;
}
