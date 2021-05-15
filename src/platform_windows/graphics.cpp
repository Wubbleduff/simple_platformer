
#include "graphics.h"
#include "logging.h"
#include "platform.h"
#include "game_math.h"
#include "shader.h"

#include "platform_windows/platform_windows.h"
#include "examples/imgui_impl_win32.h"
#include "examples/imgui_impl_opengl3.h"
#include "GL/glew.h"
#include "GL/wglew.h"
#include <windows.h>
#include <assert.h>

#include "levels.h"



using namespace GameMath;



struct CameraState
{
    v2 position = v2();
    float width = 10.0f;
};

struct ObjectBuffer
{
    GLuint vao;
    GLuint vbo;
    int bytes_capacity;
};

struct Texture
{
    GLuint handle;
};

struct GraphicsState
{
    HGLRC gl_context;

    float screen_aspect_ratio;

    Texture *white_texture;

    Shader *batch_quad_shader;
    ObjectBuffer *batch_quad_buffer;
    struct QuadRenderingData
    {
        v2 position;
        v2 half_extents;
        v4 color;
        float rotation;
    };

    void render_quad_batch(std::vector<QuadRenderingData> *packed_data);

    struct LayerGroup
    {
        std::vector<GraphicsState::QuadRenderingData> quads_packed_buffer;
        void pack_quad(QuadRenderingData *quad)
        {
            quads_packed_buffer.push_back({quad->position, quad->half_extents, quad->color, quad->rotation});
        }
    };
    std::map<int, LayerGroup *> *objects_to_render;
};
GraphicsState *Graphics::instance = nullptr;
CameraState *Graphics::Camera::instance = nullptr;

static const int TARGET_GL_VERSION[2] = { 4, 4 }; // {major, minor}




static void check_gl_errors(const char *desc)
{
    GLint error = glGetError();
    if(error)
    {
        Log::log_error("Error %i: %s\n", error, desc);
        assert(false);
    }
}

static void create_gl_context(GraphicsState *instance)
{
    HDC dc = Windows::device_context();

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixel_format = ChoosePixelFormat(dc, &pfd);
    if(pixel_format == 0) return;

    BOOL result = SetPixelFormat(dc, pixel_format, &pfd);
    if(!result) return;

    HGLRC temp_context = wglCreateContext(dc);
    wglMakeCurrent(dc, temp_context);



    glewExperimental = true;
    GLenum error = glewInit();
    if(error != GLEW_OK)
    {
        OutputDebugString("GLEW is not initialized!");
    }



    int attribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, TARGET_GL_VERSION[0],
        WGL_CONTEXT_MINOR_VERSION_ARB, TARGET_GL_VERSION[1],
        WGL_CONTEXT_FLAGS_ARB, 0,
        0
    };

    if(wglewIsSupported("WGL_ARB_create_context") == 1)
    {
        instance->gl_context = wglCreateContextAttribsARB(dc, 0, attribs);
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(temp_context);
        wglMakeCurrent(dc, instance->gl_context);
    }
    else
    {   //It's not possible to make a GL 3.x context. Use the old style context (GL 2.1 and before)
        instance->gl_context = temp_context;
    }

    //Checking GL version
    const GLubyte *GLVersionString = glGetString(GL_VERSION);

    //Or better yet, use the GL3 way to get the version number
    int OpenGLVersion[2];
    glGetIntegerv(GL_MAJOR_VERSION, &OpenGLVersion[0]);
    glGetIntegerv(GL_MINOR_VERSION, &OpenGLVersion[1]);

    if(!instance->gl_context) return; // Bad
}



static Texture *make_texture_from_bitmap(int width, int height, char *bitmap)
{
    Texture *texture = new Texture();

    glGenTextures(1, &texture->handle);
    glBindTexture(GL_TEXTURE_2D, texture->handle);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    check_gl_errors("make texture");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap);
    check_gl_errors("send texture data");

    glGenerateMipmap(GL_TEXTURE_2D);
    check_gl_errors("generate mipmap");

    return texture;
}

static void use_texture(Texture *texture)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->handle);
}

void GraphicsState::render_quad_batch(std::vector<GraphicsState::QuadRenderingData> *packed_data)
{
    if(packed_data->empty()) return;

    Shader *shader = batch_quad_shader;
    ObjectBuffer *object_buffer = batch_quad_buffer;

    use_shader(shader);
    mat4 project_m_world = Graphics::ndc_m_world();
    set_uniform(shader, "vp", project_m_world);

    glBindVertexArray(object_buffer->vao);
    check_gl_errors("use vao");
    glBindBuffer(GL_ARRAY_BUFFER, object_buffer->vbo);
    int bytes = sizeof((*packed_data)[0]) * packed_data->size();
    assert(bytes <= (object_buffer->bytes_capacity));
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, packed_data->data());
    check_gl_errors("send quad data");

    use_texture(white_texture);

    glFinish();

    glDrawArrays(GL_POINTS, 0, packed_data->size());
}




static GraphicsState::LayerGroup *get_or_add_layer_group(int layer)
{
    GraphicsState::LayerGroup *group = (*Graphics::instance->objects_to_render)[layer];
    if(group == nullptr)
    {
        group = new GraphicsState::LayerGroup();
        (*Graphics::instance->objects_to_render)[layer] = group;
    }

    return group;
}

void Graphics::quad(v2 position, v2 scale, float rotation, v4 color, int layer)
{
    GraphicsState::LayerGroup *group = get_or_add_layer_group(layer);
    GraphicsState::QuadRenderingData data = {position, scale, color, rotation};
    group->pack_quad(&data);
}






void Graphics::init()
{
    instance = new GraphicsState();

    create_gl_context(instance);

    Graphics::ImGuiImplementation::init();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Camera::instance = new CameraState();
    instance->screen_aspect_ratio = Platform::Window::aspect_ratio();

    instance->objects_to_render = new std::map<int, GraphicsState::LayerGroup *>();

    instance->batch_quad_shader = make_shader("assets/shaders/batch_quad.shader");

    // Make quad object buffer
    {
        instance->batch_quad_buffer = new ObjectBuffer();
        glGenVertexArrays(1, &instance->batch_quad_buffer->vao);
        glBindVertexArray(instance->batch_quad_buffer->vao);
        check_gl_errors("making vao");

        glGenBuffers(1, &instance->batch_quad_buffer->vbo);
        check_gl_errors("making vbo");

        static const int MAX_QUADS = 1024 * 10;
        glBindBuffer(GL_ARRAY_BUFFER, instance->batch_quad_buffer->vbo);
        glBufferData(GL_ARRAY_BUFFER, MAX_QUADS * sizeof(GraphicsState::QuadRenderingData), nullptr, GL_DYNAMIC_DRAW);
        check_gl_errors("send vbo data");
        instance->batch_quad_buffer->bytes_capacity = MAX_QUADS * sizeof(GraphicsState::QuadRenderingData);

        float stride = sizeof(GraphicsState::QuadRenderingData);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0); // Position
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(1 * sizeof(v2))); // Scale
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void *)(2 * sizeof(v2))); // Color
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void *)(2 * sizeof(v2) + sizeof(v4))); // Rotation
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        check_gl_errors("vertex attrib pointer");
    }
    
    // Make texture
    {
        char bitmap[] = { (char)255, (char)255, (char)255, (char)255 };
        instance->white_texture = make_texture_from_bitmap(1, 1, bitmap);
    }
}

void Graphics::clear_frame(v4 color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Graphics::render()
{
    std::map<int, GraphicsState::LayerGroup *> *objects_to_render = instance->objects_to_render;

    for(std::pair<int, GraphicsState::LayerGroup *> pair : *objects_to_render)
    {
        GraphicsState::LayerGroup *group = pair.second;

        // Render all quads
        instance->render_quad_batch(&group->quads_packed_buffer);
        group->quads_packed_buffer.clear();
    }

}

void Graphics::swap_frames()
{
    glFlush();
    //glFinish();

    HDC dc = Windows::device_context();
    SwapBuffers(dc);
}



GameMath::mat4 Graphics::view_m_world()
{
    return make_translation_matrix(v3(-Camera::instance->position, 0.0f));
}

GameMath::mat4 Graphics::world_m_view()
{
    return inverse(view_m_world());
}

GameMath::mat4 Graphics::ndc_m_world()
{
    mat4 ndc_m_view =
    {
        2.0f / Camera::width(), 0.0f, 0.0f, 0.0f,
        0.0f, (2.0f / Camera::width()) * instance->screen_aspect_ratio, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    return ndc_m_view * view_m_world();
}

GameMath::mat4 Graphics::world_m_ndc()
{
    return inverse(ndc_m_world());
}



GameMath::v2 &Graphics::Camera::position()
{
    return instance->position;
}

float &Graphics::Camera::width()
{
    return instance->width;
}






// ImGui Implementation
void Graphics::ImGuiImplementation::init()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    
    // Setup Platform/Renderer bindings
    ImGui_ImplOpenGL3_Init("#version 440 core");
    ImGui_ImplWin32_Init(Windows::handle());
}

void Graphics::ImGuiImplementation::new_frame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Graphics::ImGuiImplementation::end_frame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Graphics::ImGuiImplementation::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}


