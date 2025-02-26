#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include <stdlib.h>

#define ITERATIONS 20

// Global pointer for our full-screen partial update buffer.
UBYTE *partialBuffer = NULL;

void setup() {
  Serial.begin(115200);
  if (DEV_Module_Init() != 0) {
    while (1);
  }

  // ---- Full refresh: draw static background once -----------
  Serial.println("Performing full refresh...");
  EPD_2IN9B_V4_Init();
  EPD_2IN9B_V4_Clear();
  DEV_Delay_ms(500);

  // Allocate full-screen buffers for base refresh.
  UWORD Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? 
                     (EPD_2IN9B_V4_WIDTH / 8) : 
                     (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  UBYTE *BlackImage = (UBYTE *)malloc(Imagesize);
  UBYTE *RYImage = (UBYTE *)malloc(Imagesize);
  if (BlackImage == NULL || RYImage == NULL) {
    Serial.println("Memory allocation failed for base buffers");
    while (1);
  }

  // Create full-screen image buffers with 270° rotation.
  Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);

  // Draw static server texts on BlackImage (base refresh).
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  // Base refresh: draw servers at x = 10.
  Paint_DrawString_EN(10, 20, "Server0", &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 35, "Server1", &Font16, BLACK, WHITE);

  // Clear the red image.
  Paint_SelectImage(RYImage);
  Paint_Clear(WHITE);

  // Display full-screen background using base update.
  EPD_2IN9B_V4_Init();
  EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  DEV_Delay_ms(3000);

  // Allocate the global partial update buffer.
  partialBuffer = (UBYTE *)malloc(Imagesize);
  if (partialBuffer == NULL) {
    Serial.println("Failed to allocate partial update buffer");
    while (1);
  }

  free(BlackImage);
  free(RYImage);
}

void loop() {
  // We'll simulate selection cycling using a for-loop.
  // Instead of updating the selection every iteration, we update it every 3 iterations,
  // so the dot stays on one row for a couple of cycles.
  static bool dotState = false;
  static int selectedRow = 0; // 0 for Server0, 1 for Server1
  
  for (int i = 0; i < ITERATIONS; i++) {
    // Create a new full-screen image in the partial update buffer.
    Paint_NewImage(partialBuffer, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    Paint_SelectImage(partialBuffer);
    Paint_Clear(WHITE);
    
    // Re-draw static server texts (drawn at x = 10).
    Paint_DrawString_EN(10, 20, "Server0", &Font16, BLACK, WHITE);
    Paint_DrawString_EN(10, 35, "Server1", &Font16, BLACK, WHITE);
    
    // Draw the selector dot on the selected row.
    // Draw the dot at x = 2 so it appears to the left of the text.
    if (dotState) {
      if (selectedRow == 0)
        Paint_DrawString_EN(2, 20, ".", &Font12, BLACK, WHITE);
      else
        Paint_DrawString_EN(2, 35, ".", &Font12, BLACK, WHITE);
    }
    
    // Apply full-screen partial update.
    EPD_2IN9B_V4_Display_Partial(partialBuffer, 0, 0, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT);
    Serial.print("Iteration ");
    Serial.print(i);
    Serial.print(": Selected row = ");
    Serial.println(selectedRow);
    
    dotState = !dotState;  // Toggle blinking.
    
    // Every 3 iterations, simulate a button press that moves the selection.
    if (i % 3 == 2) {
      selectedRow = (selectedRow + 1) % 2;
    }
    
    DEV_Delay_ms(1000);
  }
  
  Serial.println("Selection cycle complete. Going to sleep.");
  delay(500);
  esp_deep_sleep_start();
}
