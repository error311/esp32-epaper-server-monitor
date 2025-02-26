#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include <stdlib.h>

// Global pointer for our full-screen partial update buffer.
UBYTE *partialBuffer = NULL;

void setup() {
    printf("EPD_2IN9B_V4_demo\r\n");
    if(DEV_Module_Init() != 0){
        while (1);
    }
    
    // ---- Full refresh: draw static background -----------
    printf("Full refresh background...\r\n");
    EPD_2IN9B_V4_Init();
    EPD_2IN9B_V4_Clear();
    DEV_Delay_ms(500);
    
    // Allocate full-screen buffers for black and red images.
    UBYTE *BlackImage, *RYImage;
    UWORD Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ?
                       (EPD_2IN9B_V4_WIDTH / 8) :
                       (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to allocate black memory...\r\n");
        while(1);
    }
    if((RYImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to allocate red memory...\r\n");
        while(1);
    }
    printf("Allocated full-screen buffers...\r\n");
    
    // Create full-screen image buffers with 270° rotation.
    Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    
    // Draw static text on BlackImage.
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    Paint_DrawString_EN(10, 20, "Server0", &Font16, BLACK, WHITE);
    Paint_DrawString_EN(10, 35, "Server1", &Font16, BLACK, WHITE);
    
    // Clear the red image.
    Paint_SelectImage(RYImage);
    Paint_Clear(WHITE);
    
    // Display full-screen background using base update.
    EPD_2IN9B_V4_Init();
    EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
    DEV_Delay_ms(3000);
    
    // ---- Allocate full-screen partial update buffer for blinking update ----
    partialBuffer = (UBYTE *)malloc(Imagesize);
    if(partialBuffer == NULL) {
        printf("Failed to allocate partial update buffer...\r\n");
        while(1);
    }
    
    // Free the base refresh buffers (they're no longer needed).
    free(BlackImage);
    free(RYImage);
}

void loop() {
    static bool dotState = false;
    
    // Create a new full-screen image in the partial update buffer.
    Paint_NewImage(partialBuffer, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    Paint_SelectImage(partialBuffer);
    Paint_Clear(WHITE);
    
    // Re-draw static text in the same positions.
    Paint_DrawString_EN(10, 20, "Server0", &Font16, BLACK, WHITE);
    Paint_DrawString_EN(10, 35, "Server1", &Font16, BLACK, WHITE);
    
    // If dotState is true, draw blinking dots at x=120 on the same rows.
    if(dotState) {
        Paint_DrawString_EN(120, 20, ".", &Font12, BLACK, WHITE);
        Paint_DrawString_EN(120, 35, ".", &Font12, BLACK, WHITE);
    }
    
    // Update the entire screen using partial update.
    // (We use the full screen window so that the mapping remains identical.)
    EPD_2IN9B_V4_Display_Partial(partialBuffer, 0, 0, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT);
    
    dotState = !dotState;
    DEV_Delay_ms(1000);  // 1-second blink period
}


