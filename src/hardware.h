/*
           ____  _____ __  ____________  ____  ___  ____________
          / __ \/ ___// / / / ____/ __ \/ __ \/   |/_  __/_  __/
         / / / /\__ \/ /_/ / __/ / /_/ / /_/ / /| | / /   / /   
        / /_/ /___/ / __  / /___/ _, _/ _, _/ ___ |/ /   / /    
        \____//____/_/ /_/_____/_/ |_/_/ |_/_/  |_/_/   /_/             
                                   
*/                                                                     
/* This document contains all of the user adjustable variables for the   */ 
/* system hardware                                                       */


/*-----------------------------------------------------------------------*/
// HARDWARE SETTINGS                                                     //
/* Change background task intervals and other settings                   */
/*-----------------------------------------------------------------------*/
#define debounceDelay 25        // -- Time to debounce buttons
#define OLED_RESET 0            // -- OLED reset pin (check the documentation for your hardware)
#define inputChange_Pin D0      // -- Source encdoer switch input pin
#define mute_Pin D5             // -- Volume encoder switch input pin
#define volumeEncoderCLK_Pin D3 // -- Volume encoder CLK pin
#define volumeEncoderDT_Pin D4  // -- Volume encoder DT pin
#define sourceEncoderCLK_Pin D6 // -- Source encoder CLK pin
#define sourceEncoderDT_Pin D7  // -- source ender DT pin
#define buttonPin A0            // -- Resistor ladder button input pin
