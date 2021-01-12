# bb-1915.tm: Busy Beaver, 1915 1's, 7642 tape frames, 2,133,492 steps
charset_max 1

state 0 #
input 0 write 1 move R next 1
input 1 write 1 move L next 2

state 1 #
input 0 write 0 move L next 0
input 1 write 0 move L next 3

state 2 #
input 0 write 1 move L next 0
input 1 write 1 move S next 2

state 3 #
input 0 write 1 move L next 1
input 1 write 1 move R next 4

state 4 #
input 0 write 0 move R next 3
input 1 write 0 move R next 1
