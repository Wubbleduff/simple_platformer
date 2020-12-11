
#include "game.h"
#include "logging.h"
#include "data_structures.h"
#include "graphics.h"

#include "platform_windows/platform_windows.h"
#include <cstdlib>
#include <cstdio>
#include <cassert>



struct PlatformState
{
    struct WindowState
    {
        HWND handle;
        int width;
        int height;
    } window;

    struct InputState
    {
        static const int MAX_KEYS = 256;
        static const int MAX_MOUSE_KEYS = 8;

        bool current_key_states[MAX_KEYS];
        bool previous_key_states[MAX_KEYS];
        bool event_key_states[MAX_KEYS]; // For recording windows key press events

        bool current_mouse_states[MAX_MOUSE_KEYS];
        bool previous_mouse_states[MAX_MOUSE_KEYS];
        bool event_mouse_states[MAX_MOUSE_KEYS]; // For recording windows key press events
    } input;

    bool want_to_close;

    LARGE_INTEGER start_time_counter;
};
PlatformState *Platform::instance = nullptr;



extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if(ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) return true;

    LRESULT result = 0; 

    switch (message)
    {
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        } break;
        case WM_CLOSE: 
        {
            DestroyWindow(window);
            return 0;
        } break;
        case WM_KEYDOWN:
        {
            Platform::Input::record_key_event(wParam, true);
        } break;
        case WM_KEYUP:  
        {
            Platform::Input::record_key_event(wParam, false);
        } break;
        case WM_LBUTTONDOWN:
        {
            Platform::Input::record_mouse_event(0, true);
        } break;
        case WM_LBUTTONUP:  
        {
            Platform::Input::record_mouse_event(0, false);
        } break;
        default:
        {
            result = DefWindowProc(window, message, wParam, lParam);
        } break;
    }

    return result;
}

void Platform::init()
{
    instance = new PlatformState();
    Platform::Memory::memset(instance, 0, sizeof(PlatformState));

    QueryPerformanceCounter(&instance->start_time_counter);

    // Create the window class
    WNDCLASS window_class = {};

    window_class.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = GetModuleHandle(NULL);
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.lpszClassName = "Windows Program Class";

    if(!RegisterClass(&window_class))
    {
        Log::log_error("Could not register Windows class!");
        return;
    }

    BOOL result;
    RECT rect;
    //result = SystemParametersInfoA(SPI_GETWORKAREA, 0, &rect, 0);
    rect.right = 1920 * 0.5f;
    rect.bottom = rect.right * (9.0f / 16.0f);
    unsigned window_width = rect.right;
    unsigned window_height = rect.bottom;
    //unsigned window_width = GetSystemMetrics(SM_CXSCREEN);
    //unsigned window_height = GetSystemMetrics(SM_CYSCREEN);


    // Create the window
    instance->window.handle = CreateWindowEx(0,                  // Extended style
            window_class.lpszClassName,        // Class name
            "",                           // Window name
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,  // Style of the window
            0,                                 // Initial X position
            0,                                 // Initial Y position
            window_width,                     // Initial width
            window_height,                    // Initial height
            0,                                 // Handle to the window parent
            0,                                 // Handle to a menu
            GetModuleHandle(NULL),        // Handle to an instance
            0);

    if(!instance->window.handle)
    {
        Log::log_error("Could not open window!");
        return;
    }

    RECT client_rect;
    BOOL success = GetClientRect(instance->window.handle, &client_rect);

    instance->window.width = client_rect.right;
    instance->window.height = client_rect.bottom;

    result = CreateDirectory("output", NULL);
    DWORD create_dir_result = GetLastError();
    if(result || ERROR_ALREADY_EXISTS == create_dir_result)
    {
    }
    else
    {
        // Failed to create directory.
    }


    // Input
    {
        Platform::Memory::memset(instance->input.current_key_states,  0, sizeof(bool) * PlatformState::InputState::MAX_KEYS);
        Platform::Memory::memset(instance->input.previous_key_states, 0, sizeof(bool) * PlatformState::InputState::MAX_KEYS);
        Platform::Memory::memset(instance->input.event_key_states,    0, sizeof(bool) * PlatformState::InputState::MAX_KEYS);

        Platform::Memory::memset(instance->input.current_mouse_states, 0,
                sizeof(bool) * PlatformState::InputState::MAX_MOUSE_KEYS);
        Platform::Memory::memset(instance->input.previous_mouse_states, 0,
                sizeof(bool) * PlatformState::InputState::MAX_MOUSE_KEYS);
        Platform::Memory::memset(instance->input.event_mouse_states, 0,
                sizeof(bool) * PlatformState::InputState::MAX_MOUSE_KEYS);
    }
}

void Platform::handle_os_events()
{
    MSG message;
    while(PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        if(message.message == WM_QUIT)
        {
            instance->want_to_close = true;
        }

        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    if(instance->want_to_close)
    {
        Game::stop();
    }
}

double Platform::time_since_start()
{
    LARGE_INTEGER counts_per_second;
    QueryPerformanceFrequency(&counts_per_second);

    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);

    LONGLONG counts_passed = current_time.QuadPart - instance->start_time_counter.QuadPart;

    double dt = (double)counts_passed / counts_per_second.QuadPart;

    return dt;
}

bool Platform::want_to_close()
{
    return instance->want_to_close;
}


int Platform::Window::screen_width()
{
    return instance->window.width;  
}

int Platform::Window::screen_height()
{
    return instance->window.height; 
}

int Platform::Window::monitor_frequency()
{
    int result = 0;
    BOOL devices_result;

    DISPLAY_DEVICEA display_device;
    display_device.cb = sizeof(DISPLAY_DEVICEA);
    DWORD flags = EDD_GET_DEVICE_INTERFACE_NAME;
    DWORD device_num = 0;
    do
    {
        devices_result = EnumDisplayDevicesA(NULL, device_num, &display_device, flags);
        device_num++;
        if(devices_result)
        {
            BOOL settings_result;
            DEVMODEA dev_mode;
            dev_mode.dmSize = sizeof(DEVMODEA);
            dev_mode.dmDriverExtra = 0;

            settings_result = EnumDisplaySettingsA(display_device.DeviceName, ENUM_CURRENT_SETTINGS, &dev_mode);
            if(settings_result)
            {
                int freq = dev_mode.dmDisplayFrequency;
                if(freq > result)
                {
                    result = freq;
                }
            }
        }
    } while(devices_result);

    return result;
}

float Platform::Window::aspect_ratio()
{
    return (float)instance->window.width / instance->window.height; 
}

void Platform::Window::mouse_screen_position(int *x, int *y)
{
    POINT window_client_pos;
    BOOL success = GetCursorPos(&window_client_pos);
    success = ScreenToClient(instance->window.handle, &window_client_pos);
    *x = window_client_pos.x;
    *y = window_client_pos.y;
}

template<typename T>
static T *allocx(int num)
{
    T *array = new T[num];
    array[0] = T();
}

/*
void *Platform::Memory::allocate(int bytes)
{
    //return ::malloc(bytes);
    return new char[bytes];
}

void Platform::Memory::free(void *data)
{
    delete [] data;
}
*/

void Platform::Memory::memset(void *buffer, int value, int bytes)
{
    ::memset(buffer, value, bytes);
}

void Platform::Memory::memcpy(void *dest, const void *src, int bytes)
{
    ::memcpy(dest, src, bytes);
}




struct Platform::File
{
    FILE *file;
};
static BOOL directory_exists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);

  BOOL exists = (dwAttrib != INVALID_FILE_ATTRIBUTES && 
                (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
  return exists == TRUE;
}

static void ensure_directory_exists(const char *path)
{
    int path_length = strlen(path);

    char *path_copy = new char[path_length + 1]();
    strcpy(path_copy, path);

    int stack_count = 0;
    char **dirs_stack = new char *[path_length]();

    char *start = path_copy;
    char *end = start + path_length;

    if(*end == '/') return;

    while(true)
    {
        while(*end != '/' && end != start)
        {
            end--;
        }

        *end = '\0';
        bool exists = directory_exists(start);
        *end = '/';
        if( (end == start) || exists ) break;
        

        dirs_stack[stack_count++] = end;
        end--;
    }

    while(stack_count > 0)
    {
        char *dir_path_end = dirs_stack[stack_count - 1];
        stack_count--;

        *dir_path_end = '\0';
        CreateDirectory(start, NULL);
        *dir_path_end = '/';
    }

    delete[] dirs_stack;
    delete[] path_copy;
}

Platform::File *Platform::FileSystem::open(const char *path, FileMode mode)
{

    ensure_directory_exists(path);

    Platform::File *file = new File();
    char mode_string[8] = {};
    switch(mode)
    {
        case READ:
            strcpy(mode_string, "rb");
            break;
        case WRITE:
            strcpy(mode_string, "wb");
            break;
        case READ_WRITE:
            strcpy(mode_string, "rwb");
            break;
    }

    file->file = fopen(path, mode_string);

    if(file->file == NULL)
    {
        Log::log_error("Couldn't open file %s", path);
    }

    return file;
}

void Platform::FileSystem::close(Platform::File *file)
{
    fclose(file->file);
    delete file;
}

void Platform::FileSystem::read(File *file, void *buffer, int bytes)
{
    fread(buffer, bytes, 1, file->file);
}

void Platform::FileSystem::write(File *file, void *buffer, int bytes)
{
    fwrite(buffer, bytes, 1, file->file);
}

int Platform::FileSystem::size(File *file)
{
    fseek(file->file, 0, SEEK_END);
    int size = ftell(file->file);
    fseek(file->file, 0, SEEK_SET);
    return size;
}

char *Platform::FileSystem::read_file_into_string(const char *path)
{
    FILE *file = fopen(path, "rb");
    if(file == nullptr) return nullptr;

    fseek(file, 0L, SEEK_END);
    int size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    char *buffer = new char[size + 1]();

    fread(buffer, size, 1, file);

    buffer[size] = '\0';

    return buffer;
}



// For recording windows key press events
void Platform::Input::record_key_event(int vk_code, bool state)
{
    instance->input.event_key_states[vk_code] = state;
}

// For recording windows key press events
void Platform::Input::record_mouse_event(int vk_code, bool state)
{
    instance->input.event_mouse_states[vk_code] = state;
}

void Platform::Input::read_input()
{
    // Read keyboard input
    Platform::Memory::memcpy(instance->input.previous_key_states, instance->input.current_key_states, sizeof(bool) * PlatformState::InputState::MAX_KEYS);
    Platform::Memory::memcpy(instance->input.current_key_states, instance->input.event_key_states, sizeof(bool) * PlatformState::InputState::MAX_KEYS);

    // Read mouse input
    Platform::Memory::memcpy(instance->input.previous_mouse_states, instance->input.current_mouse_states, sizeof(bool) * PlatformState::InputState::MAX_MOUSE_KEYS);
    Platform::Memory::memcpy(instance->input.current_mouse_states, instance->input.event_mouse_states, sizeof(bool) * PlatformState::InputState::MAX_MOUSE_KEYS);
}


bool Platform::Input::key_down(int key)
{
    assert(key < PlatformState::InputState::MAX_KEYS);
    return (instance->input.previous_key_states[key] == false && instance->input.current_key_states[key] == true) ? true : false;
}

bool Platform::Input::key(int key)
{
    assert(key < PlatformState::InputState::MAX_KEYS);
    return instance->input.current_key_states[key];
}


bool Platform::Input::mouse_button_down(int key)
{
    assert(key < PlatformState::InputState::MAX_MOUSE_KEYS);
    return (instance->input.previous_mouse_states[key] == false && instance->input.current_mouse_states[key] == true) ? true : false;
}

bool Platform::Input::mouse_button(int key)
{
    assert(key < PlatformState::InputState::MAX_MOUSE_KEYS);
    return instance->input.current_mouse_states[key];
}

GameMath::v2 Platform::Input::mouse_world_position()
{
    int sx, sy;
    Platform::Window::mouse_screen_position(&sx, &sy);
    GameMath::v2 p = GameMath::v2((float)sx, (float)sy);
    float screen_width = Platform::Window::screen_width();
    float screen_height = Platform::Window::screen_height();

    p.y = screen_height - p.y;

    GameMath::v2 ndc =
    {
        (p.x / screen_width)  * 2.0f - 1.0f,
        (p.y / screen_height) * 2.0f - 1.0f,
    };

    return Graphics::ndc_point_to_world(ndc);
}





// Platform Windows
HWND Windows::handle()
{
    return instance->window.handle; 
}

HDC Windows::device_context()
{
    return GetDC(instance->window.handle); 
}

