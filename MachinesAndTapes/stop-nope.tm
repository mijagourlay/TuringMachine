# stop-nope.tm: Table with unreachable stop
charset_max 1

state 0 # 1b->21R  11->3bL
input 0 write 0 move L next 0
input 1 write 0 move L next 1

state 1 # 2b->31R  21->41R
input 0 write 1 move R next 2
input 1 write 1 move R next 2

state 2 # 3b->11L  31->2bR
input 0 write 1 move L next 3 
input 1 write 0 move R next 3

state 3 # 5b->31L  51->11R
input 0 write 1 move L next 3
input 1 write 1 move R next 3

state 4 # 4b->5bR  41->stop
input 0 write 0 move R next 0
input 1 write 1 move S next 0
