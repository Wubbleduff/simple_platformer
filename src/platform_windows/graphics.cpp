
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



using namespace GameMath;



struct Camera
{
    v2 position;
    float width;
};

struct Vertex
{
    v2 pos;

    Vertex() {}
    Vertex(v2 p) : pos(p) { }
};

struct VertexUv
{
    v2 pos;
    v2 uv;

    VertexUv() {}
    VertexUv(v2 p, v2 u) : pos(p), uv(u) { }
};

struct Mesh
{
    GLuint vao;
    GLuint vbo;
    GLuint ebo;

    int num_indices;

    GLuint primitive_type;
};

struct MeshUv
{
    GLuint vao;
    GLuint vbo;
    GLuint ebo;

    int num_indices;
};

struct Texture
{
    GLuint handle;
};

struct GraphicsState
{
    HGLRC gl_context;

    Camera *camera;
    float screen_aspect_ratio;

    Mesh *quad_mesh;
    Mesh *line_mesh;
    Mesh *triangles_mesh;
    Mesh *circle_mesh;

    Shader *quad_shader;
    Shader *flat_color_shader;

    Texture *white_texture;
};
GraphicsState *Graphics::instance = nullptr;

static const int TARGET_GL_VERSION[2] = { 4, 4 }; // {major, minor}
static const int MAX_TRIANGLES = 256;
static const int CIRCLE_MESH_RESOLUTION = 256;




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

static Mesh *make_mesh(int num_vertices, Vertex *vertices, int num_indices, int *indices, GLuint primitive_type = GL_TRIANGLES)
{
    Mesh *mesh = new Mesh();

    mesh->num_indices = num_indices;
    mesh->primitive_type = primitive_type;

    glGenVertexArrays(1, &mesh->vao);
    glBindVertexArray(mesh->vao);
    check_gl_errors("making vao");

    glGenBuffers(1, &mesh->vbo);
    check_gl_errors("making vbo");

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, num_vertices * sizeof(Vertex), vertices, GL_STATIC_DRAW);
    check_gl_errors("send vbo data");

    float stride = sizeof(Vertex);

    // Position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);

    check_gl_errors("vertex attrib pointer");

    glGenBuffers(1, &mesh->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices * sizeof(*indices), indices, GL_STATIC_DRAW);

    return mesh;
}

static Mesh *make_uv_mesh(int num_vertices, VertexUv *vertices, int num_indices, int *indices, GLuint primitive_type = GL_TRIANGLES)
{
    Mesh *mesh = new Mesh();

    mesh->num_indices = num_indices;
    mesh->primitive_type = primitive_type;

    glGenVertexArrays(1, &mesh->vao);
    glBindVertexArray(mesh->vao);
    check_gl_errors("making vao");

    glGenBuffers(1, &mesh->vbo);
    check_gl_errors("making vbo");

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, num_vertices * sizeof(VertexUv), vertices, GL_STATIC_DRAW);
    check_gl_errors("send vbo data");

    float stride = sizeof(VertexUv);

    // Position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);

    // UV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(sizeof(v2)));
    glEnableVertexAttribArray(1);

    check_gl_errors("vertex attrib pointer");

    glGenBuffers(1, &mesh->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices * sizeof(*indices), indices, GL_STATIC_DRAW);

    return mesh;
}

static void set_mesh_vertex_buffer_data(Mesh *mesh, int num_vertices, Vertex *vertices)
{
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, num_vertices * sizeof(Vertex), vertices);
    glFinish();
    check_gl_errors("set mesh vertex buffer data");
}

static void set_mesh_index_buffer_data(Mesh *mesh, int num_indices, int *indices)
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, num_indices * sizeof(int), indices);
    glFinish();
    check_gl_errors("set mesh index buffer data");
}

static void use_mesh(Mesh *mesh)
{
    glBindVertexArray(mesh->vao);
    check_gl_errors("use vao");

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    check_gl_errors("use ebo");
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

static mat4 ndc_m_world(Camera *camera, float screen_aspect_ratio)
{
    mat4 view_m_world = make_translation_matrix(v3(-camera->position, 0.0f));
    mat4 ndc_m_view =
    {
        2.0f / camera->width, 0.0f, 0.0f, 0.0f,
        0.0f, (2.0f / camera->width) * screen_aspect_ratio, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    return ndc_m_view * view_m_world;
}



void Graphics::draw_quad(v2 position, v2 scale, float rotation, v4 color)
{
    Mesh *mesh = instance->quad_mesh;
    use_mesh(mesh);

    use_shader(instance->quad_shader);

    use_texture(instance->white_texture);

    mat4 world_m_model = make_translation_matrix(v3(position, 0.0f)) * make_z_axis_rotation_matrix(rotation) * make_scale_matrix(v3(scale, 1.0f));
    mat4 mvp = ndc_m_world(instance->camera, instance->screen_aspect_ratio) * world_m_model;
    set_uniform(instance->quad_shader, "mvp", mvp);
    set_uniform(instance->quad_shader, "blend_color", color);
    
    glDrawElements(mesh->primitive_type, mesh->num_indices, GL_UNSIGNED_INT, 0);

    check_gl_errors("draw");
}

void Graphics::draw_circle(v2 position, float radius, v4 color)
{
    Mesh *mesh = instance->circle_mesh;
    use_mesh(mesh);

    use_shader(instance->quad_shader);

    use_texture(instance->white_texture);

    mat4 world_m_model = make_translation_matrix(v3(position, 0.0f)) * make_scale_matrix(v3(radius, radius, 1.0f));
    mat4 mvp = ndc_m_world(instance->camera, instance->screen_aspect_ratio) * world_m_model;
    set_uniform(instance->quad_shader, "mvp", mvp);
    set_uniform(instance->quad_shader, "blend_color", color);
    
    glDrawElements(mesh->primitive_type, mesh->num_indices, GL_UNSIGNED_INT, 0);

    check_gl_errors("draw");
}

void Graphics::draw_line(v2 a, v2 b, v4 color)
{
    use_mesh(instance->triangles_mesh);

    Vertex v[] = { Vertex(a), Vertex(b) };
    int i[] = { 0, 1 };
    set_mesh_vertex_buffer_data(instance->triangles_mesh, 2, v);
    set_mesh_index_buffer_data(instance->triangles_mesh, 2, i);

    use_shader(instance->flat_color_shader);

    mat4 mvp = ndc_m_world(instance->camera, instance->screen_aspect_ratio);
    set_uniform(instance->quad_shader, "mvp", mvp);
    set_uniform(instance->quad_shader, "blend_color", color);

    // Why is this LINE_STRIP with the triangles mesh?
    glDrawElements(GL_LINE_STRIP, 2, GL_UNSIGNED_INT, 0);

    check_gl_errors("draw");
}

void Graphics::draw_triangles(int num_vertices, v2 *vertices, int num_triples, v2 **triples, v4 color)
{
    // TODO: Fix this to only be 1 allocation on initialize
    Vertex *my_vertices = new Vertex[num_vertices]();
    int num_indices = num_triples * 3;
    int *indices = new int[num_indices]();

    //assert(num_vertices == num_triples + 2);

    use_mesh(instance->triangles_mesh);

    for(int i = 0; i < num_vertices; i++)
    {
        my_vertices[i] = Vertex(vertices[i]);
    }
    set_mesh_vertex_buffer_data(instance->triangles_mesh, num_vertices, my_vertices);


    v2 *start = vertices;
    for(int i = 0; i < num_indices; i += 3)
    {
        v2 *a = triples[i + 0];
        v2 *b = triples[i + 1];
        v2 *c = triples[i + 2];
        indices[i + 0] = a - start;
        indices[i + 1] = b - start;
        indices[i + 2] = c - start;
    }
    set_mesh_index_buffer_data(instance->triangles_mesh, num_indices, indices);

    use_shader(instance->flat_color_shader);

    mat4 mvp = ndc_m_world(instance->camera, instance->screen_aspect_ratio);
    set_uniform(instance->quad_shader, "mvp", mvp);
    set_uniform(instance->quad_shader, "blend_color", color);

    glDrawElements(instance->triangles_mesh->primitive_type, num_indices, GL_UNSIGNED_INT, 0);

    check_gl_errors("draw");

    // SEE ABOVE
    delete[] my_vertices;
    delete[] indices;
}

v2 Graphics::ndc_point_to_world(v2 ndc)
{
    mat4 m_ndc_m_world = ndc_m_world(instance->camera, instance->screen_aspect_ratio);

    v4 ndc4 = v4(ndc, 0.0f, 1.0f);
    v4 world4 = inverse(m_ndc_m_world) * ndc4;

    return v2(world4.x, world4.y);
}

void Graphics::set_camera_position(v2 position)
{
    instance->camera->position = position;
}

void Graphics::set_camera_width(float width)
{
    instance->camera->width = width;
}



void Graphics::init()
{
    instance = new GraphicsState();

    create_gl_context(instance);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    instance->camera = new Camera();
    instance->camera->position = v2();
    instance->camera->width = 10.0f;
    instance->screen_aspect_ratio = Platform::Window::aspect_ratio();
    
    {
        // Make quad mesh
        VertexUv v[] =
        {
            { {-0.5f, -0.5f}, {0.0f, 0.0f} }, // BL
            { { 0.5f, -0.5f}, {1.0f, 0.0f} }, // BR
            { { 0.5f,  0.5f}, {1.0f, 1.0f} }, // TR
            { {-0.5f,  0.5f}, {0.0f, 1.0f} }, // TL
        };
        int i[] =
        {
            0, 1, 2,
            0, 2, 3
        };
        instance->quad_mesh = make_uv_mesh(_countof(v), v, _countof(i), i);

        instance->line_mesh = make_mesh(2, nullptr, 2, nullptr);
        instance->triangles_mesh = make_mesh(MAX_TRIANGLES, nullptr, MAX_TRIANGLES * 3, nullptr);

        Vertex *circle_vs = new Vertex[CIRCLE_MESH_RESOLUTION]();
        int *circle_is = new int[CIRCLE_MESH_RESOLUTION]();
        for(int i = 0; i < CIRCLE_MESH_RESOLUTION; i++)
        {
            float theta = remap(i, 0.0f, CIRCLE_MESH_RESOLUTION - 1.0f, 0.0f, 2.0f * PI);
            circle_vs[i] = { v2(cos(theta), sin(theta)) };
        }
        for(int i = 0; i < CIRCLE_MESH_RESOLUTION; i++) circle_is[i] = i;
        instance->circle_mesh = make_mesh(CIRCLE_MESH_RESOLUTION, circle_vs, CIRCLE_MESH_RESOLUTION, circle_is, GL_LINE_STRIP);
        delete[] circle_vs;
        delete[] circle_is;

        // Make shader
        instance->quad_shader = make_shader("assets/shaders/quad.shader");
        instance->flat_color_shader = make_shader("assets/shaders/flat_color.shader");

        // Make texture
        char bitmap[] = { (char)255, (char)255, (char)255, (char)255 };
        instance->white_texture = make_texture_from_bitmap(1, 1, bitmap);
    }
}

void Graphics::clear_frame(v4 color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Graphics::swap_frames()
{
    HDC dc = Windows::device_context();
    SwapBuffers(dc);
}






// ImGui Implementation
void Graphics::ImGuiImplementation::init()
{
    ImGui_ImplOpenGL3_Init("#version 440 core");
    ImGui_ImplWin32_Init(Windows::handle());
}

void Graphics::ImGuiImplementation::new_frame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
}

void Graphics::ImGuiImplementation::end_frame()
{
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Graphics::ImGuiImplementation::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}


