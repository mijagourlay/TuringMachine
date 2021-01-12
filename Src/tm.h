/* tm.h: Turing Machine simulator
//
// Written and Copyright (C) 1997 by Michael J. Gourlay
//
// Provided as is.  No warrentees, express or implied.
*/




/* Using "int" for Char seems to run faster, but takes more memory
// than "short" or "signed char".
*/
typedef signed char Char;
typedef enum {MOVE_LEFT, MOVE_RIGHT, STOP} Move;
typedef int State;




typedef struct {
  Char write;  /* character to write at this place on tape */
  Move move;   /* which way to move tape head */
  State next;  /* which state to enter next */
} Entry;




typedef struct {
  Char charset_max; /* Largest allowable character in the charset.
                    // The charset is the list of valid inputs and
                    // outputs.  The range of valid characters is from 0
                    // to charset_max.  The special value -1 means
                    // "stop".
                    */

  State state;       /* current state */

  Entry **table;    /* State machine table:
                    // indexed as table[state][input]
                    //   where state is the current state
                    //   where input is the current input character
                    //
                    // The number of elements of a table line is
                    //   line_length = (charset_max+1)
                    // The number of elements of a table is
                    //   line_length * num_states;
                    */

  int32_t num_states;  /* number of states of this machine */

  int64_t here;        /* current tape head position */

  int64_t tape_len;    /* Length of tape accessed so far */

  Char *tape;       /* data tape */
} TuringMachineT;


typedef TuringMachineT* TuringMachine;
