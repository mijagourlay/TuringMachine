# stop1.tm: move back and forth, searching for a 1, then stop
charset_max 1

state 0 # write initial marker, left side
input 0 write 1 move R next 1
input 1 write 1 move S next 0

state 1 # write initial marker, right side
input 0 write 1 move L next 2
input 1 write 1 move S next 1

state 2 # leftbound: move left until (my marker) 1, erase my marker
input 0 write 0 move L next 2
input 1 write 0 move L next 3

state 3 # write (my marker) 1 -> go righbound
input 0 write 1 move R next 4
input 1 write 1 move S next 3

state 4 # rightbound: move right until (my marker) 1, erase my marker
input 0 write 0 move R next 4
input 1 write 0 move R next 5

state 5 # write (my marker) 1 -> go leftbound
input 0 write 1 move L next 2
input 1 write 1 move S next 5
