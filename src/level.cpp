
#include "level.h"
#include "memory.h"
#include "my_math.h"
#include "my_algorithms.h"
#include "renderer.h"
#include "engine.h"
#include "input.h"



struct Player
{
    v2 position;
    v2 full_extents;

    float acceleration_strength;
    float horizontal_speed;
    float vertical_speed;
    float friction_strength;
    float drag_strength;
    float jump_strength;

    bool grounded;
    v2 shoe_position;
    float shoe_radius;
    v2 standing_plane_normal;
};

struct Line
{
    union
    {
        struct
        {
            v2 *a;
            v2 *b;
        };
        v2 *v[2];
    };

    Line() : a(nullptr), b(nullptr) {}
    Line(v2 *inA, v2 *inB) : a(inA), b(inB) {}
};

struct Triangle
{
    union
    {
        struct
        {
            v2 *a;
            v2 *b;
            v2 *c;
        };
        v2 *v[3];
    };

    Triangle(v2 *inA, v2 *inB, v2 *inC) : a(inA), b(inB), c(inC) {}
};

struct Collision
{
    int num_contact_points;
    v2 contact_points[2];

    float depth;
    v2 resolution_direction;
};

struct LevelGeometryChunk
{
    int num_vertices;
    v2 *vertices;

    int num_physical_planes;
    Line *physical_planes;

    int num_triangles;
    Triangle *triangulation;
};

struct StaticLevelGeometry
{
    int num_chunks;
    LevelGeometryChunk **chunks;
};

struct KinematicLevelGeometry
{
};

struct Enemy
{
};

struct Level
{
    bool paused; // Paused is a part of the active game level, NOT the game state

    Player *player; // A level HAS a player, level geometry, etc.


    // Split up static and kinematic level geometry because they're conceptually two different things
    // Static level geometry can be highly optimized and is very simple
    // Kinematic level geometry affects kinematics, collisions, squishing, etc.
    // It makes sense to split this up into two different data structures rather than have them posses
    // generic "collider components"
    StaticLevelGeometry *static_level_geometry;
    KinematicLevelGeometry *kinematic_level_geometry;

    int num_enemies;
    Enemy *enemies;

    float gravity_force; // A level has a gravity force, make this a vector force to easily add cool level changing effects!
};
static const float EPSILON = 0.0001f;



static void reset_player(Player *player)
{
    player->position = v2(2.0f, 2.0f);
    player->full_extents = v2(1.0f, 2.0f) * 0.2f;
    player->acceleration_strength = 32.0f;
    player->horizontal_speed = 0.0f;
    player->vertical_speed = 0.0f;
    player->friction_strength = 12.0f;
    player->drag_strength = 0.5f;
    player->jump_strength = 5.0f;
    player->grounded = false;

    player->shoe_radius = 0.20f;
    player->shoe_position = v2(0.0f, 0.025f);
}

static LevelGeometryChunk *create_piece_of_level_geometry()
{
    LevelGeometryChunk *chunk = (LevelGeometryChunk *)my_allocate(sizeof(LevelGeometryChunk));

#if 1
    v2 vertices[] =
    {
        {  0.0f,   3.0f }, // 0
        {  0.0f, -10.0f }, // 1
        { 11.0f, -10.0f }, // 2
        { 11.0f,   0.0f }, // 3
        {  7.5f,   0.0f }, // 4
        {  7.0f,   1.0f }, // 5
        {  5.0f,   1.0f }, // 6
        {  3.0f,   0.0f }, // 7
        {  1.0f,   0.0f }, // 8
        {  1.0f,   3.0f }, // 9
    };

    int num_vertices = _countof(vertices);
    chunk->num_vertices = num_vertices;
    chunk->vertices = (v2 *)my_allocate(sizeof(v2) * num_vertices);
    my_memcpy(chunk->vertices, vertices, sizeof(vertices));

    chunk->num_physical_planes = num_vertices;
    chunk->physical_planes = (Line *)my_allocate(sizeof(Line) * chunk->num_physical_planes);
    for(int i = 0; i < num_vertices; i++)
    {
        v2 *a = &(chunk->vertices[i]);
        v2 *b = &(chunk->vertices[(i + 1) % num_vertices]);
        chunk->physical_planes[i] = {a, b};
    }

    Triangle triangulation[] =
    {
        { &(chunk->vertices[1]), &(chunk->vertices[9]), &(chunk->vertices[0]) },
        { &(chunk->vertices[1]), &(chunk->vertices[8]), &(chunk->vertices[9]) },

        { &(chunk->vertices[2]), &(chunk->vertices[8]), &(chunk->vertices[1]) },
        { &(chunk->vertices[2]), &(chunk->vertices[7]), &(chunk->vertices[8]) },

        { &(chunk->vertices[4]), &(chunk->vertices[6]), &(chunk->vertices[7]) },
        { &(chunk->vertices[4]), &(chunk->vertices[5]), &(chunk->vertices[6]) },

        { &(chunk->vertices[2]), &(chunk->vertices[4]), &(chunk->vertices[7]) },
        { &(chunk->vertices[2]), &(chunk->vertices[3]), &(chunk->vertices[4]) }
    };

    chunk->num_triangles = _countof(triangulation);
    chunk->triangulation = (Triangle *)my_allocate(sizeof(Triangle) * chunk->num_triangles);
    my_memcpy(chunk->triangulation, triangulation, sizeof(triangulation));
#endif

#if 0
    v2 vertices[] =
    {
        { 10.0f, 0.0f },
        {  0.0f, 0.0f },
    };

    int num_vertices = _countof(vertices);
    chunk->num_vertices = num_vertices;
    chunk->vertices = (v2 *)my_allocate(sizeof(v2) * num_vertices);
    my_memcpy(chunk->vertices, vertices, sizeof(vertices));

    chunk->num_physical_planes = num_vertices - 1;
    chunk->physical_planes = (Line *)my_allocate(sizeof(Line) * chunk->num_physical_planes);
    for(int i = 0; i < num_vertices - 1; i++)
    {
        v2 *a = &(chunk->vertices[i]);
        v2 *b = &(chunk->vertices[i + 1]);
        chunk->physical_planes[i] = {a, b};
    }

    chunk->num_triangles = 0;
    chunk->triangulation = nullptr;
#endif

    return chunk;
}


static void draw_cylinder(RendererState *renderer_state, v2 pos, v2 full_extents, v4 color)
{
    float radius = full_extents.x / 2.0f;
    v2 diff = v2(0.0f, full_extents.y / 2.0f - radius);
    v2 a = pos + diff;
    v2 b = pos - diff;
    draw_circle(renderer_state, a, radius, color);
    draw_circle(renderer_state, b, radius, color);
    draw_line(renderer_state, b + v2(radius, 0.0f), a + v2(radius, 0.0f), color);
    draw_line(renderer_state, a - v2(radius, 0.0f), b - v2(radius, 0.0f), color);
}

static bool line_line_collision(v2 p1, v2 p2, v2 q1, v2 q2, v2 *hit_point)
{
    v2 p_n = find_ccw_normal(p2 - p1);
    v2 q_n = find_ccw_normal(q2 - q1);

    // Linear algebra is cool
    if(dot(q1 - p1, p_n) * dot(q2 - p1, p_n) > 0.0f + EPSILON) return false;
    if(dot(p1 - q1, q_n) * dot(p2 - q1, q_n) > 0.0f + EPSILON) return false;

    float small = dot(p1, p_n) - dot(q1, p_n);
    float big = dot(q2, p_n) - dot(q1, p_n);;
    float ratio = small / big;
    *hit_point = q1 + (q2 - q1) * ratio;

    return true;
}

/*
static bool box_line_collision(v2 bl, v2 tr, v2 start, v2 end, Collision *collision)
{
    bool collision_exists = false;

    v2 br = v2(tr.x, bl.y);
    v2 tl = v2(bl.x, tr.y);
    Line bottom = Line(&bl, &br);
    Line right  = Line(&br, &tr);
    Line top    = Line(&tr, &tl);
    Line left   = Line(&tl, &bl);
    Line other  = Line(&start, &end);
    v2 other_normal = normalize(find_ccw_normal(*other.b - *other.a));

    v2 box_points[4] = { bl, br, tr, tl };
    Line box_lines[4] = { bottom, right, top, left};
    v2 box_normals[4];
    for(int i = 0; i < 4; i++)
    {
        Line *line = &(box_lines[i]);
        box_normals[i] = find_ccw_normal(*line->b - *line->a);
    }

    // Find all intersections between the box lines and the other line
    for(int i = 0; i < 4; i++)
    {
        Line *line = &(box_lines[i]);
        v2 hit_point;

        bool hit = line_line_collision(*line->a, *line->b, *other.a, *other.b, &hit_point);
        if(hit)
        {
            collision_exists = true;
            collision->contact_points[collision->num_contact_points] = hit_point;
            collision->num_contact_points++;
        }
    }

    // Find the deepest point's depth
    if(collision->num_contact_points == 0)
    {
    }
    else if(collision->num_contact_points == 1)
    {
        v2 *point_in_box = other.a;
        for(int i = 0; i < 4; i++)
        {
            v2 normal = box_normals[i];
            if(dot(*other.a, normal) * dot(box_points[i], normal) < 0.0f)
            {
                point_in_box = other.b;
                break;
            }
        }

        float min_dist_to_line = INFINITY;
        for(int i = 0; i < 4; i++)
        {
            Line *line = &(box_lines[i]);
            v2 line_normal = normalize((*line->b - *line->a));
            float dist_to_line = signed_distance_to_plane(*point_in_box, *line->a, line_normal);
            if(dist_to_line < min_dist_to_line)
            {
                collision->depth = dist_to_line;
                collision->resolution_direction = line_normal;
            }
        }
    }
    else if(collision->num_contact_points == 2)
    {
        collision->depth = 0.0f;
        for(int i = 0; i < 4; i++)
        {
            v2 *point = &(box_points[i]);
            float point_depth = dot(*point - *other.a, other_normal);
            point_depth *= -1.0f; // Because normals are pointing ccw, we want the most negative value
            collision->depth = max_float(point_depth, collision->depth);
        }

        collision->resolution_direction = other_normal;
    }


    return collision_exists;
}
*/

static bool box_line_collision(v2 bl, v2 tr, v2 start, v2 end, Collision *collision)
{
    bool collision_exists = false;
    v2 br = v2(tr.x, bl.y);
    v2 tl = v2(bl.x, tr.y);

    v2 box_origin = (bl + tr) / 2.0f;
    v2 bl_o = bl - box_origin;
    v2 br_o = br - box_origin;
    v2 tr_o = tr - box_origin;
    v2 tl_o = tl - box_origin;

    v2 diff = end - start;
    v2 swept_vertices[] =
    {
        start + bl_o, start + br_o, start + tr_o, start + tl_o,
        end + bl_o, end + br_o, end + tr_o, end + tl_o,
    };
    int num_swept_vertices = _countof(swept_vertices);

    int num_convex_hull_lines;
    v2 *convex_hull[8] = {};
    find_convex_hull(num_swept_vertices, swept_vertices, &num_convex_hull_lines, convex_hull);

#if 0
    {
        RendererState *renderer_state = get_engine_renderer_state();
        v4 color = v4(1, 1, 0.5f, 0.25f);
        draw_line(renderer_state, swept_vertices[0], swept_vertices[1], color);
        draw_line(renderer_state, swept_vertices[1], swept_vertices[2], color);
        draw_line(renderer_state, swept_vertices[2], swept_vertices[3], color);
        draw_line(renderer_state, swept_vertices[3], swept_vertices[0], color);

        draw_line(renderer_state, swept_vertices[4], swept_vertices[5], color);
        draw_line(renderer_state, swept_vertices[5], swept_vertices[6], color);
        draw_line(renderer_state, swept_vertices[6], swept_vertices[7], color);
        draw_line(renderer_state, swept_vertices[7], swept_vertices[4], color);

        for(int i = 0; i < num_swept_vertices; i++)
        {
            draw_quad(renderer_state, swept_vertices[i], v2(0.02f, 0.02f), 0.0f, color);
        }

        color = v4(1, 0.5f, 0.5f, 0.75f);
        for(int i = 0; i < num_convex_hull_lines; i++)
        {
            Line line = Line(convex_hull[i], convex_hull[(i + 1) % num_convex_hull_lines]);
            v2 line_center = (*line.a + *line.b) / 2.0;
            v2 line_normal = normalize(find_ccw_normal(*line.b - *line.a));
            draw_line(renderer_state, *line.a, *line.b, color);
            draw_line(renderer_state, line_center, line_center + line_normal*0.1f, v4(1, 1, 0, 0.75f));
        }
    }
#endif


    float min_dist = INFINITY;
    v2 resolution_direction = v2();
    for(int i = 0; i < num_convex_hull_lines; i++)
    {
        Line line = Line(convex_hull[i], convex_hull[(i + 1) % num_convex_hull_lines]);
        v2 line_normal = normalize(find_ccw_normal(*line.b - *line.a)); // Should be pointing inward
        float signed_dist_from_line = signed_distance_to_plane(box_origin, *line.a, line_normal);

        if(signed_dist_from_line < 0.0f)
        {
            return false;
        }

        if(signed_dist_from_line < min_dist)
        {
            min_dist = signed_dist_from_line;
            resolution_direction = -line_normal;
        }
    }

    collision->depth = min_dist;
    collision->resolution_direction = resolution_direction;


    return true;
}

static bool cylinder_line_collision(v2 position, v2 full_extents, v2 start, v2 end, Collision *collision)
{
    if(start.x > end.x)
    {
        v2 temp = start;
        start = end;
        end = temp;
    }

    float circle_radius = full_extents.x / 2.0f;
    v2 circle_centers[4];
    v2 lines[8];
    v2 diff = end - start;
    v2 normal = find_ccw_normal(normalize(diff));

    v2 new_origin = start;
    start -= new_origin;
    end -= new_origin;
    position -= new_origin;

    circle_centers[0] = v2(0.0f, -full_extents.y / 2.0f + circle_radius);
    circle_centers[1] = circle_centers[0] + diff;
    circle_centers[2] = circle_centers[1] + v2(0.0f, full_extents.y - circle_radius * 2.0f);
    circle_centers[3] = circle_centers[2] - diff;

    lines[0] = circle_centers[0] - normal * circle_radius;
    lines[1] = circle_centers[1] - normal * circle_radius;
    lines[2] = circle_centers[1] + v2(circle_radius, 0.0f);
    lines[3] = circle_centers[2] + v2(circle_radius, 0.0f);
    lines[4] = circle_centers[2] + normal * circle_radius;
    lines[5] = circle_centers[3] + normal * circle_radius;
    lines[6] = circle_centers[3] - v2(circle_radius, 0.0f);
    lines[7] = circle_centers[0] - v2(circle_radius, 0.0f);

    float min_depth = INFINITY;
    v2 direction = v2();
    bool collision_exists = false;

    // Check lines first
    for(int i = 0; i < 8; i += 2)
    {
        v2 *a = &(lines[i]);
        v2 *b = &(lines[i + 1]);
        v2 n = find_ccw_normal(normalize(*b - *a));
        float dist = signed_distance_to_plane(position, *a, n);
        if(dist < 0.0f)
        {
            // No possible collision exists
            collision_exists = false;
            break;
        }
        if(dist < min_depth)
        {
            min_depth = dist;
            direction = -n;
            collision_exists = true;
        }
    }

    // Check the circles
    for(int i = 0; i < 4; i++)
    {
        v2 *center = &(circle_centers[i]);
        v2 *a;
        v2 *b = &(lines[i * 2]);
        if(i == 0) a = &(lines[7]);
        else       a = &(lines[i * 2 - 1]);
        v2 cp = position - *center;
        v2 ca = *a - *center;
        v2 cb = *b - *center;

#if 0
        {
            RendererState *renderer_state = get_engine_renderer_state();
            v4 color = v4(1, 1, 0.5f, 0.25f);
            draw_line(renderer_state, *center + new_origin, *a + new_origin, color);
            draw_line(renderer_state, *center + new_origin, *b + new_origin, color);
        }
#endif

        v2 a_n =  find_ccw_normal(normalize(ca));
        v2 b_n = -find_ccw_normal(normalize(cb));
        float a_align = dot(cp, a_n);
        float b_align = dot(cp, b_n);
        if(a_align > 0.0f && b_align > 0.0f)
        {
            if(length(cp) < circle_radius)
            {
                // Resolve to the circle's edge since that has to be the closest edge
                min_depth = circle_radius - length(cp);
                direction = normalize(cp);
                collision_exists = true;
                break;
            }
            else
            {
                // No possible collision exists
                collision_exists = false;
                break;
            }
        }
    }

#if 0
    {
        RendererState *renderer_state = get_engine_renderer_state();
        v4 color = v4(1, 1, 0.5f, 0.25f);
        draw_line(renderer_state, start + new_origin, end + new_origin, v4(0.5f, 0.0f, 1.0f, 0.75f));
        for(int i = 0; i < 4; i++)
        {
            draw_circle(renderer_state, circle_centers[i] + new_origin, circle_radius, color);
        }
        for(int i = 0; i < 8; i += 2)
        {
            v2 a = lines[i] + new_origin;
            v2 b = lines[i + 1] + new_origin;
            v2 n = find_ccw_normal(normalize(b - a));
            v2 mid = (a + b) / 2.0f;
            draw_line(renderer_state, a, b, color);
            draw_line(renderer_state, mid, mid + n * 0.1f, color);
        }
    }
#endif

    collision->depth = min_depth;
    collision->resolution_direction = direction;
    return collision_exists;
}

static float distance_to_line_segment(v2 p, v2 a, v2 b)
{
    v2 ab = b - a;
    v2 ba = a - b;
    v2 ap = p - a;
    v2 bp = p - b;

    if(dot(ab, ap) < 0.0f) return length(ap);
    if(dot(ba, bp) < 0.0f) return length(bp);

    v2 n = find_ccw_normal(normalize(ab));
    return absf(signed_distance_to_plane(p, a, n));
}

static Line *find_standing_plane(Level *level, v2 shoe_position, float shoe_radius)
{
    float min_dist = INFINITY;
    Line *closest_plane = nullptr;

    int num_chunks = level->static_level_geometry->num_chunks;
    LevelGeometryChunk **chunks = level->static_level_geometry->chunks;
    for(int i = 0; i < num_chunks; i++)
    {
        LevelGeometryChunk *chunk = chunks[i];
        int num_planes = chunk->num_physical_planes;
        Line *planes = chunk->physical_planes;
        for(int j = 0; j < num_planes; j++)
        {
            Line *plane = &(planes[j]);
            //v2 n = find_ccw_normal(*plane->b - *plane->a);
            //float dist = signed_distance_to_plane(point, *plane->a, n);

            {
                InputState *is = get_engine_input_state();
                if(key_toggled_down(is, ' '))
                {
                    int xx = 0;
                    xx++;
                }
            }

            float dist = distance_to_line_segment(shoe_position, *(plane->a), *(plane->b));

            if((dist < shoe_radius) && (dist < min_dist))
            {
                min_dist = dist;
                closest_plane = plane;
            }
        }
    }

#if 0
    {
        RendererState *renderer_state = get_engine_renderer_state();
        v4 color = v4(1.0f, 1.0f, 0.0f, 0.75f);
        draw_circle(renderer_state, shoe_position, shoe_radius, color);
        if(closest_plane)
        {
            v2 a = *(closest_plane->a);
            v2 b = *(closest_plane->b);
            v2 n = find_ccw_normal(normalize(b - a));
            v2 mid = (a + b) / 2.0f;
            draw_line(renderer_state, mid, mid + n * 0.1f, color);
        }
    }
#endif

    return closest_plane;
}

static void render_level(Level *level)
{
    RendererState *renderer_state = get_engine_renderer_state();
    Player *player = level->player;

    v2 camera_offset = v2(2.0f, 1.0f);
    set_camera_position(renderer_state, v2(player->position.x, 0.0f) + camera_offset);
    set_camera_width(renderer_state, 10.0f);

    draw_quad(renderer_state, player->position + v2(0.0f, player->full_extents.y / 2.0f), player->full_extents, 0.0f, v4(0.5f, 0.0f, 1.0f, 1.0f));

    LevelGeometryChunk *chunk = level->static_level_geometry->chunks[0];
    /*
    draw_triangles(
            renderer_state, chunk->num_vertices, chunk->vertices,
            chunk->num_triangles, (v2 **)(chunk->triangulation),
            v4(0.15f, 0.3f, 0.0f, 1.0f)
            );
    */
    for(int i = 0; i < chunk->num_physical_planes; i++)
    {
        Line *line = &(chunk->physical_planes[i]);
        draw_line(renderer_state, *line->a, *line->b, v4(1, 1, 1, 1));
    }
    /*
    draw_circle(renderer_state, player->position + player->shoe_position, player->shoe_radius, v4(1.0f, 1.0f, 0.0f, 1.0f));
    if(player->grounded)
    {
        v2 pos = player->position + player->shoe_position;
        draw_line(renderer_state, pos, pos + player->standing_plane_normal * player->shoe_radius, v4(1.0f, 1.0f, 0.0f, 1.0f));
    }
    */
}





Level *create_level(/*...*/)
{
    Level *level = (Level *)my_allocate(sizeof(Level));

    level->paused = false;

    level->static_level_geometry = (StaticLevelGeometry *)my_allocate(sizeof(StaticLevelGeometry));
    level->static_level_geometry->num_chunks = 1;
    level->static_level_geometry->chunks = (LevelGeometryChunk **)my_allocate(sizeof(LevelGeometryChunk *) * level->static_level_geometry->num_chunks);
    level->static_level_geometry->chunks[0] = create_piece_of_level_geometry();

    level->kinematic_level_geometry = (KinematicLevelGeometry *)my_allocate(sizeof(KinematicLevelGeometry));
    // ...

    level->num_enemies = 0;
    level->enemies = nullptr;
    // ...

    level->player = (Player *)my_allocate(sizeof(Player)); 
    reset_player(level->player);
    // ...

    level->gravity_force = -9.81f;

    return level;
}

void level_step(Level *level, float dt)
{
    InputState *input_state = get_engine_input_state();
    RendererState *renderer_state = get_engine_renderer_state();

    if(key_toggled_down(input_state, 0x1B)) // ESCAPE
    {
        level->paused = !level->paused;
    }

    if(key_toggled_down(input_state, 'R'))
    {
        reset_player(level->player);
    }

    if(level->paused)
    {
    }
    else
    {
        Player *player = level->player;

        // Input to drive kinematics
        float horizontal_force = 0.0f;
        float vertical_force = 0.0f;

        if(key_state(input_state, 'A'))
        {
            horizontal_force -= player->acceleration_strength;
        }
        if(key_state(input_state, 'D'))
        {
            horizontal_force += player->acceleration_strength;
        }

        // Drag
        horizontal_force -= player->horizontal_speed * player->drag_strength;
        vertical_force -= player->vertical_speed * player->drag_strength;

        // Friction
        // Should apply even if airborne to make movement feel consistent?
        horizontal_force -= player->horizontal_speed * player->friction_strength;

        v2 horizontal_direction = v2(1.0f, 0.0f);
        v2 vertical_direction = v2(0.0f, 1.0f);
        if(player->grounded)
        {
            v2 plane_normal = player->standing_plane_normal;
            v2 plane_direction = find_ccw_normal(plane_normal);

            horizontal_direction = plane_direction;

            if(key_toggled_down(input_state, ' '))
            {
                player->vertical_speed = player->jump_strength;
                player->grounded = false;
            }
        }
        else
        {
            vertical_force += level->gravity_force;
        }

        player->horizontal_speed += horizontal_force * dt;
        player->position += horizontal_direction * player->horizontal_speed * dt;

        player->vertical_speed += vertical_force * dt;
        player->position += vertical_direction * player->vertical_speed * dt;





        // NOTE: Right code top down! (usage code first)

#if 1
        for(int iteration = 0; iteration < 4; iteration++)
        {
            // Collision detection and resolution
            Collision collision_data[16] = {};
            Collision *collision_end = &(collision_data[0]);


            int num_chunks = level->static_level_geometry->num_chunks;
            LevelGeometryChunk **chunks = level->static_level_geometry->chunks;
            for(int i = 0; i < num_chunks; i++)
            {
                LevelGeometryChunk *chunk = chunks[i];
                int num_planes = chunk->num_physical_planes;
                Line *planes = chunk->physical_planes;
                for(int j = 0; j < num_planes; j++)
                {
                    Line *plane = &(planes[j]);

                    //v2 player_bl = player->position + v2(-player->full_extents.x / 2.0f, 0.0f);
                    //v2 player_tr = player->position + v2( player->full_extents.x / 2.0f, player->full_extents.y);
                    //bool hit = box_line_collision(player_bl, player_tr, *plane->a, *plane->b, collision_end);
                    v2 pos = player->position + v2(0.0f, player->full_extents.y / 2.0f);
                    bool hit = cylinder_line_collision(pos, player->full_extents, *plane->a, *plane->b, collision_end);
                    if(hit)
                    {
                        collision_end++;
                    }
                    //draw_line(renderer_state, *plane->a, *plane->b, v4(0, 1, 0, 1));
                }
            }

            int num_collisions = collision_end - &(collision_data[0]);
            for(int i = 0; i < num_collisions; i++)
            {
                Collision *collision = &(collision_data[i]);

                float max_slope_angle = PI / 2.0f;
                float plane_angle = angle_between(collision->resolution_direction, v2(0.0f, 1.0f));

                if(absf(plane_angle) < max_slope_angle / 2.0f)
                {
                    player->grounded = true;
                    player->vertical_speed = 0.0f;
                }

                player->position += collision->resolution_direction * collision->depth * 0.75f;
            }

        }
#endif

        Line *closest_plane = find_standing_plane(level, player->position + player->shoe_position, player->shoe_radius);
        if(player->grounded && closest_plane == nullptr)
        {
            player->grounded = false;
        }
        if(closest_plane != nullptr)
        {
            v2 standing_plane_normal = find_ccw_normal(normalize(*closest_plane->b - *closest_plane->a));
            player->standing_plane_normal = standing_plane_normal;
        }

#if 0
        {
            // Testing collision code
            static v2 a, b;
            static v2 c = v2(1, 1);
            static v2 d = v2(2, 2);
            v4 color = v4(1, 0, 0, 1);

            if(mouse_state(input_state, 0))
            {
                a = mouse_world_position(input_state);
            }

            b = mouse_world_position(input_state);

            Collision col;
            bool hit = box_line_collision(c, d, a, b, &col);

            if(hit) color = v4(0, 1, 0, 1);
            else color = v4(1, 0, 0, 1);

            v2 bl = c;
            v2 tr = d;
            v2 br = v2(tr.x, bl.y);
            v2 tl = v2(bl.x, tr.y);

            draw_line(renderer_state, a, b, color);
            draw_line(renderer_state, bl, br, color);
            draw_line(renderer_state, br, tr, color);
            draw_line(renderer_state, tr, tl, color);
            draw_line(renderer_state, tl, bl, color);
        }
#endif

#if 0
        {
            static v2 a, b;
            static v2 pos = v2();
            static v2 dim = v2(1.0f, 2.0f) * 0.5f;
            v4 color = v4(1, 1, 1, 1);

            if(mouse_state(input_state, 0))
            {
                a = mouse_world_position(input_state);
            }

            b = mouse_world_position(input_state);

            draw_cylinder(renderer_state, pos, dim, v4(1, 1, 1, 1));
            draw_quad(renderer_state, pos, v2(0.05f, 0.05f), 0.0f, v4(1, 1, 0, 1));

            Collision col;
            bool hit = cylinder_line_collision(pos, dim, a, b, &col);
            if(hit)
            {
                draw_line(renderer_state, pos, pos + col.resolution_direction * col.depth, v4(0, 1, 0, 1));
            }
        }
#endif

#if 0
        {

            static bool has_happened = false;
            static v2 points[100];
            static int num_points = _countof(points);
            static int num_hull_lines;
            static v2 *convex_hull[100];

            if(!has_happened)
            {
#if 1
                for(int i = 0; i < num_points; i++) points[i] = v2(random_range(-3.0f, 3.0f), random_range(-1.0f, 1.0f));
#else
                points[0] = v2(0.0f, 0.0f);
                points[1] = v2(1.0f, 0.0f);
                points[2] = v2(2.0f, 0.0f);
                points[3] = v2(2.0f, 1.0f);
                points[4] = v2(2.0f, 2.0f);
                points[5] = v2(1.0f, 2.0f);
                points[6] = v2(0.0f, 2.0f);
                points[7] = v2(0.0f, 1.0f);
                num_points = 8;
#endif

                find_convex_hull(num_points, points, &num_hull_lines, convex_hull);

                int d_num_hull_lines = num_hull_lines;
                v2 **d_convex_hull = convex_hull;

                has_happened = true;
            }

            if(key_toggled_down(input_state, ' '))
            {
                for(int i = 0; i < num_points; i++) points[i] = v2(random_range(-3.0f, 3.0f), random_range(-1.0f, 1.0f));
                find_convex_hull(num_points, points, &num_hull_lines, convex_hull);
            }

            v2 lowest_point = points[0];
            for(int i = 0; i < num_points; i++)
            {
                v2 point = points[i];
                if(point.y < lowest_point.y)
                {
                    if(absf(point.y - lowest_point.y) < EPSILON)
                    {
                        lowest_point = (point.x < lowest_point.x) ? point : lowest_point;
                    }
                    else
                    {
                        lowest_point = point;
                    }
                }
            }

            for(int i = 0; i < num_points; i++)
            {
                draw_quad(renderer_state, points[i], v2(0.05f, 0.05f), PI / 2.0f, v4(1, 1, 1, 0.5f));
            }
            for(int i = 0; i < num_hull_lines; i++)
            {
                Line line = { (convex_hull[i]), (convex_hull[(i + 1) % num_hull_lines]) };

                draw_line(renderer_state, *line.a, *line.b, v4(1, 1, 1, 1));

                v2 line_center = (*line.a + *line.b) / 2.0;
                v2 diff = *line.b - *line.a;
                v2 line_normal = normalize(find_ccw_normal(diff));

                draw_line(renderer_state, line_center, line_center + line_normal*0.1f, v4(1, 1, 0, 0.75f));
            }

            draw_quad(renderer_state, lowest_point, v2(0.05f, 0.05f), PI / 2.0f, v4(1, 1, 0, 1.0f));
        }
#endif



    }

    render_level(level);
}

