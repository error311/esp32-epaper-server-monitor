#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include <stdlib.h>

void setup()
{
    printf("EPD_2IN9B_V4_demo\r\n");
    if(DEV_Module_Init() != 0){
        while(1);
    }
    
    // ---- Full refresh: draw static background -------------------
    printf("Full refresh background...\r\n");
    EPD_2IN9B_V4_Init();
    EPD_2IN9B_V4_Clear();
    DEV_Delay_ms(500);
    
    // Allocate full-screen buffers for black and red images
    UBYTE *BlackImage, *RYImage;
    UWORD Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        while(1);
    }
    if((RYImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for red memory...\r\n");
        while(1);
    }
    printf("Allocating full-screen buffers...\r\n");
    
    // Create full-screen image buffers with 270° rotation.
    Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    
    // Draw static background (e.g., "EPD Test" text) on BlackImage.
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    Paint_DrawString_EN(10, 65, "EPD Test", &Font16, BLACK, WHITE);
    // For RYImage, simply clear to white.
    Paint_SelectImage(RYImage);
    Paint_Clear(WHITE);
    
    // Perform full refresh using the base function.
    EPD_2IN9B_V4_Init();
    EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
    DEV_Delay_ms(2000);
    
    
    // ---- Partial update: redraw full screen (so static text is preserved) plus dynamic dot ----
    printf("Performing full-screen partial update...\r\n");
    // Create a full-screen partial image buffer.
    Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    
    // Re-draw the same static background.
    Paint_DrawString_EN(10, 65, "EPD Test", &Font16, BLACK, WHITE);
    
    // Now overlay the dynamic dot.
    // With 270° rotation, the mapping is:
    //    physical_x = image_y
    //    physical_y = (EPD_2IN9B_V4_WIDTH - image_x)
    // We want the dot at physical (10,10). Since our dest is (0,0),
    // we require: image_y = 10 and (296 - image_x) = 10, so image_x = 286.
    Paint_DrawString_EN(286, 10, ".", &Font12, BLACK, WHITE);
    
    // Use the partial update function over the entire screen.
    EPD_2IN9B_V4_Display_Partial(BlackImage, 0, 0, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT);
    DEV_Delay_ms(2000);
    
    // Clean up and put the display to sleep.
    printf("Clearing and sleeping...\r\n");
    EPD_2IN9B_V4_Init();
    EPD_2IN9B_V4_Clear();
    EPD_2IN9B_V4_Sleep();
    free(BlackImage);
    free(RYImage);
    BlackImage = NULL;
    RYImage = NULL;
    DEV_Delay_ms(2000);
}

void loop()
{
    // Nothing to do in loop.
}

