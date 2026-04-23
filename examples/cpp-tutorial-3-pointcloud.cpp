// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

///////////////////////////////////////////////////////
// librealsense tutorial #3 - Point cloud generation //
///////////////////////////////////////////////////////

// First include the librealsense C++ header file
#include <librealsense/rs.hpp>
#include <cstdio>
#include <vector>
#include <cmath>

// Each depth pixel is tagged with one of four categories:
//   CAT_VALID      - measured depth within [min_depth, max_depth]; holes filled by neighbours
//   CAT_TOO_CLOSE  - non-zero raw depth below min_depth (camera's near blind-spot)
//   CAT_SHADOW     - zero raw depth adjacent to a valid surface (stereo occlusion edge)
//   CAT_BACKGROUND - zero raw depth with no nearby valid surface (open space / beyond range)
enum DepthCat : uint8_t { CAT_VALID = 0, CAT_TOO_CLOSE = 1, CAT_SHADOW = 2, CAT_BACKGROUND = 3 };

static std::vector<uint16_t> g_filled_depth;
static std::vector<DepthCat>  g_depth_cat;
static std::vector<uint16_t> g_prev_depth;
static std::vector<DepthCat> g_prev_cat;
static bool g_have_prev = false;

static void classify_depth(const uint16_t * src, int width, int height,
                            int kw = 5, int kh = 3,
                            uint16_t min_depth = 300, uint16_t max_depth = 4500,
                            int min_neighbors = 3,
                            uint16_t max_neighbor_delta = 160)
{
    const int n = width * height;
    g_filled_depth.resize(n);
    g_depth_cat.resize(n);
    const int kw2 = kw / 2, kh2 = kh / 2;
    for(int y = 0; y < height; ++y)
    {
        for(int x = 0; x < width; ++x)
        {
            uint16_t raw = src[y * width + x];
            uint16_t v;
            DepthCat cat;
            if(raw > 0 && raw < min_depth)
            {
                // Real surface, but too close for stereo to measure reliably
                cat = CAT_TOO_CLOSE;
                v = min_depth;  // place marker at the near-clip plane
            }
            else if(raw > max_depth)
            {
                // Beyond useful range
                cat = CAT_BACKGROUND;
                v = max_depth;
            }
            else if(raw > 0)
            {
                cat = CAT_VALID;
                v = raw;
            }
            else  // raw == 0
            {
                uint16_t local_min = 0;
                for(int dy = -kh2; dy <= kh2; ++dy)
                {
                    int ny = y + dy;
                    if(ny < 0 || ny >= height) continue;
                    for(int dx = -kw2; dx <= kw2; ++dx)
                    {
                        int nx = x + dx;
                        if(nx < 0 || nx >= width) continue;
                        uint16_t nb = src[ny * width + nx];
                        if(nb >= min_depth && nb <= max_depth)
                            if(local_min == 0 || nb < local_min) local_min = nb;
                    }
                }

                int close_count = 0;
                uint16_t best = 0;
                if(local_min)
                {
                    for(int dy = -kh2; dy <= kh2; ++dy)
                    {
                        int ny = y + dy;
                        if(ny < 0 || ny >= height) continue;
                        for(int dx = -kw2; dx <= kw2; ++dx)
                        {
                            int nx = x + dx;
                            if(nx < 0 || nx >= width) continue;
                            uint16_t nb = src[ny * width + nx];
                            if(nb >= min_depth && nb <= max_depth && (uint16_t)std::abs((int)nb - (int)local_min) <= max_neighbor_delta)
                            {
                                ++close_count;
                                if(best == 0 || nb < best) best = nb;
                            }
                        }
                    }
                }

                if(close_count >= min_neighbors) { cat = CAT_VALID;      v = best;      }
                else if(close_count > 0)         { cat = CAT_SHADOW;     v = best;      }  // occlusion edge
                else                       { cat = CAT_BACKGROUND; v = max_depth; }  // open space
            }
            g_filled_depth[y * width + x] = v;
            g_depth_cat   [y * width + x] = cat;
        }
    }
}

static void prune_speckles(int width, int height, int min_support = 2, uint16_t max_depth = 4500)
{
    std::vector<uint16_t> depth_out = g_filled_depth;
    std::vector<DepthCat> cat_out = g_depth_cat;

    for(int y = 1; y < height - 1; ++y)
    {
        for(int x = 1; x < width - 1; ++x)
        {
            const int idx = y * width + x;
            if(g_depth_cat[idx] != CAT_VALID) continue;

            int support = 0;
            for(int dy = -1; dy <= 1; ++dy)
            {
                for(int dx = -1; dx <= 1; ++dx)
                {
                    if(dx == 0 && dy == 0) continue;
                    const int nidx = (y + dy) * width + (x + dx);
                    if(g_depth_cat[nidx] == CAT_VALID) ++support;
                }
            }

            if(support < min_support)
            {
                bool near_surface = false;
                uint16_t best = 0;
                for(int dy = -1; dy <= 1; ++dy)
                {
                    for(int dx = -1; dx <= 1; ++dx)
                    {
                        if(dx == 0 && dy == 0) continue;
                        const int nidx = (y + dy) * width + (x + dx);
                        if(g_depth_cat[nidx] == CAT_VALID || g_depth_cat[nidx] == CAT_SHADOW)
                        {
                            near_surface = true;
                            uint16_t dv = g_filled_depth[nidx];
                            if(dv > 0 && (best == 0 || dv < best)) best = dv;
                        }
                    }
                }
                if(near_surface)
                {
                    cat_out[idx] = CAT_SHADOW;
                    depth_out[idx] = best ? best : g_filled_depth[idx];
                }
                else
                {
                    cat_out[idx] = CAT_BACKGROUND;
                    depth_out[idx] = max_depth;
                }
            }
        }
    }

    g_filled_depth.swap(depth_out);
    g_depth_cat.swap(cat_out);
}

static void temporal_stabilize(int width, int height, uint16_t max_neighbor_delta = 160)
{
    const int n = width * height;
    if(!g_have_prev || (int)g_prev_depth.size() != n)
    {
        g_prev_depth = g_filled_depth;
        g_prev_cat = g_depth_cat;
        g_have_prev = true;
        return;
    }

    for(int i = 0; i < n; ++i)
    {
        DepthCat c = g_depth_cat[i];
        DepthCat p = g_prev_cat[i];
        uint16_t d = g_filled_depth[i];
        uint16_t pd = g_prev_depth[i];

        // Hold onto the last plausible surface for one frame when a pixel blinks to background.
        if(c == CAT_BACKGROUND && (p == CAT_VALID || p == CAT_SHADOW))
        {
            c = p;
            d = pd;
        }
        // If a pixel oscillates between valid and shadow at similar depth, keep it valid.
        else if(c == CAT_SHADOW && p == CAT_VALID && (uint16_t)std::abs((int)d - (int)pd) <= max_neighbor_delta)
        {
            c = CAT_VALID;
            d = (uint16_t)((3 * (int)d + 2 * (int)pd) / 5);
        }
        else if(c == CAT_VALID && p == CAT_VALID)
        {
            d = (uint16_t)((3 * (int)d + 2 * (int)pd) / 5);
        }

        g_depth_cat[i] = c;
        g_filled_depth[i] = d;
    }

    g_prev_depth = g_filled_depth;
    g_prev_cat = g_depth_cat;
}

// Also include GLFW to allow for graphical display
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

double yaw, pitch, lastX, lastY; int ml;

struct view_tuning
{
    float fov_deg = 58.0f;
    float zoom_mul = 1.0f;
    float camera_scale = 1.9f;
    float camera_bias = 0.22f;
    float near_clip = 0.02f;
    float far_clip = 30.0f;
};

static void print_tuning_json(const view_tuning & t)
{
    printf("{\"fov_deg\":%.2f,\"zoom_mul\":%.3f,\"camera_scale\":%.3f,\"camera_bias\":%.3f,\"near_clip\":%.3f,\"far_clip\":%.2f}\n",
           t.fov_deg, t.zoom_mul, t.camera_scale, t.camera_bias, t.near_clip, t.far_clip);
}

static bool key_pressed_once(GLFWwindow * win, int key)
{
    static std::vector<unsigned char> was_down(GLFW_KEY_LAST + 1, 0);
    if(key < 0 || key > GLFW_KEY_LAST) return false;
    const int state = glfwGetKey(win, key);
    const unsigned char is_down = (state == GLFW_PRESS || state == GLFW_REPEAT) ? 1 : 0;
    const bool pressed = (is_down && !was_down[key]);
    was_down[key] = is_down;
    return pressed;
}

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

    const int stream_width = 640, stream_height = 480;
    dev->enable_stream(rs::stream::depth, stream_width, stream_height, rs::format::z16, 30);

    rs::stream texture_stream = rs::stream::color;
    bool use_color_texture = true;
    bool has_ir = false;  // separate infrared stream available alongside colour
    try
    {
        dev->enable_stream(texture_stream, stream_width, stream_height, rs::format::rgb8, 30);
        printf("Using COLOR texture stream at %dx%d.\n", stream_width, stream_height);
        // Also enable infrared so we can texture shadow/background regions with real IR data
        try
        {
            dev->enable_stream(rs::stream::infrared, stream_width, stream_height, rs::format::y8, 30);
            has_ir = true;
            printf("Infrared stream also enabled for shadow/background texturing.\n");
        }
        catch(const rs::error &) { /* infrared alongside colour is best-effort */ }
    }
    catch(const rs::error &)
    {
        texture_stream = rs::stream::infrared;
        use_color_texture = false;
        // has_ir stays false; we alias texture_image as ir_image below
        dev->enable_stream(texture_stream, stream_width, stream_height, rs::format::y8, 30);
        printf("Color stream unavailable, using INFRARED texture stream at %dx%d.\n", stream_width, stream_height);
    }
    dev->start();

    // Open a GLFW window to display our output
    if(!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
    const int window_width = 1200;
    const int window_height = 900;
    GLFWwindow * win = glfwCreateWindow(window_width, window_height, "librealsense tutorial #3", nullptr, nullptr);
    if(!win) throw std::runtime_error("Failed to create GLFW window");
    glfwMakeContextCurrent(win);
    glfwSetWindowPos(win, 100, 100);
    glfwSetCursorPosCallback(win, on_cursor_pos);
    glfwSetMouseButtonCallback(win, on_mouse_button);

    view_tuning tuning;
    printf("Live controls: [/] FOV, -/= zoom out/in, R reset view+zoom+FOV\n");
    print_tuning_json(tuning);

    rs::float3 view_center = {0.0f, 0.0f, 0.8f};
    float view_radius = 0.45f;
    bool view_init = false;

    while(!glfwWindowShouldClose(win))
    {
        // Wait for new frame data
        glfwPollEvents();

        bool tuning_changed = false;
        if(key_pressed_once(win, GLFW_KEY_LEFT_BRACKET))
        {
            tuning.fov_deg = clamp(tuning.fov_deg - 1.0f, 35.0f, 95.0f);
            tuning_changed = true;
        }
        if(key_pressed_once(win, GLFW_KEY_RIGHT_BRACKET))
        {
            tuning.fov_deg = clamp(tuning.fov_deg + 1.0f, 35.0f, 95.0f);
            tuning_changed = true;
        }
        if(key_pressed_once(win, GLFW_KEY_MINUS))
        {
            tuning.zoom_mul = clamp(tuning.zoom_mul * 1.05f, 0.40f, 3.00f);
            tuning_changed = true;
        }
        if(key_pressed_once(win, GLFW_KEY_EQUAL))
        {
            tuning.zoom_mul = clamp(tuning.zoom_mul / 1.05f, 0.40f, 3.00f);
            tuning_changed = true;
        }
        if(key_pressed_once(win, GLFW_KEY_R))
        {
            yaw = pitch = 0;
            view_init = false;
            tuning.fov_deg = 58.0f;
            tuning.zoom_mul = 1.0f;
            tuning_changed = true;
        }
        if(tuning_changed) print_tuning_json(tuning);

        dev->wait_for_frames();

        // Retrieve our images
        const uint8_t * texture_image = (const uint8_t *)dev->get_frame_data(texture_stream);
        // ir_image: dedicated infrared stream when colour is active, otherwise reuse texture
        const uint8_t * ir_image = has_ir
            ? (const uint8_t *)dev->get_frame_data(rs::stream::infrared)
            : (use_color_texture ? nullptr : texture_image);

        // Retrieve camera parameters for mapping between depth and color
        rs::intrinsics depth_intrin = dev->get_stream_intrinsics(rs::stream::depth);
        rs::extrinsics depth_to_texture = dev->get_extrinsics(rs::stream::depth, texture_stream);
        rs::intrinsics texture_intrin = dev->get_stream_intrinsics(texture_stream);
        float scale = dev->get_depth_scale();

        // Classify depth and apply post-filters to reduce edge bleed, sparkle and isolated speckles
        classify_depth((const uint16_t *)dev->get_frame_data(rs::stream::depth),
                   depth_intrin.width, depth_intrin.height,
                   5, 3, 300, 4500, 3, 160);
        prune_speckles(depth_intrin.width, depth_intrin.height, 2, 4500);
        temporal_stabilize(depth_intrin.width, depth_intrin.height, 160);

        // Estimate cloud center and radius from a decimated set of valid points.
        // This gives a stable pivot so rotated views stay centered and framed.
        rs::float3 target_center = view_center;
        {
            double sx = 0.0, sy = 0.0, sz = 0.0;
            int cnt = 0;
            for(int dy = 0; dy < depth_intrin.height; dy += 2)
            {
                for(int dx = 0; dx < depth_intrin.width; dx += 2)
                {
                    const int idx = dy * depth_intrin.width + dx;
                    if(g_depth_cat[idx] != CAT_VALID) continue;

                    const float depth_in_meters = g_filled_depth[idx] * scale;
                    rs::float3 p = depth_intrin.deproject({(float)dx, (float)dy}, depth_in_meters);
                    sx += p.x; sy += p.y; sz += p.z;
                    ++cnt;
                }
            }
            if(cnt > 0)
            {
                target_center = {(float)(sx / cnt), (float)(sy / cnt), (float)(sz / cnt)};
            }
        }

        float target_radius = view_radius;
        {
            float max_r2 = 0.0f;
            float max_r2_inlier = 0.0f;
            for(int dy = 0; dy < depth_intrin.height; dy += 2)
            {
                for(int dx = 0; dx < depth_intrin.width; dx += 2)
                {
                    const int idx = dy * depth_intrin.width + dx;
                    if(g_depth_cat[idx] != CAT_VALID) continue;

                    const float depth_in_meters = g_filled_depth[idx] * scale;
                    rs::float3 p = depth_intrin.deproject({(float)dx, (float)dy}, depth_in_meters);
                    const float rx = p.x - target_center.x;
                    const float ry = p.y - target_center.y;
                    const float rz = p.z - target_center.z;
                    const float r2 = rx * rx + ry * ry + rz * rz;
                    if(r2 > max_r2) max_r2 = r2;

                    // Prefer inliers around the subject depth to avoid far outliers shrinking the cloud.
                    if(std::abs(rz) <= 0.60f && std::abs(rx) <= 0.85f && std::abs(ry) <= 0.85f)
                    {
                        if(r2 > max_r2_inlier) max_r2_inlier = r2;
                    }
                }
            }

            if(max_r2_inlier > 0.0f) target_radius = std::sqrt(max_r2_inlier);
            else if(max_r2 > 0.0f) target_radius = std::sqrt(max_r2);
        }

        if(!view_init)
        {
            view_center = target_center;
            view_radius = target_radius;
            view_init = true;
        }
        else
        {
            const float alpha = 0.15f;
            view_center.x = view_center.x * (1.0f - alpha) + target_center.x * alpha;
            view_center.y = view_center.y * (1.0f - alpha) + target_center.y * alpha;
            view_center.z = view_center.z * (1.0f - alpha) + target_center.z * alpha;
            view_radius = view_radius * (1.0f - alpha) + target_radius * alpha;
        }

        if(view_radius < 0.18f) view_radius = 0.18f;
        if(view_radius > 1.10f) view_radius = 1.10f;
        float camera_distance = (view_radius * tuning.camera_scale + tuning.camera_bias) * tuning.zoom_mul;
        if(camera_distance < 0.60f) camera_distance = 0.60f;
        if(camera_distance > 5.00f) camera_distance = 5.00f;

        // Set up a perspective transform in a space that we can rotate by clicking and dragging the mouse
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        int fb_width = window_width, fb_height = window_height;
        glfwGetFramebufferSize(win, &fb_width, &fb_height);
        glViewport(0, 0, fb_width, fb_height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        const float aspect = fb_height ? (float)fb_width / (float)fb_height : 1.0f;
        gluPerspective(tuning.fov_deg, aspect, tuning.near_clip, tuning.far_clip);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(0,0,0, 0,0,1, 0,-1,0);
        glTranslatef(0,0,camera_distance);
        glRotated(pitch, 1, 0, 0);
        glRotated(yaw, 0, 1, 0);
        glTranslatef(-view_center.x, -view_center.y, -view_center.z);

        // We will render our depth data as a set of points in 3D space
        glPointSize(2);
        glEnable(GL_DEPTH_TEST);
        glBegin(GL_POINTS);

        for(int dy=0; dy<depth_intrin.height; ++dy)
        {
            for(int dx=0; dx<depth_intrin.width; ++dx)
            {
                const int idx = dy * depth_intrin.width + dx;
                const uint16_t depth_value    = g_filled_depth[idx];
                const DepthCat cat            = g_depth_cat[idx];

                // Throttle background point density so the backdrop remains readable when rotating.
                if(cat == CAT_BACKGROUND && ((dx & 1) || (dy & 1))) continue;

                const float    depth_in_meters = depth_value * scale;

                // Deproject this pixel to a 3D point using its assigned depth
                rs::float2 depth_pixel = {(float)dx, (float)dy};
                rs::float3 depth_point = depth_intrin.deproject(depth_pixel, depth_in_meters);

                if(cat == CAT_TOO_CLOSE)
                {
                    // White: a real surface exists but is too close for stereo measurement
                    glColor3ub(255, 255, 255);
                }
                else if(cat == CAT_SHADOW)
                {
                    // Gray: stereo occlusion shadow — something is here but hidden from one camera.
                    // Tint with IR luminance (IR and depth share intrinsics on R200) for subtle structure.
                    uint8_t ir_v = ir_image ? ir_image[idx] : 128;
                    uint8_t g_val = (uint8_t)(ir_v / 2 + 64);  // mid-gray band 64-191
                    glColor3ub(g_val, g_val, g_val);
                }
                else if(cat == CAT_BACKGROUND)
                {
                    // Dim background plane at max range, textured with IR.
                    // IR illumination naturally fades with distance, giving depth cues.
                    uint8_t ir_v = ir_image ? ir_image[idx] : 60;
                    uint8_t bg   = (uint8_t)(ir_v / 3 + 20);   // dark band 20-105
                    glColor3ub(bg, bg, bg);
                }
                else  // CAT_VALID
                {
                    // Map to colour texture image coordinates
                    rs::float3 texture_point = depth_to_texture.transform(depth_point);
                    rs::float2 texture_pixel = texture_intrin.project(texture_point);
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
                        uint8_t v = texture_image[cy * texture_intrin.width + cx];
                        glColor3ub(v, v, v);
                    }
                }

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
