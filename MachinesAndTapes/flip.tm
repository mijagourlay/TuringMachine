#flip.tm: Turing machine that flip bits to right of initial position
charset_max 1
state
input 0 write 1 move R next 0
input 1 write 0 move R next 0
