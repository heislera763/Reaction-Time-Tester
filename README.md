# Reaction Time Tester:

This is a basic program allowing users to measure their reaction times. A basic configuration file is included to let users easily change some functionality.

The program works like so: It starts in the inital "Ready" state, in which the user waits for a change. After some time the screen will switch to the "React" state, which is when the user will either press an alphabet key or click their mouse to signify their reaction. After this is the "Result" state, which will display the time to react, as well as an average of the previous reactions. Also, if the user reacts before the "React" state, they will trigger the "Early" state, which signifies a failure.

### The following can currently be configured in the .cfg file:
 - "Ready", "React", "Result", and "Early" screen colors
 - Delay
 - Number of trials to average

# Background Info:

This is a program I made while testing out the capabilities of GPT4 (via ChatGPT). It took a lot of verbose prompting, manual adjustments, and lots of diagnostics to get it into this state, but I am happy with the results. Please note that the program doesn't work correctly without reaction.cfg in the same folder as the executable.

From my experimentation on a Windows 11 system it seems pretty robust in its current state, but I am not able to test on other systems easily at this time. As far as linux support goes I imagine Wine or a similar software could handle this very easily.

The final thing I would like to note is that the most painful part of doing all this was realizing that gpt was slightly misformatting some stuff and I had to drudge through MS documentation to figure out how to fix it.
