SET(SOURCES ui_Start_Mode.c
    ui_Driver_Mode.c
    ui_Engineer_Mode.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    ui_img_mmr_black_small_png.c
    fonts/ui_font_GEAR_FONT.c)

add_library(ui ${SOURCES})
