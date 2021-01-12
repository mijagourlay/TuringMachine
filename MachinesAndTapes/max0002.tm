# max0002.tm: table 00794329694501 written from tm.c version Mar 13 1997

charset_max 1

state 0
input 0 write 0 move R next 2
input 1 write 0 move L next 3

state 1
input 0 write 1 move S next 4
input 1 write 1 move L next 2

state 2
input 0 write 1 move L next 1
input 1 write 0 move R next 1

state 3
input 0 write 0 move L next 0
input 1 write 0 move L next 0

state 4
input 0 write 0 move L next 0
input 1 write 1 move L next 0
