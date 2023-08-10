# Reaction Time Tester
This program enables users to measure their reaction times. Users can customize the program's functionality using an included configuration file.

### How it Works
1. Ready State: The user waits for a prompt, visually indicated by the screen's color change.
2. React State: Upon prompt, the user presses an alphanumeric key or clicks the mouse to record their reaction time.
3. Result State: The program displays the reaction time and an average of previous results.
4. Early State: If the user reacts before the "React" prompt, this state indicates an early reaction (i.e., a failure).

### Setup and Requirements
1. Unzip the release folder wherever you want the program to be installed.
2. Ensure that default.cfg is in the same directory as the program's executable for proper functioning.
   - On the first launch, the program duplicates the contents of default.cfg into reaction.cfg. Users can modify reaction.cfg to customize settings. However, avoid altering default.cfg. If you need to reset reaction.cfg to default settings, delete it.
3. The program currently supports only Windows and has only been test on Windows 11 Systems. Linux users may be able to achieve full functionality through compatibility layers like WINE.

# Background Info
This is a program I made while testing out the capabilities of GPT4 (via ChatGPT). It took a lot of verbose prompting, manual adjustments, and lots of diagnostics to get it into this state, but I am happy with the results.
