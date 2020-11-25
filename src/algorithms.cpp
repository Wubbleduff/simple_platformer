
#include "algorithms.h"
#include "platform.h"

#include <stdlib.h>



static const float EPSILON = 0.0001f;



void my_sort(void *base, size_t num, size_t element_bytes, int (*cmp)(const void *a, const void *b))
{
    qsort(base, num, element_bytes, cmp);
}



struct VertexAndAngle
{
    v2 *v;
    float angle;
};
static int compare_vertex_and_angle(const void *v_a, const void *v_b)
{
    VertexAndAngle *a = (VertexAndAngle *)v_a;
    VertexAndAngle *b = (VertexAndAngle *)v_b;
    return (a->angle < b->angle) ? -1 : 1;
}
void find_convex_hull(int num_vertices, v2 *vertices, int *num_hull_lines, v2 *(hull[]))
{
    // Find the lowest point
    v2 *lowest_point = &(vertices[0]);
    for(int i = 0; i < num_vertices; i++)
    {
        v2 *point = &(vertices[i]);
        if(absf(point->y - lowest_point->y) < EPSILON)
        {
            lowest_point = (point->x < lowest_point->x) ? point : lowest_point;
        }
        else if(point->y < lowest_point->y)
        {
            lowest_point = point;
        }
    }

    // Sort points by the angle between it and the lowest point and the x axis
    VertexAndAngle *sorted_vertices = (VertexAndAngle *)Platform::Memory::allocate(sizeof(VertexAndAngle) * num_vertices);
    for(int i = 0; i < num_vertices; i++)
    {
        float angle;
        if(&(vertices[i]) == lowest_point) angle = -INFINITY;
        else angle = angle_between(vertices[i] - *lowest_point, v2(1.0f, 0.0f));
        sorted_vertices[i] = { &(vertices[i]), angle };
    }
    qsort(sorted_vertices, num_vertices, sizeof(VertexAndAngle), compare_vertex_and_angle);

    // Add points to list that aren't clockwise
    *num_hull_lines = 0;
    for(int i = 0; i < num_vertices; i++)
    {
        v2 *next_considered_point = sorted_vertices[i].v;

        while(*num_hull_lines > 1)
        {
            v2 *top_stack = hull[*num_hull_lines - 1];
            v2 *next_top_stack = hull[*num_hull_lines - 2];
            float cross_p = cross(*next_top_stack, *top_stack, *next_considered_point);

            if(absf(cross_p) < EPSILON)
            {
                // The 3 points are collinear, take the furthest one
                if(length_squared(*next_top_stack - *next_considered_point) < length_squared(*next_top_stack - *top_stack))
                {
                    v2 *temp = next_considered_point;
                    next_considered_point = top_stack;
                    top_stack = temp;
                }
            }

            if(cross_p < EPSILON)
            {
                (*num_hull_lines)--;
            }
            else
            {
                break;
            }
        }

        hull[*num_hull_lines] = next_considered_point;
        (*num_hull_lines)++;
    }

    // One last check
    v2 top_stack = *(hull[*num_hull_lines - 1]);
    v2 next_top_stack = *(hull[*num_hull_lines - 2]);
    float cross_p = cross(next_top_stack, top_stack, *(sorted_vertices[0].v));
    if(cross_p < EPSILON)
    {
        (*num_hull_lines)--;
    }

    free(sorted_vertices);
}

