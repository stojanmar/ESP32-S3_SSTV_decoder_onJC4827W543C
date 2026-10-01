# ESP32-S3_SSTV_decoder_onJC4827W543C
Standalone SSTV receiver-decoder using ESP32-S3 4.3" display JC4827W543C. Decodes many popular SSTV modes.
This is another project related to SSTV signals reception.
I demonstrate here, how can you use unexpensive display, which integrates ESP32-S3 microcontroller to receive, decode and display images. Only one mini microphone board
needs to be connected to display and you can receive images transmitted in real time.
To simplify my work I have used sstv library published couple of years ago by Jonathan P Dawson. He did a hard work on using Raspbery Py Pico and kindly sheared his work on Github.
I managed to operate same library for decoding on ESP32 architecture. I also added a display touch functionality, so the image reception can be stopped or started manualy. 
The audio signal from radio over the microphone is analog. I had to introduce my own, very fast analog read and double sample buffering to maintain necessary processing speed.
If you dont have an shortwave radio, you can alternativelly use one of the online web radios

Verified in Arduino with ESP32 SDK core 2.0.17.
Hardware:
Beside display you need MAX4466 Microphone Amplifier Module. Connect its output to pin 6 on exposed display connector.
Also provide 3v3 and GND connection between two boards.
<img width="400" height="306" alt="display" src="https://github.com/user-attachments/assets/dd563701-1df9-4ada-9bb0-bfa89c08b114" />
<img width="120" height="120" alt="image" src="https://github.com/user-attachments/assets/e6935a5d-724e-49c8-a16e-53bcd55ff394" />

Link to my video on Youtube: https://www.youtube.com/watch?v=3JQ2a8_2Shg

My thanks here to Jonathan P Dawson and link to his page: https://101-things.readthedocs.io/en/latest/sstv_decoder.html

If you like, you can buy me a coffee: https://ko-fi.com/stojanm
Some decoded images:

<img width="403" height="226" alt="20260922_225537" src="https://github.com/user-attachments/assets/d7291f20-3ad9-4235-a226-c98a0fecfad4" />
<img width="403" height="226" alt="20260915_120207" src="https://github.com/user-attachments/assets/5bf41b90-54bd-4b88-a73b-1e566c864edf" />









