# bb-136612.tm: Busy Beaver, 136612 1's, 409749 frames, 13,122,572,797 shifts
# run time: 10949 seconds (3 hours) on 133MHz Pentium, Linux 2.0.0
charset_max 1

state 0 # 1b->21L 11->11L
input 0 write 1 move L next 1
input 1 write 1 move L next 0

state 1 # 2b->31R 21->21R
input 0 write 1 move R next 2
input 1 write 1 move R next 1

state 2 # 3b->6bR 31->41R
input 0 write 0 move R next 5
input 1 write 1 move R next 3

state 3 # 4b->11L 41->5bR
input 0 write 1 move L next 0
input 1 write 0 move R next 4

state 4 # 5b->1bL 51->31R
input 0 write 0 move L next 0
input 1 write 1 move R next 2

state 5 # 6b->51L 61->S
input 0 write 1 move L next 4
input 1 write 1 move S next 5
