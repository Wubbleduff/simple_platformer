
#pragma once

namespace GameMath
{
    static const float PI = 3.14159265f;

    ///////////////////////////////////////////////////////////////////////////////
    // vector structs
    ///////////////////////////////////////////////////////////////////////////////
    struct v2
    {
        union
        {
            struct { float x, y; };
            float v[2];
        };

        v2() : x(0.0f), y(0.0f) { }
        v2(const v2 &v) : x(v.x), y(v.y) { }
        v2(float in_x, float in_y) : x(in_x), y(in_y) { }
    };

    struct v3
    {
        union
        {
            struct { float x; float y; float z; };
            struct { float r; float g; float b; };
            struct { float h; float s; float v; };
            float m[3];
        };
        v3() : x(0.0f), y(0.0f), z(0.0f) { }
        v3(const v3 &v) : x(v.x), y(v.y), z(v.z) { }
        v3(float in_x, float in_y, float in_z) : x(in_x), y(in_y), z(in_z) { }
        v3(v2 v, float a) : x(v.x), y(v.y), z(a)   { }
        v3(float a, v2 v) : x(a),   y(v.x), z(v.y) { }
    };

    struct v4
    {
        union
        {
            struct { float x, y, z, w; };
            struct { float r, g, b, a; };
            struct { float h, s, v, a; };
            float m[4];
        };
        v4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) { }
        v4(const v4 &v) : x(v.x), y(v.y), z(v.z), w(v.w) { }
        v4(float in_x, float in_y, float in_z, float in_w) : x(in_x), y(in_y), z(in_z), w(in_w) { }
        v4(v2 v, float a, float b) : x(v.x), y(v.y), z(a),   w(b)   { }
        v4(float a, v2 v, float b) : x(a),   y(v.x), z(v.y), w(b)   { }
        v4(float a, float b, v2 v) : x(a),   y(b),   z(v.x), w(v.y) { }
        v4(v3 v, float a)          : x(v.x), y(v.y), z(v.z), w(a)   { }
        v4(float a, v3 v)          : x(a),   y(v.x), z(v.y), w(v.z) { }
    };



    ///////////////////////////////////////////////////////////////////////////////
    // common operations
    ///////////////////////////////////////////////////////////////////////////////

    float cos(float a);
    float sin(float a);
    float squared(float a);
    float sqrt(float a);
    float to_power(float base, float exponent);
    int min(int a, int b);
    int min(int a, int b, int c);
    int max(int a, int b);
    int max(int a, int b, int c);
    float min(float a, float b);
    float min(float a, float b, float c);
    float max(float a, float b);
    float max(float a, float b, float c);
    int clamp(int a, int min, int max);
    float clamp(float a, float min, float max);
    float abs(float a);
    float deg_to_rad(float a);
    float rad_to_deg(float a);
    float lerp(float a, float b, float t);
    float inv_lerp(float a, float b, float t);
    float remap(float a, float from_min, float from_max, float to_min, float to_max);
    float average(float *values, int num);





    ///////////////////////////////////////////////////////////////////////////////
    // vector operations
    ///////////////////////////////////////////////////////////////////////////////

    v2 operator+(v2 a, v2 b);
    v3 operator+(v3 a, v3 b);
    v4 operator+(v4 a, v4 b);

    v2 operator-(v2 a, v2 b);
    v3 operator-(v3 a, v3 b);
    v4 operator-(v4 a, v4 b);

    v2 operator-(v2 a);
    v3 operator-(v3 a);
    v4 operator-(v4 a);

    float dot(v2 a, v2 b);
    float dot(v3 a, v3 b);
    float dot(v4 a, v4 b);

    v2 operator*(v2 a, float scalar);
    v3 operator*(v3 a, float scalar);
    v4 operator*(v4 a, float scalar);

    v2 operator*(float scalar, v2 a);
    v3 operator*(float scalar, v3 a);
    v4 operator*(float scalar, v4 a);

    v2 multiply_components(v2 a, v2 b);
    v3 multiply_components(v3 a, v3 b);
    v4 multiply_components(v4 a, v4 b);

    v2 operator/(v2 a, float scalar);
    v3 operator/(v3 a, float scalar);
    v4 operator/(v4 a, float scalar);

    v2 divide_components(v2 a, v2 b);
    v3 divide_components(v3 a, v3 b);
    v4 divide_components(v4 a, v4 b);

    v2 &operator+=(v2 &a, v2 b);
    v3 &operator+=(v3 &a, v3 b);
    v4 &operator+=(v4 &a, v4 b);

    v2 &operator-=(v2 &a, v2 b);
    v3 &operator-=(v3 &a, v3 b);
    v4 &operator-=(v4 &a, v4 b);

    v2 &operator*=(v2 &a, float b);
    v3 &operator*=(v3 &a, float b);
    v4 &operator*=(v4 &a, float b);

    v2 &operator/=(v2 &a, float b);
    v3 &operator/=(v3 &a, float b);
    v4 &operator/=(v4 &a, float b);

    v2 reflect(v2 v, v2 n);
    v3 reflect(v3 v, v3 n);
    v4 reflect(v4 v, v4 n);

    v2 lerp(v2 a, v2 b, float t);
    v3 lerp(v3 a, v3 b, float t);
    v4 lerp(v4 a, v4 b, float t);

    v2 lerp_components(v2 a, v2 b, v2 t);
    v3 lerp_components(v3 a, v3 b, v3 t);
    v4 lerp_components(v4 a, v4 b, v4 t);

    v2 inv_lerp_components(v2 a, v2 b, v2 t);
    v3 inv_lerp_components(v3 a, v3 b, v3 t);
    v4 inv_lerp_components(v4 a, v4 b, v4 t);

    v2 slerp(v2 a, v2 b, float t);

    v2 remap(v2 a, v2 from_min, v2 from_max, v2 to_min, v2 to_max);
    v3 remap(v3 a, v3 from_min, v3 from_max, v3 to_min, v3 to_max);
    v4 remap(v4 a, v4 from_min, v4 from_max, v4 to_min, v4 to_max);


    float length(v2 v);
    float length(v3 v);
    float length(v4 v);

    float length_squared(v2 v);
    float length_squared(v3 v);
    float length_squared(v4 v);

    v2 normalize(v2 v);
    v3 normalize(v3 v);
    v4 normalize(v4 v);

    v2 normalize_nonzero(v2 v);
    v3 normalize_nonzero(v3 v);
    v4 normalize_nonzero(v4 v);

    float signed_distance_to_plane(v2 a, v2 p, v2 n);
    float signed_distance_to_plane(v3 a, v3 p, v3 n);

    float angle_between(v2 a, v2 b);
    float angle_between(v3 a, v3 b);
    float angle_between(v4 a, v4 b);

    // Returns this vector clamped by max length
    v2 clamp_length(v2 v, float max_length);
    v3 clamp_length(v3 v, float max_length);
    v4 clamp_length(v4 v, float max_length);




    ///////////////////////////////////////////////////////////////////////////////
    // v2 specific operations
    ///////////////////////////////////////////////////////////////////////////////

    // Returns a counter-clockwise normal to the given vector with the same length
    v2 find_ccw_normal(v2 a);

    float cross(v2 a, v2 b, v2 c);

    bool is_ccw(v2 a, v2 b, v2 c);

    // Returns this vector rotated by the angle in radians
    v2 rotate_vector(v2 a, float angle);

    // Returns the angle this vector is pointing at in radians. If the vector is
    // pointing straight right, the angle is 0. If left, the angle is PI / 2.0f
    // etc.
    //            PI/2
    //             |
    //       PI <-- --> 0
    //             |
    //         -PI/2
    float angle(v2 a);



    ///////////////////////////////////////////////////////////////////////////////
    // v3 specific operations
    ///////////////////////////////////////////////////////////////////////////////

    v3 cross(v3 a, v3 b);

    v3 hsv_to_rgb(v3 hsv);



    ///////////////////////////////////////////////////////////////////////////////
    // matrix structs
    ///////////////////////////////////////////////////////////////////////////////

    struct mat4
    {
        float m[4][4];

        mat4() : m { {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f} }
        {}

        mat4(float aa, float ab, float ac, float ad,
                float ba, float bb, float bc, float bd,
                float ca, float cb, float cc, float cd,
                float da, float db, float dc, float dd) :
            m { {aa, ab, ac, ad},
                {ba, bb, bc, bd},
                {ca, cb, cc, cd},
                {da, db, dc, dd} }
        {}

        const float *operator[](unsigned i) const
        {
            return m[i];
        }
        float *operator[](unsigned i)
        {
            return m[i];
        }
    };



    ///////////////////////////////////////////////////////////////////////////////
    // matrix operations
    ///////////////////////////////////////////////////////////////////////////////

    v4 operator*(const mat4 &lhs, v4 rhs);

    mat4 operator*(const mat4 &lhs, const mat4 &rhs);

    // Recursive function for finding determinant of matrix. 
    // n is current dimension of a[][]
    float determinant(mat4 &A, int n);

    // Function to calculate and store inverse, returns false if 
    // matrix is singular 
    mat4 inverse(mat4 A);
    mat4 make_translation_matrix(v3 offset);
    mat4 make_scale_matrix(v3 scale);
    mat4 make_x_axis_rotation_matrix(float radians);
    mat4 make_y_axis_rotation_matrix(float radians);
    mat4 make_z_axis_rotation_matrix(float radians);



    ///////////////////////////////////////////////////////////////////////////////
    // random
    ///////////////////////////////////////////////////////////////////////////////
    void seed_random(unsigned int n);
    float random_01();
    int random_range(int min, int max);
    float random_range(float min, float max);
    v4 random_color();

} // namespace GameMath
