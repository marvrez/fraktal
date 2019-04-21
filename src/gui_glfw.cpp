// Developed by Simen Haugo.
// See LICENSE.txt for copyright and licensing details (standard MIT License).

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <GL/gl3w.h>
#include <GL/gl3w.c>
#include <GLFW/glfw3.h>
#include <string.h>
#include <args.h>

// Required to link with vc2010 GLFW
#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#define IMGUI_IMPL_OPENGL_LOADER_GL3W
#include <imgui.cpp>
#include <imgui_draw.cpp>
#include <imgui_demo.cpp>
#include <imgui_widgets.cpp>
#include <imgui_impl_glfw.cpp>
#include <imgui_impl_opengl3.cpp>
#include <open_sans_regular.h>

#include "imgui_extensions.h"
#include "gui.h"

#define FRAKTAL_GUI
#define FRAKTAL_OMIT_GL_SYMBOLS
#include "fraktal.cpp"

void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

enum { NUM_GLFW_KEYS = 512 };
static struct glfw_key_t
{
    bool was_pressed;
    bool was_released;
    bool is_down;
} glfw_keys[NUM_GLFW_KEYS];

void mark_key_events_as_processed()
{
    for (int key = 0; key < NUM_GLFW_KEYS; key++)
    {
        glfw_keys[key].was_pressed = false;
        glfw_keys[key].was_released = false;
    }
}

void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key >= 0 && key < NUM_GLFW_KEYS)
    {
        bool was_down = glfw_keys[key].is_down;
        if (action == GLFW_PRESS && !was_down) {
            glfw_keys[key].was_pressed = true;
            glfw_keys[key].is_down = true;
        }
        if (action == GLFW_RELEASE && was_down) {
            glfw_keys[key].was_released = true;
            glfw_keys[key].is_down = false;
        }
        if (action == GLFW_REPEAT) {
            glfw_keys[key].was_pressed = true;
            glfw_keys[key].is_down = true;
        }
    }
}

struct guiSettings
{
    int width,height,x,y;
};

int g_window_pos_x = -1;
int g_window_pos_y = -1;

void write_settings_to_disk(const char *ini_filename, guiSettings s)
{
    FILE *f = fopen(ini_filename, "wt");
    if (!f)
        return;

    fprintf(f, "[FraktalWindow]\n");
    fprintf(f, "width=%d\n", s.width);
    fprintf(f, "height=%d\n", s.height);
    fprintf(f, "x=%d\n", s.x);
    fprintf(f, "y=%d\n", s.y);
    fprintf(f, "\n");

    size_t imgui_ini_size = 0;
    const char *imgui_ini_data = ImGui::SaveIniSettingsToMemory(&imgui_ini_size);
    fwrite(imgui_ini_data, sizeof(char), imgui_ini_size, f);

    fclose(f);
}

void read_settings_from_disk(const char *ini_filename, guiSettings *s)
{
    char *f = read_file(ini_filename);
    if (!f)
        return;

    char *data = f;
    char *line = read_line(&data);
    bool fraktal = false;
    int width,height,x,y;
    while (line)
    {
        if (*line == '\0') ; // skip blanks
        else if (0 == strcmp(line, "[FraktalWindow]")) { fraktal = true; }
        else if (fraktal && 1 == sscanf(line, "width=%d", &width)) s->width = width;
        else if (fraktal && 1 == sscanf(line, "height=%d", &height)) s->height = height;
        else if (fraktal && 1 == sscanf(line, "x=%d", &x)) s->x = x;
        else if (fraktal && 1 == sscanf(line, "y=%d", &y)) s->y = y;
        else break;
        line = read_line(&data);
    }

    // the rest of the ini file is ImGui settings
    ImGui::LoadIniSettingsFromMemory(data);

    free(f);
}

void glfw_window_pos_callback(GLFWwindow *window, int x, int y)
{
    g_window_pos_x = x;
    g_window_pos_y = y;
}

int main(int argc, char **argv)
{
    const char *ini_filename = "fraktal.ini";

    guiSceneDef def = {0};
    arg_int32(&def.resolution_x,          200,                               "-width",    "Render resolution (x)");
    arg_int32(&def.resolution_y,          200,                               "-height",   "Render resolution (y)");
    arg_string(&def.model_kernel_path,    "./examples/vase.f",               "-model",    "Path to a .f kernel containing model definition");
    arg_string(&def.color_kernel_path,    "./libf/publication.f",            "-color",    "Path to a .f kernel containing color renderer definition");
    arg_string(&def.compose_kernel_path,  "./libf/mean_and_gamma_correct.f", "-compose",  "Path to a .f kernel containing color composer definition");
    arg_string(&def.geometry_kernel_path, "./libf/geometry.f",               "-geometry", "Path to a .f kernel containing geometry renderer definition");
    if (!arg_parse(argc, argv))
    {
        arg_help();
        return 1;
    }

    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowBorderSize = 0.0f;

    guiSettings settings = {0};
    settings.width = 800;
    settings.height = 600;
    settings.x = -1;
    settings.y = -1;
    ImGui::GetIO().IniFilename = NULL; // override ImGui load/save ini behavior with our own
    read_settings_from_disk(ini_filename, &settings);

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return EXIT_FAILURE;

    #if __APPLE__
    def.glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
    #else
    def.glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    #endif

    // sanitize settings
    if (settings.width <= 1) settings.width = 800;
    if (settings.height <= 1) settings.height = 600;

    if (settings.x >= 0 && settings.y >= 0)
        glfwWindowHint(GLFW_VISIBLE, false);

    GLFWwindow* window = glfwCreateWindow(settings.width, settings.height, "fraktal", NULL, NULL);
    if (window == NULL)
        return EXIT_FAILURE;

    if (settings.x >= 0 && settings.y >= 0)
    {
        glfwSetWindowPos(window, settings.x, settings.y);
        glfwShowWindow(window);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetWindowPosCallback(window, glfw_window_pos_callback);

    if (gl3wInit() != 0)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(def.glsl_version);

    ImVector<ImWchar> glyph_ranges; // this must persist until call to GetTexData
    {
        const char *data = (const char*)open_sans_regular_compressed_data;
        const unsigned int size = open_sans_regular_compressed_size;
        float height = 16.0f;

        ImGuiIO &io = ImGui::GetIO();

        io.Fonts->AddFontFromMemoryCompressedTTF(data, size, height);

        // add math symbols with a different font size
        ImFontConfig config;
        config.MergeMode = true;
        ImFontGlyphRangesBuilder builder;
        builder.AddText("\xce\xb8\xcf\x86\xe0\x04"); // theta, phi
        builder.BuildRanges(&glyph_ranges);
        io.Fonts->AddFontFromMemoryCompressedTTF(data, size, 18.0f, &config, glyph_ranges.Data);
    }

    fraktal_share_context();

    guiState scene = {0};
    if (!gui_load(scene, def))
    {
        fprintf(stderr, "Failed to load scene. Make sure that your executable can access the data directory in the Fraktal repository, or to any overridden kernel paths.");
        return 1;
    }

    while (!glfwWindowShouldClose(window) && !scene.should_exit)
    {
        static int settle_frames = 5;
        if (scene.auto_render || settle_frames > 0)
        {
            glfwPollEvents();
        }
        else
        {
            glfwWaitEvents();
            settle_frames = 5;
        }

        const double max_redraw_rate = 60.0;
        const double min_redraw_time = 1.0/max_redraw_rate;
        static double t_last_redraw = -min_redraw_time;
        double t_curr = glfwGetTime();
        double t_delta = t_curr - t_last_redraw;
        bool should_redraw = false;
        if (t_delta >= min_redraw_time)
        {
            t_last_redraw = t_curr;
            should_redraw = true;
        }

        if (should_redraw)
        {
            settle_frames--;
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            #define copy_key_event(struct_name, glfw_key) \
                scene.keys.struct_name.pressed = glfw_keys[glfw_key].was_pressed && !ImGui::GetIO().WantCaptureKeyboard; \
                scene.keys.struct_name.released = glfw_keys[glfw_key].was_released && !ImGui::GetIO().WantCaptureKeyboard; \
                scene.keys.struct_name.down = glfw_keys[glfw_key].is_down && !ImGui::GetIO().WantCaptureKeyboard;
            copy_key_event(Enter, GLFW_KEY_ENTER);
            copy_key_event(Space, GLFW_KEY_SPACE);
            copy_key_event(Ctrl, GLFW_KEY_LEFT_CONTROL);
            copy_key_event(Alt, GLFW_KEY_LEFT_ALT);
            copy_key_event(Shift, GLFW_KEY_LEFT_SHIFT);
            copy_key_event(Left, GLFW_KEY_LEFT);
            copy_key_event(Right, GLFW_KEY_RIGHT);
            copy_key_event(Up, GLFW_KEY_UP);
            copy_key_event(Down, GLFW_KEY_DOWN);
            copy_key_event(W, GLFW_KEY_W);
            copy_key_event(A, GLFW_KEY_A);
            copy_key_event(S, GLFW_KEY_S);
            copy_key_event(D, GLFW_KEY_D);
            copy_key_event(P, GLFW_KEY_P);
            copy_key_event(PrintScreen, GLFW_KEY_PRINT_SCREEN);
            mark_key_events_as_processed();

            glfwMakeContextCurrent(window);
            int window_fb_width, window_fb_height;
            glfwGetFramebufferSize(window, &window_fb_width, &window_fb_height);
            glViewport(0, 0, window_fb_width, window_fb_height);
            glClearColor(0.14f, 0.14f, 0.14f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            gui_present(scene);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (ImGui::GetIO().WantSaveIniSettings)
            {
                // update settings
                settings.x = g_window_pos_x;
                settings.y = g_window_pos_y;
                glfwGetWindowSize(window, &settings.width, &settings.height);
                write_settings_to_disk(ini_filename, settings);
                ImGui::GetIO().WantSaveIniSettings = false;
            }

            glfwSwapBuffers(window);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
