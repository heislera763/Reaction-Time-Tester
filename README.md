# Reaction Time Tester
This program allows users to measure their reaction times. Using an included configuration file, users can customize many of the program's functionalities. Key features include adjustable timing, raw input support, and a lightweight design.

### How it Works
1. Ready State: The user waits for a prompt, visually indicated by the screen's color change.
2. React State: Upon prompt, the user presses an alphanumeric key or clicks the mouse to record their reaction time.
3. Result State: The program displays the reaction time and an average of previous results.
4. Early State: If the user reacts before the "React" prompt, this state indicates an early reaction (i.e., a failure).

### Setup and Requirements
1. Unzip the release folder wherever you want the program to be installed.
2. Ensure that default.cfg is in the same directory as the program's executable for proper functioning.
   - On the first launch, the program duplicates the contents of default.cfg into reaction.cfg. Users can modify reaction.cfg to customize settings. However, avoid altering default.cfg. If you need to reset reaction.cfg to default settings, delete it.
3. The program currently supports only Windows and has only been tested on Windows 11 Systems. Linux users may be able to achieve full functionality through compatibility layers like WINE.

# Background Info
Most of my programming background is in (relatively simple) embedded systems, and I've never created a win32 application before. So, I decided to use GPT-4 (via ChatGPT) to assist with the development. While it handled most of the heavy lifting, I've made the overarching design choices as well as some manual tweaks of my own. As development has gone on I have been taking on more of the actual coding work, while using GPT-4 to deal with menial tasks.
