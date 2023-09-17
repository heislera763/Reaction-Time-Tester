# Reaction Time Tester
This program allows users to measure their reaction times. Using the configuration file (user.cfg), users can customize much of the program's behavior. Key features include adjustable timing, raw input support, and a lightweight design.

### Setup and Requirements
1. Unzip the release folder wherever you want the program to be installed.
2. Ensure that default.cfg is in the /config/ folder.
   - On the first launch, the program duplicates the contents of default.cfg into user.cfg. Users can modify user.cfg to customize settings. However, do not alter or delete default.cfg. If you need to reset user.cfg to default settings, delete it.
3. The program currently supports only Windows and has only been tested on a Windows 11 System. Linux users may be able to achieve full functionality through compatibility layers like WINE.

### How it Works
1. Ready State: The user waits for a color change.
2. React State: Upon color change, the user presses any alphanumeric key or clicks their mouse to record their reaction time.
3. Result State: The program displays the reaction time and an average of previous results (Minimum numbers of results must be collected for an average to appear).
4. Early State: If the user reacts before the "React" screen appears, this is considered an early reaction (i.e. a failure).

### Background Info
Most of my programming background is in some simple terminal stuff and embedded systems applications, and I've never created a win32 application before. I decided to use GPT-4 to help with a lot of the annoying parts of this project (primarily dealing with weird Microsoft/Windows stuff), while I made the overarching design choices. As development has gone on I have taken on all of the programming work, while occasionally using GPT-4 to deal with menial tasks and organization.

An interesting aspect of generating large chunks of the initial codebase was that in the early stages it sometimes felt like I was working with someone else on the project. However, the technical debt that GPT ended up creating meant that I needed to put forth a suprising amount of effort to unmangle the code. Because of this, the project has unintentionally become a great educational experience with regards to working around poor initial design decisions in a codebase.

### Issue with Windows Smartscreen
Unfortunately, since my release binaries are rarely seen by Windows Smartscreen, it will typically flag them upon attempting to run the executable. If you are not comfortable running my release binaries I invite you to compile it yourself. If you have VS Code and UCRT64 (or maybe another Windows targetting C compiler) it should be fairly straightforward to compile a build identical to my releases.
