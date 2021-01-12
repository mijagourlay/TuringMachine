# bb-4098.tm: Busy Beaver, 4098 1's, 24487 tape frames, 47,176,870 shifts
# run time: 39 seconds on 133 MHz Pentium, Linux 2.0.0
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
input 0 write 1 move S next 4
input 1 write 0 move R next 0
