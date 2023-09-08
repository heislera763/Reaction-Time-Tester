# Reaction Time Tester
This program allows users to measure their reaction times. Using an included configuration file, users can customize many of the program's functionalities. Key features include adjustable timing, raw input support, and a lightweight design.

### Setup and Requirements
1. Unzip the release folder wherever you want the program to be installed.
2. Ensure that default.cfg is in the /config/ folder.
   - On the first launch, the program duplicates the contents of default.cfg into reaction.cfg. Users can modify reaction.cfg to customize settings. However, do not alter or delete default.cfg. If you need to reset reaction.cfg to default settings, delete it.
3. The program currently supports only Windows and has only been tested on a Windows 11 System. Linux users may be able to achieve full functionality through compatibility layers like WINE.

### How it Works
1. Ready State: The user waits for a prompt, visually indicated by the screen's color change.
2. React State: Upon prompt, the user presses an alphanumeric key or clicks the mouse to record their reaction time.
3. Result State: The program displays the reaction time and an average of previous results.
4. Early State: If the user reacts before the "React" prompt, this state indicates an early reaction (i.e., a failure).

### Background Info
Most of my programming background is in some simple terminal stuff and embedded systems applications, and I've never created a win32 application before. So, I decided to use GPT-4 (via ChatGPT) to assist with the initial development. While it handled much of the heavy lifting (mostly dealing with weird Microsoft stuff and also making many functions), I've made all the overarching design choices as well as many additions of my own. As development has gone on I have been taking on more of the actual coding work, while using GPT-4 to deal with menial tasks and minor adjustments.

### Issue with Windows Smartscreen
Unfortunately, since my release binaries are rarely seen by Windows Smartscreen, it will typically flag them upon attempting to run the executable. If you are not comfortable running my release binaries I invite you to compile it yourself, if you have Visual Studio the project file including in the repo should be preconfigured to build the same release binaries that I distribute.
