#include "global.h"
#include "drawing.h"
#include "field.h"
#include "inventory.h"
#include "main.h"
#include "menu.h"
#include "text.h"
#include "title_screen.h"
#include "world.h"

//For collision detection
//The player's bounding box is a prism that encompasses everything within 
//playerPosition.x - PLAYER_RADIUS to playerPosition.x + PLAYER_RADIUS,
//playerPosition.z - PLAYER_RADIUS to playerPosition.z + PLAYER_RADIUS, and
//playerPosition.y to playerPosition.y + PLAYER_HEIGHT
#define PLAYER_RADIUS 0.5
#define PLAYER_HEIGHT 1.5

//How far the player's eyes are above where they are standing
#define EYE_LEVEL 1.5

//How far the block selection can reach
#define SELECT_RADIUS 5

enum
{
    STANDING,
    MIDAIR,
    SWIMMING,
};

struct Vec3i
{
    int x;
    int y;
    int z;
};

struct Vec3f
{
    float x;
    float y;
    float z;
};

static struct Vec3f playerPosition;
static float yVelocity;
static float yaw;  //positive value means looking right
static float pitch;  //positive value means looking up
static int state;
static bool selectedBlockActive;
static struct Vec3i selectedBlockPos;
static struct Vec3i selectedBlockFace;
static bool showDebugInfo;

static struct MenuItem pauseMenuItems[] = {
    {"Continue"},
    {"Quit"},
};

static struct Menu pauseMenu = {
    "Paused",
    pauseMenuItems,
    ARRAY_LENGTH(pauseMenuItems),
};

static void field_main(void);
static void field_draw(void);

//==================================================
// Pause Menu
//==================================================

static void pause_menu_main(void)
{
    switch (menu_process_input())
    {
        case 0: //Continue
            set_main_callback(field_main);
            set_draw_callback(field_draw);
            break;
        case 1: //Quit
            world_close();
            title_screen_init();
            break;
    }
}

static void pause_menu_draw(void)
{
    field_draw();
    menu_draw();
}

static void open_pause_menu(void)
{
    drawing_set_2d_mode();
    menu_init(&pauseMenu);
    set_main_callback(pause_menu_main);
    set_draw_callback(pause_menu_draw);
}

//==================================================
// Overworld Functions
//==================================================

//See "A Fast Voxel Traversal Algorithm for Ray Tracing" by John Amanatides and Andrew Woo
static void get_selected_block(void)
{
    struct Vec3f direction = {
        cosf(DegToRad(90 - yaw)) * cosf(DegToRad(pitch)),
        sinf(DegToRad(pitch)),
        -sinf(DegToRad(90 - yaw)) * cosf(DegToRad(pitch)),
    };
    float eyeY = playerPosition.y + EYE_LEVEL;
    //coordinates of block we are currently at
    int x = floorf(playerPosition.x);
    int y = floorf(eyeY);
    int z = floorf(playerPosition.z);
    //increment/decrement coordinates
    int stepX = 0;
    int stepY = 0;
    int stepZ = 0;
    //ray distance it takes to equal one unit in each direction
    float tDeltaX = INFINITY;
    float tDeltaY = INFINITY;
    float tDeltaZ = INFINITY;
    //ray distance it takes to move to next block boundary in each direction
    float tMaxX = INFINITY;
    float tMaxY = INFINITY;
    float tMaxZ = INFINITY;
    
    if (direction.x > 0.0)
    {
        stepX = 1;
        tDeltaX = fabsf(1.0 / direction.x);
        tMaxX = fabsf((floorf(playerPosition.x + 1.0) - playerPosition.x) / direction.x);
    }
    else if (direction.x < 0.0)
    {
        stepX = -1;
        tDeltaX = fabsf(1.0 / direction.x);
        tMaxX = fabsf((playerPosition.x - ceilf(playerPosition.x - 1.0)) / direction.x);
    }
    if (direction.y > 0.0)
    {        
        stepY = 1;
        tDeltaY = fabsf(1.0 / direction.y);
        tMaxY = fabsf((floorf(eyeY + 1.0) - eyeY) / direction.y);
    }
    else if (direction.y < -0.0)
    {
        stepY = -1;
        tDeltaY = fabsf(1.0 / direction.y);
        tMaxY = fabsf((eyeY - ceilf(eyeY - 1.0)) / direction.y);
    }
    if (direction.z > 0.0)
    {
        stepZ = 1;
        tDeltaZ = fabsf(1.0 / direction.z);
        tMaxZ = fabsf((floorf(playerPosition.z + 1.0) - playerPosition.z) / direction.z);
    }
    else if (direction.z < -0.0)
    {
        stepZ = -1;
        tDeltaZ = fabsf(1.0 / direction.z);
        tMaxZ = fabsf((playerPosition.z - ceilf(playerPosition.z - 1.0)) / direction.z);
    }
    
    selectedBlockActive = false;
    
    for (int i = 0; i < SELECT_RADIUS; i++)
    {
        if (tMaxX < tMaxY)
        {
            if (tMaxX < tMaxZ)
            {
                //increment x
                tMaxX += tDeltaX;
                x += stepX;
                selectedBlockFace.x = -stepX;
                selectedBlockFace.y = 0;
                selectedBlockFace.z = 0;
            }
            else
            {
                goto increment_z;
            }
        }
        else
        {
            if (tMaxY < tMaxZ)
            {
                //increment y
                tMaxY += tDeltaY;
                y += stepY;
                selectedBlockFace.x = 0;
                selectedBlockFace.y = -stepY;
                selectedBlockFace.z = 0;
            }
            else
            {
                //increment z
              increment_z:
                tMaxZ += tDeltaZ;
                z += stepZ;
                selectedBlockFace.x = 0;
                selectedBlockFace.y = 0;
                selectedBlockFace.z = -stepZ;
            }
        }
        
        if (BLOCK_IS_SOLID(world_get_block_at(x, y, z)))
        {
            selectedBlockPos.x = x;
            selectedBlockPos.y = y;
            selectedBlockPos.z = z;
            selectedBlockActive = true;
            return;
        }
    }
}

static void draw_crosshair(void)
{
    u8 crosshairColor[] ATTRIBUTE_ALIGN(32) = {255, 255, 255};
    
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_U16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
    GX_SetArray(GX_VA_CLR0, crosshairColor, 3 * sizeof(u8));
    
    GX_Begin(GX_LINES, GX_VTXFMT0, 4);
    GX_Position2u16(gDisplayWidth / 2, gDisplayHeight / 2 + 10);
    GX_Color1x8(0);
    GX_Position2u16(gDisplayWidth / 2, gDisplayHeight / 2 - 10);
    GX_Color1x8(0);
    GX_Position2u16(gDisplayWidth / 2 + 10, gDisplayHeight / 2);
    GX_Color1x8(0);
    GX_Position2u16(gDisplayWidth / 2 - 10, gDisplayHeight / 2);
    GX_Color1x8(0);
    GX_End();
}

static void draw_block_selection(void)
{
    u8 blockSelectionColor[] ATTRIBUTE_ALIGN(32) = {0, 0, 0};
    
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
    GX_SetArray(GX_VA_CLR0, blockSelectionColor, 3 * sizeof(u8));
    
    GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 5);
    if (selectedBlockFace.x != 0)
    {
        int x = selectedBlockPos.x + ((selectedBlockFace.x > 0) ? 1 : 0);
        
        GX_Position3f32(x, selectedBlockPos.y + 1, selectedBlockPos.z + 1);
        GX_Color1x8(0);
        GX_Position3f32(x, selectedBlockPos.y + 1, selectedBlockPos.z);
        GX_Color1x8(0);
        GX_Position3f32(x, selectedBlockPos.y, selectedBlockPos.z);
        GX_Color1x8(0);
        GX_Position3f32(x, selectedBlockPos.y, selectedBlockPos.z + 1);
        GX_Color1x8(0);
        GX_Position3f32(x, selectedBlockPos.y + 1, selectedBlockPos.z + 1);
        GX_Color1x8(0);
    }
    else if (selectedBlockFace.y != 0)
    {
        int y = selectedBlockPos.y + ((selectedBlockFace.y > 0) ? 1 : 0);
        
        GX_Position3f32(selectedBlockPos.x, y, selectedBlockPos.z);
        GX_Color1x8(0);
        GX_Position3f32(selectedBlockPos.x + 1, y, selectedBlockPos.z);
        GX_Color1x8(0);
        GX_Position3f32(selectedBlockPos.x + 1, y, selectedBlockPos.z + 1);
        GX_Color1x8(0);
        GX_Position3f32(selectedBlockPos.x, y, selectedBlockPos.z + 1);
        GX_Color1x8(0);
        GX_Position3f32(selectedBlockPos.x, y, selectedBlockPos.z);
        GX_Color1x8(0);
    }
    else if (selectedBlockFace.z != 0)
    {
        float z = selectedBlockPos.z + ((selectedBlockFace.z > 0) ? 1 : 0);
        
        GX_Position3f32(selectedBlockPos.x, selectedBlockPos.y + 1, z);
        GX_Color1x8(0);
        GX_Position3f32(selectedBlockPos.x + 1, selectedBlockPos.y + 1, z);
        GX_Color1x8(0);
        GX_Position3f32(selectedBlockPos.x + 1, selectedBlockPos.y, z);
        GX_Color1x8(0);
        GX_Position3f32(selectedBlockPos.x, selectedBlockPos.y, z);
        GX_Color1x8(0);
        GX_Position3f32(selectedBlockPos.x, selectedBlockPos.y + 1, z);
        GX_Color1x8(0);
    }
    else
    {
        assert(false);
    }
    GX_End();
}

//TODO: This collision check can be improved.
static void apply_motion_vector(struct Vec3f motion)
{
    //Clamp fall speed to avoid tunneling problem on large falls. This should be very rare.
    if (motion.y < -1.0)
        motion.y = -1.0;
    
    if (motion.x > 0.0)
    {
        if (BLOCK_IS_SOLID(world_get_block_at(playerPosition.x + motion.x + PLAYER_RADIUS, playerPosition.y, playerPosition.z)))
            motion.x = 0;
    }
    else if (motion.x < 0.0)
    {
        if (BLOCK_IS_SOLID(world_get_block_at(playerPosition.x + motion.x - PLAYER_RADIUS, playerPosition.y, playerPosition.z)))
            motion.x = 0;
    }
    
    if (motion.y > 0.0)
    {
        //TODO: Implement later when we are able to test this.
        assert(state == MIDAIR);
    }
    else if (motion.y < 0.0) //Player hit the ground
    {
        assert(state == MIDAIR);
        if (BLOCK_IS_SOLID(world_get_block_at(playerPosition.x, playerPosition.y + motion.y, playerPosition.z)))
        {
            state = STANDING;
            yVelocity = 0;
            motion.y = 0;
        }
    }
    
    if (motion.z > 0.0)
    {
        if (BLOCK_IS_SOLID(world_get_block_at(playerPosition.x, playerPosition.y, playerPosition.z + motion.z + PLAYER_RADIUS)))
            motion.z = 0;
    }
    else if (motion.z < 0.0)
    {
        if (BLOCK_IS_SOLID(world_get_block_at(playerPosition.x, playerPosition.y, playerPosition.z + motion.z - PLAYER_RADIUS)))
            motion.z = 0;
    }
    
    playerPosition.x += motion.x;
    playerPosition.y += motion.y;
    playerPosition.z += motion.z;
    
    if (state == STANDING)
    {
        if (!BLOCK_IS_SOLID(world_get_block_at(playerPosition.x, playerPosition.y - 1.0, playerPosition.z)))
        {
            state = MIDAIR;
        }
    }
}

static void field_main(void)
{
    struct Vec3f motion;
    float forward = 0.0;
    float right = 0.0;
    
    if (gControllerPressedKeys & PAD_BUTTON_START)
        open_pause_menu();
    
    if (state == STANDING)
    {
        assert(yVelocity == 0.0);
        if (gControllerPressedKeys & PAD_BUTTON_A)
        {
            state = MIDAIR;
            yVelocity = 0.18;
        }
    }
    
    if (gControllerPressedKeys & PAD_BUTTON_B)
    {
        if (selectedBlockActive)
        {
            int block = world_get_block_at(selectedBlockPos.x, selectedBlockPos.y, selectedBlockPos.z);
            
            world_set_block(selectedBlockPos.x, selectedBlockPos.y, selectedBlockPos.z, BLOCK_AIR);
            inventory_add_block(block);
        }
    }
    else if (gControllerPressedKeys & PAD_BUTTON_Y)
    {
        if (selectedBlockActive && inventory[inventorySelection].count > 0)
        {
            world_set_block(selectedBlockPos.x + selectedBlockFace.x,
                            selectedBlockPos.y + selectedBlockFace.y,
                            selectedBlockPos.z + selectedBlockFace.z,
                            inventory[inventorySelection].type);
            inventory[inventorySelection].count--;
        }
    }
    
    if (gControllerPressedKeys & PAD_TRIGGER_Z)
        showDebugInfo = !showDebugInfo;
        
    if (gControllerPressedKeys & PAD_BUTTON_LEFT)
    {
        inventorySelection--;
        if (inventorySelection == -1)
            inventorySelection = NUM_ITEM_SLOTS - 1;
    }
    else if (gControllerPressedKeys & PAD_BUTTON_RIGHT)
    {
        inventorySelection++;
        if (inventorySelection == NUM_ITEM_SLOTS)
            inventorySelection = 0;
    }
    
    if (gCStickX > 10 || gCStickX < -10)
        yaw += (float)gCStickX / 100.0;
    if (gCStickY > 10 || gCStickY < -10)
        pitch += (float)gCStickY / 100.0;
    
    if (gAnalogStickX > 10 || gAnalogStickX < -10)
        right = (float)gAnalogStickX / 1000.0;
    if (gAnalogStickY > 10 || gAnalogStickY < -10)
        forward = (float)gAnalogStickY / 1000.0;
    
    //Wrap yaw to -180 to 180
    if (yaw > 180.0)
        yaw -= 360.0;
    else if (yaw < -180.0)
        yaw += 360.0;
    
    //Restrict pitch range to -90 to 90
    if (pitch > 90.0)
        pitch = 90.0;
    else if (pitch < -90.0)
        pitch = -90.0;
    
    switch (state)
    {
        case MIDAIR:
            yVelocity -= 0.01;
            break;
        case SWIMMING: //not implemented yet.
            break;
    }
    
    motion.x = right * sinf(DegToRad(yaw + 90.0)) - forward * cosf(DegToRad(yaw + 90.0));
    motion.y = yVelocity;
    motion.z = -forward * sinf(DegToRad(yaw + 90.0)) - right * cosf(DegToRad(yaw + 90.0));
    
    apply_motion_vector(motion);
    get_selected_block();
}

static void render_scene(void)
{
    Mtx posMtx;
    Mtx rotMtx;
    Mtx yawRotMtx;
    Mtx pitchRotMtx;
    guVector axis;
    
    guMtxIdentity(posMtx);
    guMtxApplyTrans(posMtx, posMtx, -playerPosition.x, -(playerPosition.y + EYE_LEVEL), -playerPosition.z);
    
    axis = (guVector){0.0, 1.0, 0.0};
    guMtxRotAxisDeg(yawRotMtx, &axis, yaw);
    
    axis = (guVector){-1.0, 0.0, 0.0};
    guMtxRotAxisDeg(pitchRotMtx, &axis, pitch);
    
    guMtxConcat(pitchRotMtx, yawRotMtx, rotMtx);
    guMtxConcat(rotMtx, posMtx, posMtx);
    GX_LoadPosMtxImm(posMtx, GX_PNMTX0);
    
    world_render_chunks_at(playerPosition.x, playerPosition.z);
}

static char *get_state_text(void)
{
    switch (state)
    {
        case STANDING:
            return "STANDING";
            break;
        case MIDAIR:
            return "MIDAIR";
            break;
        case SWIMMING:
            return "SWIMMING";
            break;
        default:
            return "UNDEFINED";
    }
}

static void field_draw(void)
{
    struct Chunk *chunk = world_get_chunk_containing(playerPosition.x, playerPosition.z);
    
    drawing_set_3d_mode();
    render_scene();
    if (selectedBlockActive)
        draw_block_selection();
    drawing_set_2d_mode();
    inventory_draw();
    if (showDebugInfo)
    {
        text_set_font_size(8, 16);
        text_init();
        text_draw_string_formatted(50, 50, false, "Position: (%.2f, %.2f, %.2f), Chunk: (%i, %i)",
                                                  playerPosition.x, playerPosition.y, playerPosition.z, chunk->x, chunk->z);
        text_draw_string_formatted(50, 66, false, "Camera angle: (%.2f, %.2f)",
                                                  yaw, pitch);
        if (selectedBlockActive)
            text_draw_string_formatted(50, 82, false, "Selected block: (%i, %i, %i)", selectedBlockPos.x, selectedBlockPos.y, selectedBlockPos.z);
        else
            text_draw_string(50, 82, false, "Selected block: none");
        text_draw_string_formatted(50, 98, false, "State: %s", get_state_text());
    }
    draw_crosshair();
}

void field_init(void)
{
    struct Chunk *chunk;
    int x, y, z;
    
    world_init();
    playerPosition.x = 0.0;
    playerPosition.z = 0.0;
    yaw = 0.0;
    pitch = 0.0;
    chunk = world_get_chunk_containing(playerPosition.x, playerPosition.z);
    x = (unsigned int)floor(playerPosition.x) % CHUNK_WIDTH;
    z = (unsigned int)floor(playerPosition.z) % CHUNK_WIDTH;
    
    for (y = CHUNK_HEIGHT -1; y >= 0; y--)
    {
        if (BLOCK_IS_SOLID(chunk->blocks[x][y][z]))
        {
            y++;
            break;
        }
    }
    playerPosition.y = y;
    state = STANDING;
    yVelocity = 0.0;
    selectedBlockActive = false;
    inventory_init();
    showDebugInfo = false;
    
    text_set_font_size(8, 16);
    set_main_callback(field_main);
    set_draw_callback(field_draw);
}
