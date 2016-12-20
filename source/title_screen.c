#include "global.h"
#include "drawing.h"
#include "field.h"
#include "file.h"
#include "keyboard.h"
#include "main.h"
#include "menu.h"
#include "text.h"
#include "title_screen.h"

#define MAX_SAVE_FILES 5
#define SAVENAME_MAX 16  //Maximum length of a file name (including the null terminator)
#define SEED_MAX 16      //Maximum length of a seed string (including the null terminator)

//Title TPL data
#include "title_tpl.h"
#include "title.h"

#define TITLE_BANNER_WIDTH 432
#define TITLE_BANNER_HEIGHT 72

static TPLFile titleTPL;
static GXTexObj titleTexture;
static unsigned int pressStartBlinkCounter;

static struct MenuItem mainMenuItems[] = {
    {"Start Game"},
    {"Exit to Homebrew Channel"},
};

static struct Menu mainMenu = {
    "Main Menu",
    mainMenuItems,
    ARRAY_LENGTH(mainMenuItems),
};

static char saveFiles[MAX_SAVE_FILES][SAVENAME_MAX];
static char saveFileLabels[MAX_SAVE_FILES][SAVENAME_MAX + 3];  //+3 is for the 
static struct MenuItem filesMenuItems[MAX_SAVE_FILES + 1];
static int fileNum;

static struct Menu filesMenu = {
    "Select A File",
    filesMenuItems,
    ARRAY_LENGTH(filesMenuItems),
};

static struct MenuItem newgameMenuItems[] = {
    {"Enter Name"},
    {"Enter Seed"},
    {"Start!"},
    {"Back"},
};

static struct Menu newgameMenu = {
    "New Game",
    newgameMenuItems,
    ARRAY_LENGTH(newgameMenuItems),
};

static struct MenuItem startgameMenuItems[] = {
    {"Start!"},
    {"Erase File"},
    {"Back"},
};

static struct Menu startgameMenu = {
    "Start Game",
    startgameMenuItems,
    ARRAY_LENGTH(startgameMenuItems),
};

static struct MenuItem eraseconfirmMenuItems[] = {
    {"Yes"},
    {"No"},
};

static struct Menu eraseconfirmMenu = {
    "Erase this file?",
    eraseconfirmMenuItems,
    ARRAY_LENGTH(eraseconfirmMenuItems),
};

static void main_menu_init(void);
static void files_menu_init(void);
static void newgame_menu_init(void);
static void startgame_menu_init(int file);

static void draw_title_banner(void)
{
    int x = (gDisplayWidth - TITLE_BANNER_WIDTH) / 2;
    int y = 100;
    
    drawing_set_fill_texture(&titleTexture, TITLE_BANNER_WIDTH, TITLE_BANNER_HEIGHT);
    drawing_draw_textured_rect(x, y, TITLE_BANNER_WIDTH, TITLE_BANNER_HEIGHT);
}

//==================================================
// Start Game Menu
//==================================================

static void eraseconfirm_menu_main(void)
{
    switch (menu_process_input())
    {
        case 0:  //Yes
            file_delete(saveFiles[fileNum]);
        case MENU_CANCEL:
        case 1:  //No
            files_menu_init();
    }
}

static void eraseconfirm_menu_draw(void)
{
    menu_draw();
}

static void startgame_menu_main(void)
{
    switch (menu_process_input())
    {
        case 0:  //Start!
            field_init();
            break;
        case 1:  //Erase File
            menu_init(&eraseconfirmMenu);
            set_main_callback(eraseconfirm_menu_main);
            set_draw_callback(eraseconfirm_menu_draw);
            break;
        case MENU_CANCEL:
        case 2:  //Back
            files_menu_init();
            break;
    }
}

static void startgame_menu_draw(void)
{
    menu_draw();
}

static void startgame_menu_init(int file)
{
    fileNum = file;
    menu_init(&startgameMenu);
    set_main_callback(startgame_menu_main);
    set_draw_callback(startgame_menu_draw);
}

//==================================================
// New Game Menu
//==================================================

static char worldName[SAVENAME_MAX];
static char worldSeed[SEED_MAX];
static char nameKeyboardBuffer[SAVENAME_MAX];
static char seedKeyboardBuffer[SEED_MAX];
static bool msgBoxActive;  //Set whenever the "File already exists." box is shown

static bool check_if_already_exists(const char *name)
{
    if (!stricmp(name, nameKeyboardBuffer))
    {
        msgBoxActive = true;
        return false;
    }
    else
    {
        return true;
    }
}

static void name_kb_main(void)
{
    if (gControllerPressedKeys & PAD_BUTTON_START)
        exit(0);
    if (msgBoxActive)
    {
        if (menu_msgbox_process_input())
            msgBoxActive = false;
        return;
    }
    
    switch (keyboard_process_input())
    {
        case KEYBOARD_OK:
            msgBoxActive = false;
            file_enumerate(check_if_already_exists);
            if (msgBoxActive)
            {
                menu_msgbox_init("File already exists.");
            }
            else
            {
                strcpy(worldName, nameKeyboardBuffer);
                newgame_menu_init();
            }
            break;
        case KEYBOARD_CANCEL:
            newgame_menu_init();
            break;
    }
}

static void name_kb_draw(void)
{
    keyboard_draw();
    if (msgBoxActive)
        menu_msgbox_draw();
}

static void newgame_menu_main(void)
{
    if (gControllerPressedKeys & PAD_BUTTON_START)
        exit(0);
    switch (menu_process_input())
    {
        case 0:  //Enter Name
            strcpy(nameKeyboardBuffer, worldName);
            keyboard_init("Enter World Name", nameKeyboardBuffer, ARRAY_LENGTH(nameKeyboardBuffer));
            set_main_callback(name_kb_main);
            set_draw_callback(name_kb_draw);
            break;
        case 1:  //Enter Seed
            break;
        case 2:  //Start!
            file_create(worldName);
            field_init();
            break;
        case MENU_CANCEL:
        case 3:  //Back
            files_menu_init();
            break;
    }
}

static void newgame_menu_draw(void)
{
    menu_draw();
}

static void newgame_menu_init(void)
{
    msgBoxActive = false;
    menu_init(&newgameMenu);
    set_main_callback(newgame_menu_main);
    set_draw_callback(newgame_menu_draw);
}

//==================================================
// Files Menu
//==================================================

static void files_menu_main(void)
{
    int item = menu_process_input();
    
    if (item == MENU_NORESULT)
        return;
    
    if (item == MAX_SAVE_FILES || item == MENU_CANCEL)  //Back
    {
        main_menu_init();
    }
    else
    {
        if (saveFiles[item][0] == '\0')  //This is an empty save file slot
        {
            memset(worldName, '\0', sizeof(worldName));
            memset(worldSeed, '\0', sizeof(worldSeed));
            newgame_menu_init();
        }
        else
        {
            startgame_menu_init(item);
        }
    }
}

static void files_menu_draw(void)
{
    menu_draw();
}

static bool enum_files_callback(const char *name)
{
    if (fileNum == MAX_SAVE_FILES)
    {
        return false;
    }
    else
    {
        assert(strlen(name) < SAVENAME_MAX);
        strcpy(saveFiles[fileNum], name);
        sprintf(saveFileLabels[fileNum], "%i. %s", fileNum + 1, name);
        fileNum++;
        return true;
    }
}

static void files_menu_init(void)
{
    for (int i = 0; i < MAX_SAVE_FILES; i++)
        sprintf(saveFileLabels[i], "asdkjfgh");
    
    //Populate file menu
    fileNum = 0;
    file_enumerate(enum_files_callback);
    //Add empty labels for the remaining slots
    while (fileNum < MAX_SAVE_FILES)
    {
        saveFiles[fileNum][0] = '\0';
        sprintf(saveFileLabels[fileNum], "%i. (New Game)", fileNum + 1);
        fileNum++;
    }
    for (int i = 0; i < MAX_SAVE_FILES; i++)
        filesMenuItems[i].text = saveFileLabels[i];
    filesMenuItems[MAX_SAVE_FILES].text = "Back";
    menu_init(&filesMenu);
    set_main_callback(files_menu_main);
    set_draw_callback(files_menu_draw);
}

//==================================================
// Main Menu
//==================================================

static void main_menu_main(void)
{
    if (gControllerPressedKeys & PAD_BUTTON_B)
    {
        title_screen_init();
    }
    else
    {
        switch (menu_process_input())
        {
            case 0:  //Start Game
                files_menu_init();
                break;
            case 1:  //Exit to Homebrew Channel
                exit(0);
                break;
        }
    }
}

static void main_menu_draw(void)
{
    draw_title_banner();
    menu_draw();
}

static void main_menu_init(void)
{
    menu_init(&mainMenu);
    set_main_callback(main_menu_main);
    set_draw_callback(main_menu_draw);
}

//==================================================
// Title Screen
//==================================================

static void title_screen_main(void)
{
    if (gControllerPressedKeys & PAD_BUTTON_START)
        main_menu_init();
}

static void title_screen_draw(void)
{
    draw_title_banner();
    if (!(pressStartBlinkCounter & 0x20))
    {
        text_init();
        text_draw_string(gDisplayWidth / 2, 300, true, "Press Start");
    }
    pressStartBlinkCounter++;
}

void title_screen_init(void)
{
    drawing_set_2d_mode();
    text_set_font_size(16, 32);
    set_main_callback(title_screen_main);
    set_draw_callback(title_screen_draw);
    pressStartBlinkCounter = 0;
}

void title_screen_load_textures(void)
{
    TPL_OpenTPLFromMemory(&titleTPL, (void *)title_tpl, title_tpl_size);
    TPL_GetTexture(&titleTPL, titleTextureId, &titleTexture);
    GX_InitTexObjFilterMode(&titleTexture, GX_NEAR, GX_NEAR);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_InvalidateTexAll();
}
