

#include "game_math.h"

#include <stdlib.h>
#include <math.h>


namespace GameMath
{

    ///////////////////////////////////////////////////////////////////////////////
    // common operations
    ///////////////////////////////////////////////////////////////////////////////

    float cos(float a) { return cosf(a); }
    float sin(float a) { return sinf(a); }
    float squared(float a) { return a * a; }
    float sqrt(float a) { return sqrtf(a); }
    float to_power(float base, float exponent) { return (float)pow(base, exponent); }
    int min(int a, int b) { return (a < b) ? a : b; }
    int min(int a, int b, int c) { return min(a, min(b, c)); }
    int max(int a, int b) { return (a > b) ? a : b; }
    int max(int a, int b, int c) { return max(a, max(b, c)); }
    float min(float a, float b) { return (a < b) ? a : b; }
    float min(float a, float b, float c) { return min(a, min(b, c)); }
    float max(float a, float b) { return (a > b) ? a : b; }
    float max(float a, float b, float c) { return max(a, max(b, c)); }
    int clamp(int a, int min, int max) { if(a < min) return min; if(a > max) return max; return a; }
    float clamp(float a, float min, float max) { if(a < min) return min; if(a > max) return max; return a; }
    int floor(float a) { return (int)a; }
    float abs(float a) { return (a < 0.0f) ? -a : a; } 
    float deg_to_rad(float a) { return a * (PI / 180.0f); }
    float rad_to_deg(float a) { return a * (180.0f / PI); }
    float lerp(float a, float b, float t) { return ((1.0f - t) * a) + (t * b); }
    float inv_lerp(float a, float b, float t) { return (t - a) / (b - a); }
    float remap(float a, float from_min, float from_max, float to_min, float to_max)
    {
        float from_percent = inv_lerp(from_min, from_max, a);
        return lerp(to_min, to_max, from_percent);
    }






    ///////////////////////////////////////////////////////////////////////////////
    // vector operations
    ///////////////////////////////////////////////////////////////////////////////

    v2 operator+(v2 a, v2 b) { return v2(a.x + b.x, a.y + b.y); }
    v3 operator+(v3 a, v3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
    v4 operator+(v4 a, v4 b) { return v4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }

    v2 operator-(v2 a, v2 b) { return v2(a.x - b.x, a.y - b.y); }
    v3 operator-(v3 a, v3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
    v4 operator-(v4 a, v4 b) { return v4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }

    v2 operator-(v2 a) { return v2(-a.x, -a.y); }
    v3 operator-(v3 a) { return v3(-a.x, -a.y, -a.z); }
    v4 operator-(v4 a) { return v4(-a.x, -a.y, -a.z, -a.w); }

    float dot(v2 a, v2 b) { return (a.x * b.x) + (a.y * b.y); }
    float dot(v3 a, v3 b) { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z); }
    float dot(v4 a, v4 b) { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w); }

    v2 operator*(v2 a, float scalar) { return v2(a.x * scalar, a.y * scalar); }
    v3 operator*(v3 a, float scalar) { return v3(a.x * scalar, a.y * scalar, a.z * scalar); }
    v4 operator*(v4 a, float scalar) { return v4(a.x * scalar, a.y * scalar, a.z * scalar, a.w * scalar); }

    v2 operator*(float scalar, v2 a) { return v2(a.x * scalar, a.y * scalar); }
    v3 operator*(float scalar, v3 a) { return v3(a.x * scalar, a.y * scalar, a.z * scalar); }
    v4 operator*(float scalar, v4 a) { return v4(a.x * scalar, a.y * scalar, a.z * scalar, a.w * scalar); }

    v2 multiply_components(v2 a, v2 b) { return v2(a.x * b.x, a.y * b.y); }
    v3 multiply_components(v3 a, v3 b) { return v3(a.x * b.x, a.y * b.y, a.z * b.z); }
    v4 multiply_components(v4 a, v4 b) { return v4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }

    v2 operator/(v2 a, float scalar) { return v2(a.x / scalar, a.y / scalar); }
    v3 operator/(v3 a, float scalar) { return v3(a.x / scalar, a.y / scalar, a.z / scalar); }
    v4 operator/(v4 a, float scalar) { return v4(a.x / scalar, a.y / scalar, a.z / scalar, a.w / scalar); }

    v2 divide_components(v2 a, v2 b) { return v2(a.x / b.x, a.y / b.y); }
    v3 divide_components(v3 a, v3 b) { return v3(a.x / b.x, a.y / b.y, a.z / b.z); }
    v4 divide_components(v4 a, v4 b) { return v4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }

    v2 &operator+=(v2 &a, v2 b) { a.x += b.x; a.y += b.y; return a; }
    v3 &operator+=(v3 &a, v3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
    v4 &operator+=(v4 &a, v4 b) { a.x += b.x; a.y += b.y; a.z += b.z; a.w += b.w; return a; }

    v2 &operator-=(v2 &a, v2 b) { a.x -= b.x; a.y -= b.y; return a; }
    v3 &operator-=(v3 &a, v3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
    v4 &operator-=(v4 &a, v4 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; a.w -= b.w; return a; }

    v2 &operator*=(v2 &a, float b) { a.x *= b; a.y *= b; return a; }
    v3 &operator*=(v3 &a, float b) { a.x *= b; a.y *= b; a.z *= b; return a; }
    v4 &operator*=(v4 &a, float b) { a.x *= b; a.y *= b; a.z *= b; a.w *= b; return a; }

    v2 &operator/=(v2 &a, float b) { a.x /= b; a.y /= b; return a; }
    v3 &operator/=(v3 &a, float b) { a.x /= b; a.y /= b; a.z /= b; return a; }
    v4 &operator/=(v4 &a, float b) { a.x /= b; a.y /= b; a.z /= b; a.w /= b; return a; }

    v2 reflect(v2 v, v2 n) { return v - 2.0f * dot(v, n) * n; }
    v3 reflect(v3 v, v3 n) { return v - 2.0f * dot(v, n) * n; }
    v4 reflect(v4 v, v4 n) { return v - 2.0f * dot(v, n) * n; }

    v2 lerp(v2 a, v2 b, float t) { return ((1.0f - t) * a) + (t * b); }
    v3 lerp(v3 a, v3 b, float t) { return ((1.0f - t) * a) + (t * b); }
    v4 lerp(v4 a, v4 b, float t) { return ((1.0f - t) * a) + (t * b); }

    v2 lerp_components(v2 a, v2 b, v2 t)
    {
        return multiply_components(v2(1.0f, 1.0f) - t, a) + multiply_components(t, b);
    }

    v3 lerp_components(v3 a, v3 b, v3 t)
    {
        return multiply_components(v3(1.0f, 1.0f, 1.0f) - t, a) + multiply_components(t, b);
    }

    v4 lerp_components(v4 a, v4 b, v4 t)
    {
        return multiply_components(v4(1.0f, 1.0f, 1.0f, 1.0f) - t, a) + multiply_components(t, b);
    }

    v2 inv_lerp_components(v2 a, v2 b, v2 t) { return divide_components((t - a), (b - a)); }
    v3 inv_lerp_components(v3 a, v3 b, v3 t) { return divide_components((t - a), (b - a)); }
    v4 inv_lerp_components(v4 a, v4 b, v4 t) { return divide_components((t - a), (b - a)); }

    v2 remap(v2 a, v2 from_min, v2 from_max, v2 to_min, v2 to_max) { return lerp_components(to_min, to_max, inv_lerp_components(from_min, from_max, a)); }
    v3 remap(v3 a, v3 from_min, v3 from_max, v3 to_min, v3 to_max) { return lerp_components(to_min, to_max, inv_lerp_components(from_min, from_max, a)); }
    v4 remap(v4 a, v4 from_min, v4 from_max, v4 to_min, v4 to_max) { return lerp_components(to_min, to_max, inv_lerp_components(from_min, from_max, a)); }

    // HERE
    v2 slerp(v2 a, v2 b, float t)
    {
        float angle = angle_between(a, b);
        v2 t1 = (sinf((1.0f - t) * angle) / sinf(angle)) * a;
        v2 t2 = (sinf(t * angle) / sinf(angle)) * b;
        return t1 + t2;
    }

    // Gets the length of the vector
    float length(v2 v)
    {
        return sqrtf(squared(v.x) + squared(v.y));
    }
    float length(v3 v)
    {
        return sqrtf(squared(v.x) + squared(v.y) + squared(v.z));
    }
    float length(v4 v)
    {
        return sqrtf(squared(v.x) + squared(v.y) + squared(v.z) + squared(v.w));
    }

    // Gets the squared length of this vector
    float length_squared(v2 v)
    {
        return squared(v.x) + squared(v.y);
    }
    float length_squared(v3 v)
    {
        return squared(v.x) + squared(v.y) + squared(v.z);
    }
    float length_squared(v4 v)
    {
        return squared(v.x) + squared(v.y) + squared(v.z) + squared(v.w);
    }

    v2 normalize(v2 v)
    {
        float l = length(v);
        if(l == 0.0f) return v2();
        return v / l;
    }
    v3 normalize(v3 v)
    {
        float l = length(v);
        if(l == 0.0f) return v3();
        return v / l;
    }
    v4 normalize(v4 v)
    {
        float l = length(v);
        if(l == 0.0f) return v4();
        return v / l;
    }

    v2 normalize_nonzero(v2 v) { return v / length(v); }
    v3 normalize_nonzero(v3 v) { return v / length(v); }
    v4 normalize_nonzero(v4 v) { return v / length(v); }

    float signed_distance_to_plane(v2 a, v2 p, v2 n)
    {
        n = normalize(n);
        return dot(a, n) - dot(p, n);
    }
    float signed_distance_to_plane(v3 a, v3 p, v3 n)
    {
        n = normalize(n);
        return dot(a, n) - dot(p, n);
    }

    float angle_between(v2 a, v2 b) { return (float)acos(dot(normalize(a), normalize(b))); }
    float angle_between(v3 a, v3 b) { return (float)acos(dot(normalize(a), normalize(b))); }
    float angle_between(v4 a, v4 b) { return (float)acos(dot(normalize(a), normalize(b))); }

    // Returns this vector clamped by max length
    v2 clamp_length(v2 v, float max_length)
    {
        float len = length(v);
        if(len > max_length)
        {
            v = v / len;
            v = v * max_length;
        }

        return v;
    }
    v3 clamp_length(v3 v, float max_length)
    {
        float len = length(v);
        if(len > max_length)
        {
            v = v / len;
            v = v * max_length;
        }

        return v;
    }
    v4 clamp_length(v4 v, float max_length)
    {
        float len = length(v);
        if(len > max_length)
        {
            v = v / len;
            v = v * max_length;
        }

        return v;
    }




    ///////////////////////////////////////////////////////////////////////////////
    // v2 specific operations
    ///////////////////////////////////////////////////////////////////////////////

    // Returns a counter-clockwise normal to the given vector with the same length
    v2 find_ccw_normal(v2 a)
    {
        return v2(-a.y, a.x);
    }

    float cross(v2 a, v2 b, v2 c)
    {
        return (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x);
    }

    bool is_ccw(v2 a, v2 b, v2 c)
    {
        float cross_p = cross(a, b, c);
        return cross_p > 0.0f;
    }


    // HERE
    // Returns this vector rotated by the angle in radians
    v2 rotate_vector(v2 a, float angle)
    {
        v2 v;

        v.x = a.x * (float)cos(angle) - a.y * (float)sin(angle);
        v.y = a.x * (float)sin(angle) + a.y * (float)cos(angle);

        return v;
    }

    // Returns the angle this vector is pointing at in radians. If the vector is
    // pointing straight right, the angle is 0. If left, the angle is PI / 2.0f
    // etc.
    //            PI/2
    //             |
    //       PI <-- --> 0
    //             |
    //         -PI/2
    float angle(v2 a)
    {
        return atan2f(a.y, a.x);
    }



    ///////////////////////////////////////////////////////////////////////////////
    // v3 specific operations
    ///////////////////////////////////////////////////////////////////////////////

    v3 cross(v3 a, v3 b)
    {
        v3 v;
        v.x = (a.y * b.z) - (a.z * b.y);
        v.y = (a.z * b.x) - (a.x * b.z);
        v.z = (a.x * b.y) - (a.y * b.x);
        return v;
    }

    v3 hsv_to_rgb(v3 hsv)
    {
        float hh, p, q, t, ff;
        int i;
        v3 rgb;

        if(hsv.s <= 0.0)
        {
            rgb.r = hsv.v;
            rgb.g = hsv.v;
            rgb.b = hsv.v;
            return rgb;
        }
        hh = hsv.h;
        if(hh >= 360.0f) hh = 0.0f;
        hh /= 60.0f;
        i = (int)hh;
        ff = hh - i;
        p = hsv.v * (1.0f - hsv.s);
        q = hsv.v * (1.0f - (hsv.s * ff));
        t = hsv.v * (1.0f - (hsv.s * (1.0f - ff)));

        switch(i)
        {
            case 0:
                rgb.r = hsv.v;
                rgb.g = t;
                rgb.b = p;
                break;
            case 1:
                rgb.r = q;
                rgb.g = hsv.v;
                rgb.b = p;
                break;
            case 2:
                rgb.r = p;
                rgb.g = hsv.v;
                rgb.b = t;
                break;

            case 3:
                rgb.r = p;
                rgb.g = q;
                rgb.b = hsv.v;
                break;
            case 4:
                rgb.r = t;
                rgb.g = p;
                rgb.b = hsv.v;
                break;
            case 5:
            default:
                rgb.r = hsv.v;
                rgb.g = p;
                rgb.b = q;
                break;
        }
        return rgb;     
    }



    ///////////////////////////////////////////////////////////////////////////////
    // matrix operations
    ///////////////////////////////////////////////////////////////////////////////

    // This funciton was made only for the matrix-vector multiplication
    float dot4v(const float *a, v4 b)
    {
        return (a[0] * b.x) + (a[1] * b.y) + (a[2] * b.z) + (a[3] * b.w);
    }
    v4 operator*(const mat4 &lhs, v4 rhs)
    {
        v4 result;

        result.x = dot4v(lhs[0], rhs);
        result.y = dot4v(lhs[1], rhs);
        result.z = dot4v(lhs[2], rhs);
        result.w = dot4v(lhs[3], rhs);

        return result;
    }

    mat4 operator*(const mat4 &lhs, const mat4 &rhs)
    {
        mat4 product;

        // Loop through each spot in the resulting matrix
        for(unsigned row = 0; row < 4; row++)
        {
            for(unsigned col = 0; col < 4; col++)
            {
                // Dot the row and column for the given slot
                float dot = 0.0f;
                for(unsigned i = 0; i < 4; i++)
                {
                    dot += lhs.m[row][i] * rhs.m[i][col];
                }

                product[row][col] = dot;
            }
        }

        return product;
    }




















    // Function to get cofactor of A[p][q] in temp[][]. n is current 
    // dimension of A[][] 
    // https://www.geeksforgeeks.org/adjoint-inverse-matrix/
    void get_cofactor(mat4 &a, mat4 &temp, int p, int q, int n) 
    { 
        int i = 0, j = 0; 

        // Looping for each element of the matrix 
        for(int row = 0; row < n; row++) 
        { 
            for(int col = 0; col < n; col++) 
            { 
                //  Copying into temporary matrix only those element 
                //  which are not in given row and column 
                if(row != p && col != q) 
                { 
                    temp[i][j++] = a[row][col]; 

                    // Row is filled, so increase row index and 
                    // reset col index 
                    if(j == n - 1) 
                    { 
                        j = 0; 
                        i++; 
                    } 
                } 
            } 
        } 
    } 

    // Recursive function for finding determinant of matrix. 
    // n is current dimension of a[][]
    float determinant(mat4 &A, int n) 
    { 
        if(n == 1) return A[0][0];

        float D = 0; // Initialize result 
        mat4 temp; // To store cofactors 
        float sign = 1;  // To store sign multiplier 

        // Iterate for each element of first row 
        for(int f = 0; f < n; f++) 
        { 
            // Getting Cofactor of A[0][f] 
            get_cofactor(A, temp, 0, f, n); 
            D += sign * A[0][f] * determinant(temp, n - 1); 

            // terms are to be added with alternate sign 
            sign = -sign; 
        } 

        return D; 
    } 

    // Function to get adjoint of A[N][N] in adj[N][N]. 
    void adjoint(mat4 &A, mat4 &adj) 
    { 
        // temp is used to store cofactors of A[][] 
        int sign = 1;
        mat4 temp; 

        for(int i = 0; i < 4; i++) 
        { 
            for(int j = 0; j < 4; j++) 
            { 
                // Get cofactor of A[i][j] 
                get_cofactor(A, temp, i, j, 4); 

                // sign of adj[j][i] positive if sum of row 
                // and column indexes is even. 
                sign = ((i + j) % 2 == 0) ? 1 : -1; 

                // Interchanging rows and columns to get the 
                // transpose of the cofactor matrix 
                adj[j][i] = sign * determinant(temp, 4-1); 
            } 
        } 
    } 

    // Function to calculate and store inverse, returns false if 
    // matrix is singular 
    mat4 inverse(mat4 A) 
    {
        mat4 inverse;

        // Find determinant of A[][] 
        float det = determinant(A, 4); 
        if (det == 0) 
        { 
            return mat4(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); 
        } 

        // Find adjoint 
        mat4 adj; 
        adjoint(A, adj); 

        // Find Inverse using formula "inverse(A) = adj(A)/det(A)" 
        for(int i = 0; i < 4; i++) 
            for(int j = 0; j < 4; j++) 
                inverse[i][j] = adj[i][j] / det; 

        return inverse; 
    }










    mat4 make_translation_matrix(v3 offset)
    {
        mat4 result = 
        {
            1.0f, 0.0f, 0.0f, offset.x,
            0.0f, 1.0f, 0.0f, offset.y,
            0.0f, 0.0f, 1.0f, offset.z,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        return result;
    }

    mat4 make_scale_matrix(v3 scale)
    {
        mat4 result = 
        {
            scale.x, 0.0f, 0.0f, 0.0f,
            0.0f, scale.y, 0.0f, 0.0f,
            0.0f, 0.0f, scale.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        return result;
    }

    mat4 make_x_axis_rotation_matrix(float radians)
    {
        mat4 result = 
        {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, (float)cos(radians), (float)-sin(radians), 0.0f,
            0.0f, (float)sin(radians), (float)cos(radians), 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        return result;
    }

    mat4 make_y_axis_rotation_matrix(float radians)
    {
        mat4 result = 
        {
            (float)cos(radians), 0.0f, (float)sin(radians), 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            (float)-sin(radians), 0.0f, (float)cos(radians), 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        return result;
    }

    mat4 make_z_axis_rotation_matrix(float radians)
    {
        mat4 result = 
        {
            (float)cos(radians), (float)-sin(radians), 0.0f, 0.0f,
            (float)sin(radians), (float)cos(radians), 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        return result;
    }



    ///////////////////////////////////////////////////////////////////////////////
    // random
    ///////////////////////////////////////////////////////////////////////////////

    void seed_random(unsigned int n)
    {
        srand(n);
    }

    float random_01()
    {
        return (float)rand() / (float)RAND_MAX;
    }

    int random_range(int min, int max)
    {
        int random_number = rand();

        int diff = max - min;
        int range = random_number % (diff + 1);
        int result = range + min;
        return result;
    }

    float random_range(float min, float max)
    {
        return lerp(min, max, random_01());
    }

    v4 random_color()
    {
        float hue = random_range(0.0f, 360.0f);
        float sat = 1.0f;
        float value = 1.0f;
        v3 hsv = { hue, sat, value };
        return v4( hsv_to_rgb(hsv), 1.0f);
    }


}
