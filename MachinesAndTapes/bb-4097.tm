# bb-4097.tm: Busy Beaver, 4097 1's, 24487 tape frames, 47,176,870 shifts
# run time: 39 seconds on 133 MHz Pentium, Linux 2.0.0
# Note: The output would have 4098 1's if the "Stop" wrote a 1.
charset_max 1

state 0 #
input 0 write 1 move L next 1
input 1 write 1 move R next 2

state 1 #
input 0 write 1 move L next 2
input 1 write 1 move L next 1

state 2 #
input 0 write 1 move L next 3
input 1 write 0 move R next 4

state 3 #
input 0 write 1 move R next 0
input 1 write 1 move R next 3

state 4 #
input 0 Stop
input 1 write 0 move R next 0
