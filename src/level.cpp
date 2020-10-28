
#include "level.h"
#include "memory.h"
#include "my_math.h"
#include "my_algorithms.h"
#include "renderer.h"
#include "engine.h"
#include "input.h"

#include "imgui.h"


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

    float max_slope;

    bool grounded;
    struct LevelGeometryChunk *standing_chunk;
    Line *standing_plane;
    float standing_plane_t;
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
    StaticLevelGeometry *static_geometry;
    KinematicLevelGeometry *kinematic_geometry;

    int num_enemies;
    Enemy *enemies;

    float gravity_force; // A level has a gravity force, make this a vector force to easily add cool level changing effects!
};
static const float EPSILON = 0.0001f;




static float length(Line *a)
{
    return length(*a->b - *a->a);
}

static v2 find_ccw_normal(Line *line)
{
    return find_ccw_normal(normalize(*line->b - *line->a));
}

static void reset_player(Player *player, LevelGeometryChunk *start_chunk, Line *start_plane)
{
    player->position = v2(2.0f, 2.0f);
    player->full_extents = v2(1.0f, 2.0f) * 0.5f;
    player->acceleration_strength = 32.0f;
    player->horizontal_speed = 0.0f;
    player->vertical_speed = 0.0f;
    player->friction_strength = 12.0f;
    player->drag_strength = 0.5f;
    player->jump_strength = 5.0f;

    player->max_slope = 0.707;

    player->grounded = true;
    player->standing_chunk = start_chunk;
    player->standing_plane = start_plane;
    player->standing_plane_t = 0.5f;
}

static LevelGeometryChunk *create_piece_of_level_geometry()
{
    LevelGeometryChunk *chunk = (LevelGeometryChunk *)my_allocate(sizeof(LevelGeometryChunk));

#if 1
    v2 vertices[] =
    {
        {  0.0f,   3.0f }, // 0     0---9
        {  0.0f, -10.0f }, // 1     |   |         6------5
        { 11.0f, -10.0f }, // 2     |   |        /        \       .
        { 11.0f,   0.0f }, // 3     |   8------7           4------3
        {  7.5f,   0.0f }, // 4     |                             |
        {  7.0f,   1.0f }, // 5     |                             |
        {  5.0f,   1.0f }, // 6     |                             |
        {  3.0f,   0.0f }, // 7     |                             |
        {  1.0f,   0.0f }, // 8     |                             |
        {  1.0f,   3.0f }, // 9     1-----------------------------2
    };

    int num_vertices = _countof(vertices);
    chunk->num_vertices = num_vertices;
    chunk->vertices = (v2 *)my_allocate(sizeof(v2) * num_vertices);
    my_memcpy(chunk->vertices, vertices, sizeof(vertices));

    chunk->num_physical_planes = num_vertices;
    chunk->physical_planes = (Line *)my_allocate(sizeof(Line) * chunk->num_physical_planes);
    for(int i = num_vertices - 1; i >= 0; i--)
    {
        v2 *a = &(chunk->vertices[i]);
        v2 *b = &(chunk->vertices[(i > 0) ? (i - 1) : (num_vertices - 1)]);
        chunk->physical_planes[(num_vertices - 1) - i] = {a, b};
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


static void draw_capsule(RendererState *renderer_state, v2 pos, v2 full_extents, v4 color)
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

    if(dot(q1 - p1, p_n) * dot(q2 - p1, p_n) > 0.0f + EPSILON) return false;
    if(dot(p1 - q1, q_n) * dot(p2 - q1, q_n) > 0.0f + EPSILON) return false;

    float small = dot(p1, p_n) - dot(q1, p_n);
    float big = dot(q2, p_n) - dot(q1, p_n);;
    float ratio = small / big;
    *hit_point = q1 + (q2 - q1) * ratio;

    return true;
}

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

static bool capsule_line_collision(v2 position, v2 full_extents, v2 start, v2 end, Collision *collision)
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

static Line *get_previous_plane(LevelGeometryChunk *chunk, Line *plane)
{
    Line *start = chunk->physical_planes;
    plane--;
    if(plane < start) plane += chunk->num_physical_planes;
    return plane;
}

static Line *get_next_plane(LevelGeometryChunk *chunk, Line *plane)
{
    Line *end = chunk->physical_planes + chunk->num_physical_planes;
    plane++;
    if(plane >= end) plane -= chunk->num_physical_planes;
    return plane;
}



static void render_level(Level *level)
{
    RendererState *renderer_state = get_engine_renderer_state();
    Player *player = level->player;

    v2 camera_offset = v2(2.0f, 1.0f);
    set_camera_position(renderer_state, v2(player->position.x, 0.0f) + camera_offset);
    set_camera_width(renderer_state, 10.0f);

#if 0
    draw_quad(renderer_state, player->position + v2(0.0f, player->full_extents.y / 2.0f), player->full_extents,
            0.0f, v4(0.5f, 0.0f, 1.0f, 1.0f));
#endif

#if 1
    draw_capsule(renderer_state, player->position + v2(0.0f, player->full_extents.y / 2.0f), player->full_extents,
            v4(0.0f, 0.8f, 0.0f, 1.0f));
#endif

    LevelGeometryChunk *chunk = level->static_geometry->chunks[0];
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
        v2 midpoint = (*line->a + *line->b) / 2.0f;
        v2 normal = find_ccw_normal(normalize(*line->b - *line->a));
        draw_line(renderer_state, midpoint, midpoint + normal * 0.1f, v4(1, 1, 1, 1));
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

    level->static_geometry = (StaticLevelGeometry *)my_allocate(sizeof(StaticLevelGeometry));
    level->static_geometry->num_chunks = 1;
    level->static_geometry->chunks = (LevelGeometryChunk **)my_allocate(sizeof(LevelGeometryChunk *) * level->static_geometry->num_chunks);
    level->static_geometry->chunks[0] = create_piece_of_level_geometry();

    level->kinematic_geometry = (KinematicLevelGeometry *)my_allocate(sizeof(KinematicLevelGeometry));
    // ...

    level->num_enemies = 0;
    level->enemies = nullptr;
    // ...

    level->player = (Player *)my_allocate(sizeof(Player)); 
    reset_player(level->player,
            level->static_geometry->chunks[0],
            &(level->static_geometry->chunks[0]->physical_planes[3]));

    // ...

    level->gravity_force = -9.81f;

    return level;
}


static void do_ground_movement(Level *level, Player *player, float dt);
static void do_freefall_movement(Level *level, Player *player, float dt);

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
        reset_player(level->player,
                level->static_geometry->chunks[0],
                &(level->static_geometry->chunks[0]->physical_planes[3]));
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

        if(player->grounded)
        {
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
        player->vertical_speed += vertical_force * dt;

        if(absf(player->horizontal_speed) < EPSILON) player->horizontal_speed = 0.0f;
        if(absf(player->vertical_speed)   < EPSILON) player->vertical_speed   = 0.0f;



        if(player->grounded)
        {
            do_ground_movement(level, player, dt);
        }
        else
        {
            do_freefall_movement(level, player, dt);
        }

    }

    render_level(level);
}

// Here for figuring out how to move a circle along a plane corner
#if 0
static float delta_plane_length(Line *p1, Line *p2, float player_radius)
{
    float angle_between_planes = angle_between(*p1->b - *p1->a, *p2->b - *p2->a);
    float delta = angle_between_planes * player_radius * 0.5f; // Half of the arc length made by the planes
    return delta;
}
#endif

static float find_distance_to_end(Player *player, Line *next_plane, bool moving_forward)
{
    float distance_to_end = (moving_forward) ?
        (1.0f - player->standing_plane_t) :
        (player->standing_plane_t);

    return distance_to_end;
}

static void move_along_plane(Player *player, bool moving_forward, float *movement_distance_remaining)
{
    float direction = (moving_forward) ? 1.0f : -1.0f;
    float standing_plane_length = length(player->standing_plane);

    float t_dist_to_move = *movement_distance_remaining / standing_plane_length;

    player->standing_plane_t += t_dist_to_move * direction;
    *movement_distance_remaining = 0.0f;
}

static void move_to_next_plane(Player *player, bool moving_forward, Line *next_plane, float distance_to_end,
        float *movement_distance_remaining)
{
    *movement_distance_remaining -= distance_to_end;

    player->standing_plane_t = (moving_forward) ?  (0.0f) : (1.0f);
    player->standing_plane = next_plane;
}

static void do_ground_movement(Level *level, Player *player, float dt)
{
    RendererState *renderer_state = get_engine_renderer_state();

    float current_velocity = player->horizontal_speed * dt;
    float movement_distance_remaining = absf(current_velocity);
    bool moving_forward = (current_velocity > 0.0f);

    // While still has movement distance
    while(movement_distance_remaining > 0.0f)
    {
        Line *next_plane = (moving_forward) ?
            (get_next_plane(player->standing_chunk, player->standing_plane)) :
            (get_previous_plane(player->standing_chunk, player->standing_plane));
        float distance_to_end = find_distance_to_end(player, next_plane, moving_forward);

        bool will_move_past = (movement_distance_remaining > distance_to_end);
        if(!will_move_past)
        {
            // Move player along plane
            move_along_plane(player, moving_forward, &movement_distance_remaining);
            break;
        }
        else
        {
            // Move player to next plane
            move_to_next_plane(player, moving_forward, next_plane, distance_to_end, &movement_distance_remaining);
        }

    }

    v2 on_plane_position = lerp(*player->standing_plane->a, *player->standing_plane->b, player->standing_plane_t);
    player->position = on_plane_position;
}

static void do_freefall_movement(Level *level, Player *player, float dt)
{
    v2 velocity = v2(player->horizontal_speed, player->vertical_speed) * dt;
    v2 old_player_position = player->position;
    v2 new_player_position = player->position + velocity;

    player->position = new_player_position;

    bool hit_plane = false;
    LevelGeometryChunk *collided_chunk = nullptr;
    Line *closest_plane = nullptr;
    v2 hit_point;
    float min_dist = INFINITY;
    for(int chunk_index = 0; chunk_index < level->static_geometry->num_chunks; chunk_index++)
    {
        LevelGeometryChunk *chunk = level->static_geometry->chunks[chunk_index];

        for(int plane_index = 0; plane_index < chunk->num_physical_planes; plane_index++)
        {
            Line *plane = &(chunk->physical_planes[plane_index]);
            v2 plane_normal = find_ccw_normal(plane);

            if(dot(plane_normal, velocity) < 0.0f)
            {
                v2 plane_hit_point;
                bool hit = line_line_collision(old_player_position, new_player_position, *plane->a, *plane->b, &plane_hit_point);
                if(hit)
                {
                    float dist = distance(old_player_position, plane_hit_point);
                    if(dist < min_dist)
                    {
                        hit_plane = true;
                        collided_chunk = chunk;
                        closest_plane = plane;
                        hit_point = plane_hit_point;
                        min_dist = dist;
                    }
                }
            }
        }
    }

    if(hit_plane)
    {
        v2 hit_plane_normal = find_ccw_normal(closest_plane);
        if(dot(hit_plane_normal, v2(1.0f, 0.0f)) < player->max_slope)
        {
            player->grounded = true;
            player->standing_chunk = collided_chunk;
            player->standing_plane = closest_plane;
            player->standing_plane_t = distance(hit_point, *closest_plane->a) / length(closest_plane);

            player->position = hit_point; // Shouldn't have to do this here
        }
        else
        {
            //v2 hit_plane_direction = normalize(*closest_plane->b - *closest_plane->a);
            //v2 
            player->position = hit_point + hit_plane_normal * 0.1f; // Shouldn't have to do this here
        }
    }
}

