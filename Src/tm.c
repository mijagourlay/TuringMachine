/* tm.c: Turing Machine simulator
//
// Written and Copyright (C) 1997 by Michael J. Gourlay
//
// Provided as is.  No warrentees, express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <alloca.h>

#ifndef sun
#include <getopt.h>
#endif

#include <curses.h>
#include <limits.h>
#include <signal.h>

#include "fifo.h"

#include "tm.h"




/* MAX: return larger of the two input numbers */
#define MAX(a,b) (((a)>(b))?(a):(b))

#define ABS(a)   (((a)<0)?(-(a)):(a))




/* tmNew: allocate and initialize a new Turing Machine
//
// DESCRIPTION
//   Allocates a new Turing Machine object.  tmNew should always be
//   used by the client to instantiate a new Turing Machine object.
//   The client should not allocate space for a Turing Machine without
//   using tmNew, since the other Turing Machine methods depend on the
//   initializations done in tmNew.
//
// RETURN VALUE
//   Return the address of the new Turing Machine object.
//   Returns NULL if memory could not be allocated.
*/
TuringMachineT *
tmNew(void)
{
  TuringMachineT *tm;
  if((tm = (TuringMachineT*) calloc(1, sizeof(TuringMachineT)))==NULL)
  {
    return NULL;
  }

  tm->charset_max = -1;
  tm->state       = 0;
  tm->table       = NULL;
  tm->num_states  = 0;
  tm->here        = 0;
  tm->tape_len    = 0;
  tm->tape        = NULL;

  return tm;
}




/* NAME
//   tmTapeFree: free memory of a Turing Machine tape
*/
void
tmTapeFree(TuringMachineT *this)
{
  if(this->tape != NULL) {
    free(this->tape);
    this->tape = NULL;
  }
  this->tape_len = 0;
  this->here     = 0;
}




/* NAME
//   tmTableRead: read a Turing Machine state transition table from a file
//
//
// Table file format: Each line can either be one of the following:
//   charset_max <integer>
//     indicates the charset_max for this TM.
//     'charset_max' must occur before any 'state' lines.
//   state
//     indicates that table entries for a new state are to be read.
//     'state' must occur before any 'input' lines.
//   input <integer> write <integer> move <L|R|S> next <integer>
//     indicates table entry for the current (state,input) tuple.
//   #
//     comment line
//   blank line
//
// Any table that has a "next state" entry which refers to a non-existent
// state is not a valid table.
//
//
// RETURN VALUE
//   Returns a negative value if there is an error.
//   Returns a positive value if the table was read without error.
*/
int64_t
tmTableRead(TuringMachineT *this, char *filename)
{
  int32_t charset_max   = -1;
  int32_t state         = -1; /* current state being read */
  int32_t si;                 /* state index for loop */
  int32_t input;              /* input character for this table entry */
  int32_t write;              /* character to write for this (state, input) */
  int32_t next;               /* next state for this (state, input) */
  char move;               /* direction to move head for this (state, input) */
  int32_t line_num       = 0; /* input line number, for debugging */
  char line[1024];         /* file line buffer */
  FILE *stream;            /* file stream */
  int32_t max_refd_state = 0; /* number of the highest refered to state */
  Entry *table     = NULL; /* table memory place */
  Entry **tablePP  = NULL; /* table array of pointers */

  if((stream=fopen(filename, "r"))==NULL) {
    fprintf(stderr, "tmTableRead: error opening '%s'\n", filename);
    return -1;
  }

  while(fgets(line, 1023, stream)) {
    line[strlen(line)-1] = '\0'; /* remove trailing newline */

    line_num++;

    if(sscanf(line, "charset_max %i", &charset_max)==1) {
      if(this->charset_max >= 0) {
        fprintf(stderr,
                "tmTableRead: %i: encountered more than one 'charset_max'\n",
                line_num);
        fprintf(stderr,
                "tmTableRead: previous value charset_max=%i\n",
                charset_max);
        fclose(stream);
        return -2;
      } else {
        this->charset_max = charset_max;  /* type conversion */
      }

    } else if(!strncmp(line, "state", 5)) {
      if(charset_max < 0) {
        fprintf(stderr,
                "tmTableRead: %i: must have 'charset_max' before 'state'\n",
                line_num);
        fclose(stream);
        return -3;
      }

      state ++;
      this->num_states = state + 1;

      /* Allocate a new state line for the TM */
      if((table = realloc(table,
                          sizeof(Entry) * (charset_max + 1) * this->num_states)
         )==NULL)
      {
        fprintf(stderr, "tmTableRead: out of memory\n");
        fclose(stream);
        return -4;
      }
      if((tablePP = realloc(tablePP, sizeof(Entry*) * this->num_states))==NULL)
      {
        fprintf(stderr, "tmTableRead: out of memory\n");
        fclose(stream);
        return -4;
      }

      for(si=0; si < this->num_states; si++) {
        tablePP[si] = &table[si * (charset_max+1)];
      }

      this->table = tablePP;

    } else if(sscanf(line, "input %i write %i move %c next %i",
              &input, &write, &move, &next) == 4)
    {
      if(state < 0) {
        fprintf(stderr,
                "tmTableRead: %i: must have 'state' before 'input'\n",
                line_num);
        fclose(stream);
        return -5;
      }

      if((input < 0) || (input > charset_max)) {
        fprintf(stderr, "tmTableRead: %i: bad value for input: %i\n",
                line_num, input);
        fclose(stream);
        return -6;
      }

      /* Store table entry */
      if((write < 0) || (write > charset_max)) {
        fprintf(stderr, "tmTableRead: %i: bad value for write: %i\n",
                line_num, write);
        fclose(stream);
        return -7;
      }
      this->table[state][input].write = write;  /* type conversion */

      if(next < 0) {
        fprintf(stderr, "tmTableRead: %i: bad value for next: %i\n",
                line_num, next);
        fclose(stream);
        return -8;
      }
      this->table[state][input].next = next;  /* type conversion */
      max_refd_state = MAX(max_refd_state, next);

      if(move == 'L') {
        this->table[state][input].move = MOVE_LEFT;
      } else if(move == 'R') {
        this->table[state][input].move = MOVE_RIGHT;
      } else if(move == 'S') {
        this->table[state][input].move = STOP;
      } else {
        fprintf(stderr,
          "tmTableRead: %i: bad value for move: %c\n", line_num, move);
        fclose(stream);
        return -9;
      }

    } else if(line[0]=='#') {
      /* comment line. */
      printf("%s\n", line);

    } else if(line[0] == '\0') {
      /* blank line. Do nothing */

    } else {
      fprintf(stderr, "tmTableRead: %i: invalid line: '%s'\n",
              line_num, line);
      fclose(stream);
      return -11;
    }
  }

  fclose(stream);

  if(max_refd_state > state) {
    fprintf(stderr, "tmTableRead: refered to non-existent state %i\n",
            max_refd_state);
    fclose(stream);
    return -12;
  }

  return state;
}




/* NAME
//   tmTapeIndex: translates the signed value of "here" into an index for tape[]
//
//
// DESCRIPTION
//   if here >= 0, tapeIndex(here) = here * 2
//   if here < 0 , tapeIndex(here) = - (here * 2 + 1)
//
//   The tape index has the property that negative values of here map to
//   odd values of the index, and non-negative values of here map
//   to even values of the index, such that no elements of tape[] are
//   wasted.
//
//
// RETURN VALUE
//   Returns a positive integer which is the index into this->tape[]
//   which corresponds to the (possibly negative) signed this->here
//   tape head position.
*/
#ifdef FUNCTION
int64_t
tmTapeIndex(const TuringMachineT *this)
{
  if(this->here >= 0) {
    return this->here * 2;
  } else {
    return - (this->here * 2 + 1);
  }
}
#else
#define tmTapeIndex(this) \
  (((this)->here >= 0) ? (((this)->here)*2) : (-(((this)->here)*2+1)))
#endif




#define tmTapeFrame(this) ((this)->tape[tmTapeIndex(this)])




/* NAME
//   tmTapeHead: return Turing Machine tape head location given tape index
//
//
// DESCRIPTION
//   tmTapeHead is the inverse of tmTapeIndex.
//
//   If index is even, here is non-negative.
//   If index is odd, here is negative.
//   index == 0 corresponds to a this->here == 0.
//
//
// SEE ALSO
//   tmTapeIndex
*/
int64_t
tmTapeHead(const TuringMachineT *this, int64_t index)
{
  if(index % 2 == 0) {
    return index / 2;
  } else {
    return - (index+1) / 2;
  }
}




/* NAME
//   tmTableIndex: return lexical index of the Turing machine table
//
//
// DESCRIPTION
//   When systematically generating all possible Turing machine state
//   transition tables, it is useful to order those tables.
//   tmTableIndex takes in a table and generates a unique index for
//   that table.  It is possible to go back and forth between a table
//   and its index.
*/
int64_t
tmTableIndex(const TuringMachineT *this)
{
  int       di;       /* "digit" index */
  int       si;       /* state index */
  int       ii;       /* input index */
  int       dv;       /* "digit" value */
  int64_t ti  = 0;  /* table index */
  int64_t p21 = 1;  /* power of 21 */

  for(si=0; si < this->num_states; si++) {
    for(ii=0; ii <= this->charset_max; ii++) {
      /* Find which digit we're at */
      di = si * this->num_states + ii;

      /* Compute lexical value of this table entry */
      if(this->table[si][ii].move != STOP) {
        dv =   this->table[si][ii].write
             + this->table[si][ii].next * (this->charset_max + 1)
             + this->table[si][ii].move * (this->charset_max + 1)
               * this->num_states;
      } else {
        dv = 20;
      }
      /* Accumulate the table index */
      ti += p21 * dv;

      /* Compute next power of 21 */
      p21 *= 21;
    }
  }
  return ti;
}




/* NAME
//   tmTableWrite: write a Turing machine table in a machine readable form
//
//
// DESCRIPTION
//   tmTableWrite writes a Turing machine table in a way such that it
//   can be read in by tmTableRead.
*/
int
tmTableWrite(const TuringMachineT * const this, const char * const filename)
{
  FILE *stream;
  int si;
  int ii;
  static char *move_chars = "LRS";
  const int64_t ti = tmTableIndex(this);  /* table index */

  /* Open the tape file for writing */
  if((stream=fopen(filename, "w"))==NULL) {
    fprintf(stderr, "tmTableWrite: error opening '%s'\n", filename);
    return -1;
  }

  fprintf(stream, "# %s: table %014lli written from %s version %s\n",
          filename, ti, __FILE__, __DATE__);

  fprintf(stream, "\ncharset_max %i\n", this->charset_max);

  for(si=0; si < this->num_states; si++) {
    fprintf(stream, "\nstate %i\n", si);
    for(ii=0; ii <= this->charset_max; ii++) {
      fprintf(stream, "input %i write %i move %c next %i\n", ii,
             this->table[si][ii].write,
             move_chars[this->table[si][ii].move],
             this->table[si][ii].next);
    }
  }
  fclose(stream);
  return 1;
}




/* tmTablePrint: print Turing Machine state transition table */
#define tmTablePrint(this) tmTableCurse(this, -1, -1)




/* NAME
//   tmTableCurse: use curses to print Turing Machine state transition table
//
//
// ARGUMENTS
//   wy (in): row to print table, where row 0 is the top
//     if wy is negative then print without using curses
//
//   wx (in): column to print table, where colunm 0 is the left
//
//
// DESCRIPTION
//   The left-most column of the state transition table indicates the
//   state.  Each row of the state transition table indicates the
//   instructions to perform for that given state.  Each of the columns
//   to the right of the leftmost column are for a given input from the
//   current tape frame.  The entries in the state transition table
//   have 3 symbols.  The left symbol indicates the value to be written
//   onto the current tape frame, the middle symbol indicates the
//   direction that the tape head will move, and the right symbol
//   indicates the next state to enter.  The entry that corresponds to
//   the current state and tape input value is highlighted.
//
//   If the terminal text window is too small to fit the table, a
//   message is printed which indicates the minimum number of lines the
//   window must have in order to display the table.
//
//
// SEE ALSO
//   tmTablePrint(), tmTapeFrame(), curses library
*/
void
tmTableCurse(const TuringMachineT *this, const int wy, const int wx)
{
  static char *move_chars = "LRS";

  int64_t   si;
  int64_t   ii;
  int    py = wy;
  int  (*print)(const char *fmt,...);

  if(wy > 0) {
    print = printw;
  } else {
    print = printf;
  }

  if(wy >= 0) move(py, wx);
  print("+---------");
  for(ii=0; ii < this->charset_max; ii++) {
    print("-----------");
  }
  print("---------+\n");
  py++;

  if(wy >= 0) move(py,wx);
  print("| state | ");
  for(ii=0; ii <= this->charset_max; ii++) {
    print(" %5li   | ", ii);
  }
  print("\n");
  py++;

  if(wy >= 0) move(py,wx);
  print("+-------+-");
  for(ii=0; ii < this->charset_max; ii++) {
    print("---------+-");
  }
  print("---------|\n");
  py++;

  for(si=0; si < this->num_states; si++) {
    if(wy >= 0) move(py,wx);
    print("| %5li :", si);

    for(ii=0; ii <= this->charset_max; ii++) {
      if((this->state == si) && (tmTapeFrame(this) == ii)) {
        if(wy >= 0) attron(A_STANDOUT);
        print(">");
      } else {
        print(" ");
      }

      print("%2li ", this->table[si][ii].write);
      print("%2c ", move_chars[this->table[si][ii].move]);
      print("%2li", this->table[si][ii].next);

      if((this->state == si) && (tmTapeFrame(this) == ii)) {
        print("<");
        if(wy >= 0) attroff(A_STANDOUT);
      } else {
        print(" ");
      }

      print("|");
    }
    print("\n");
    py++;

    if(wy >= 0) move(py,wx);
    print("+-------+-");
    for(ii=0; ii < this->charset_max; ii++) {
      print("---------+-");
    }
    print("---------|\n");
    py++;
  }

  /* Get size of the curses window */
  if(wy >= 0) {
    int min_x, min_y;
    int max_x, max_y;
    int size_y;

    getbegyx(stdscr, min_y, min_x);
    getmaxyx(stdscr, max_y, max_x);
    size_y = max_y - min_y - py;
    if(size_y < 0) {
      move(wy,wx);
      clrtobot();
      move(wy,wx);
      printw("\n\nWindow too short for table.  ");
      printw("Need at least %i lines for this table.\n\n", py);
    }
  }

  if(wy >= 0) refresh();
}




/* NAME
//   tmTapeAlloc: Allocate more memory for tape, if needed
//
//
// DESCRIPTION
//   The tape of an abstract Turing Machine is infinitely long.  In
//   this simulation, the tape is finite, obviously, but the tape can
//   "grow" dynamically.  Any time a portion of the tape is accessed
//   which was not previously represented (i.e., was not previously
//   allocated), more tape is allocated.  tmTapeAlloc() should be
//   called every time the this->here tape head position is changed.
//
//
// NOTE
//   If the memory allocation fails, tmTapeAlloc() exits the process.
//
//
// SEE ALSO
//   tmTapeMove(), tmTapeIndex(), realloc()
*/
void
tmTapeAlloc(TuringMachineT *this)
{
  int64_t ti = tmTapeIndex(this);

  if(this->tape_len <= ti) {
    int64_t ni;

    if((this->tape = realloc(this->tape, sizeof(Char) * (ti+1)))==NULL)
    {
      fprintf(stderr, "tmTapeAlloc: out of memory\n");
      exit(1);
    }

    /* Blank out the new tape elements */
    for(ni=this->tape_len; ni <= ti; ni++) {
      /*printf("tmTapeAlloc: blanking tape[%li]\n", ni);*/
      this->tape[ni] = 0;
    }

    this->tape_len = ti + 1;
  }
}




/* NAME
//   tmTapeMove: move Turing Machine tape head
//
//
// ARGUMENTS
//   this (in/out): Turing Machine
//   move (in): direction to move tape head.
//
//
// DESCRIPTION
//   tmTapeMove() moved the tape head of a Turing Machine either left
//   or right by one frame.  tmTapeMove() should be used as the only
//   way to move a tape head.
//
//   If the tape had not previously been long enough to refer to the
//   index that results from the move, then the tape is reallocated to
//   include the new tape index, and the new tape elements are
//   initialized to blank (0).
//
//
// RETURN VALUE
//   Return -1 if a Stop instruction was reached.
//   Return 0 if a MOVE_LEFT or a MOVE_RIGHT was performed.
//
//
// SEE ALSO
//   tmTapeAlloc()
*/
int
tmTapeMove(TuringMachineT *this, Move move)
{
  if(move==MOVE_LEFT) {
    this->here --;
  } else if(move==MOVE_RIGHT) {
    this->here ++;
  } else {
    return -1;
  }
  tmTapeAlloc(this);
  return 0;
}




/* NAME
//   tmTapeWrite: write a tape for a Turing Machine in machine readable way
//
//
// DESCRIPTION
//   Write the tape of a Turing Machine in a file format suitable
//   for the tape to be read in by tmTapeRead.
//
//
// RETURN VALUE
//   If there was an error, return a non-positive number.
//   If the tape was written without error, return a positive number.
//
//
// SEE ALSO
//   tmTapeRead, tmTapeHead, tmTapeFrame
*/
int
tmTapeWrite(TuringMachineT *this, const char *filename)
{
  const int64_t here     = this->here;

  /* far_head: head position corresponding to the end of the tape */
  const int64_t far_head = tmTapeHead(this, this->tape_len - 1);

  int64_t       start_index;
  int64_t       last_index;
  FILE      *stream;

  /* Open the tape file for writing */
  if((stream=fopen(filename, "w"))==NULL) {
    fprintf(stderr, "tmTapeWrite: error opening '%s'\n", filename);
    return -1;
  }

  /* Print the comment line */
  fprintf(stream, "# tmTapeWrite: %lli frames, head at %lli\n",
          this->tape_len, this->here);

  /* Print the starting index */
  if(far_head >= 0) {
    start_index = -far_head;
    last_index = far_head;
  } else {
    start_index = far_head;
    last_index = -(far_head+1);
  }
  fprintf(stream, "start %lli\n", start_index);

  /* Print the current state */
  {
    const int32_t state = this->state;  /* type conversion */
    fprintf(stream, "state %i\n", state);
  }

  /* Print the tape frames */
  {
    int64_t frame;

    for(this->here = start_index; this->here <= last_index; this->here ++)
    {
      frame = tmTapeFrame(this);  /* type conversion */
      if(this->here == here) {
        fprintf(stream, "# tape head at %lli\nhead ", this->here);
      }
      fprintf(stream, "%lli\n", frame);
    }
  }

  /* put tape head back where it was */
  this->here = here;

  /* Close the output file */
  fclose(stream);

  return 1;
}




/* NAME
//   tmTapeRead: read a tape for a Turing Machine
//
//
// TAPE FILE FORMAT
//   The tape file is a text file which described the initial value of
//   the tape.  The first line of the tape file must be a comment line
//   of the form
//
//     # comment
//
//   The second line of the tape file must be of the form
//
//     start <integer>
//
//   where the integer is the starting index of the rest of the values
//   in the tape file.
//
//   The third and subsequent lines in the tape file are tape frame
//   values or a line of the form
//
//     state <integer>
//
//   A line beginning with the string "state" is used to indicate the
//   starting state of the machine.  If no "state" line is present,
//   then state 0 is assumed to be the starting state.  Usually, the
//   user will not supply a starting state because    the convention
//   with Turing Machines is that their initial state is the first
//   state in the table.  The "state" ine feature is intended to be
//   used only for restarting a Turing Machine simulation from a
//   previous execution.  Only one "state" line may be present.
//
//   The values of the tape frames are interpretted as sequential from
//   left to right, starting at the index given on the second line of
//   the tape file.  Tape values must be one per line.
//
//   If a tape value is preceded by the string "head" then the
//   corresponding tape frame will be the starting location of the tape
//   head when the simulation starts.  If the tape head is not
//   specified in this way (i.e., if there are no tape values preceded
//   by the "head" string) then the initial head location will be at
//   index 0.  If more than one "head" line is present in the tape file,
//   an error occurs.
//
//
// NOTE
//   At least enough memory is allocated for the tape to hold all of
//   the characters read from the tape file.  Any extra memory
//   allocated for the tape is set to blank.
//
//
// RETURN VALUE
//   Return negative value if error occurs.
//   Return positive value if no errors occured.
//
//
// SEE ALSO
//   tmTapeMove(), tmTapeAlloc(), tmTapeWrite()
*/
int64_t
tmTapeRead(TuringMachineT *this, char *filename)
{
  char  line[1024];      /* input line buffer, for parsing file */
  int64_t  start;           /* index of first frame in tape file */
  int32_t  character;       /* input character from tape file */
  int32_t  line_num    = 1; /* current file line number.  used for debugging */
  FILE *stream;          /* tape file stream */
  int64_t  head_start  = 0; /* starting head location.  Default is 0 */
  int   head_given  = 0; /* flag: was head location present in file? */
  int32_t  state;           /* initial state */
  int   state_given = 0; /* flag: was initial state present in file? */



  /* Open the tape file for reading */
  if((stream=fopen(filename, "r"))==NULL) {
    fprintf(stderr, "tmTapeRead: error opening '%s'\n", filename);
    return -1;
  }

  /* Read and ignore the first line -- comment line.
  // Note that the comment line must be present,
  // and must be the first line of the tape file.
  */
  fgets(line, 1023, stream);
  line_num++;
  printf("%s: %s\n", filename, line);

  /* Read the starting-index */
  /* This scanf will take an empty string as a valid integer.
  // Sounds fucked up to me.  In any case, that integer takes the value
  // zero which is acceptable.
  */
  if((fscanf(stream, "start %lli \n", &start))!=1) {
    fprintf(stderr, "tmTapeRead: error reading start index\n");
    fclose(stream);
    return -2;  /* return "error" status */
  }
  line_num++;

  /* Set the tape head position at the left end of where the
  // tape file provides tape data.
  */
  this->here = start;

  /* Allocate some memory for the tape */
  tmTapeAlloc(this);  /* call tmTapeAlloc whenever this->here is changed */

  /* Move tape head one frame to the left because the tape head is moved
  // to the right just before setting the frame value.
  // This have to be done after the tmTapeAlloc because we only want to
  // allocate as much space as we need, and no more.  It would not be
  // crucial that we do not allocate more space except that if the tape
  // being read is a tape written from a previous run, and more tape
  // were allocated than necessary, then the tape would gradually grow
  // unnecessarily.
  */
  this->here -- ;

  /* Read the tape characters from the tape file,
  // and store them in the Turing Machine tape array.
  */
  while(fgets(line, 1024, stream)) {
    line[strlen(line)-1] = '\0';  /* remove trailing newline */

    if(   ((sscanf(line, " %i ", &character))==1)
       || ((sscanf(line, "head %i ", &character))==1)
      )
    {
      if((character < -1) || (character > this->charset_max)) {
        // int32_t charset_max = this->charset_max;  /* type conversion */
        fprintf(stderr, "tmTapeRead: %i: character %i too big > %i\n",
                line_num, character, this->charset_max);
        return -3;
      }

      tmTapeMove(this, MOVE_RIGHT);
      tmTapeFrame(this) = character;  /* type conversion */

      if(!strncmp(line, "head", 4)) {
        if(head_given) {
          fprintf(stderr,
                  "tmTapeRead: err: %i: multiple head locations given\n",
                  line_num);
          return -4;
        }
        head_start = this->here;
        head_given = 1;
      }

    } else if(sscanf(line, "state %i ", &state)==1) {
      if(state_given) {
        fprintf(stderr,
                "tmTapeRead: err: %i: multiple initial states given\n",
                line_num);
          return -5;
      }
      state_given = 1;
      this->state = state;

    } else if (line[0] == '#') {
      /* comment line -- ignore */

    } else if (line[0] == '\0') {
      /* blank line -- ignore */

    } else {
      fprintf(stderr, "tmTapeRead: %i: error reading character\n", line_num);
      fprintf(stderr, "tmTapeRead: %i: line='%s'\n", line_num, line);
      fclose(stream);
      return -6;  /* return "error" status */
    }

    /* Increment line counter for The next pass through this loop body */
    line_num ++;
  }

  /* Close the tape file */
  fclose(stream);

  /* Move tape head to initial position */
  this->here = head_start;
  tmTapeAlloc(this);  /* call tmTapeAlloc whenever this->here is changed */

  return line_num;  /* return "okay" status */
}




/* NAME
//   tmTapeBlank:  erase Turing machine tape and initialize a new one
*/
void
tmTapeBlank(TuringMachineT *this)
{
  tmTapeFree(this);

  /* Set the tape head position at the left end of where the
  // tape file provides tape data.
  */
  this->here = 0;

  /* Allocate some memory for the tape */
  tmTapeAlloc(this);  /* call tmTapeAlloc whenever this->here is changed */
}




/* NAME
//   tmTapePrint: print Turing Machine tape
//
//
// DESCRIPTION
//   The tape of a Turing Machine is printed.  The tape frame at which
//   the tape head is sitting is indicated by a head location marker The
//   tape frame where the head started (tape frame 0) is indicated by a
//   frame index marker.
//
//
// SEE ALSO
//   tmTapeIndex()
*/
void
tmTapePrint(TuringMachineT *this)
{
  int64_t ti;
  int64_t left_end;
  int64_t right_end;
  int64_t here = this->here;

  printf("TAPE:\n");
  printf("-----\n");
  if(this->tape_len % 2 == 0) {
    /* last element on tape is odd, i.e. has a negative index */
    left_end = - this->tape_len / 2;
    right_end = this->tape_len + left_end - 1;
  } else {
    /* last element on tape is even, i.e. has a non-negative index */
    right_end = this->tape_len / 2;
    left_end = right_end - this->tape_len + 1;
  }

#if (VERBOSE >= 2)
  printf("tape spans from %li to %li\n", left_end, right_end);
  this->here = left_end;
  printf("tape indices are %li", tmTapeIndex(this));
  this->here = right_end;
  printf(" to %li\n", tmTapeIndex(this));
  this->here = here;
#endif

  /* Print tape */
  for(ti=left_end; (this->here=ti) <= right_end; ti++) {
    int64_t tape_frame = tmTapeFrame(this);  /* type conversion */

    if(ti==0) {
      printf(" [0]>");
    }
    if(ti==here) {
      printf(" [head@%lli]>", ti);
    }
    printf(" %1lli", tape_frame);
  }
  this->here = here;  /* restore tape head location */
  printf("\n");
}




/* tmTapeOneCount: count the number of '1's on a Turing Machine tape
//
// RETURN VALUE
//   Return the number of '1's on the tape.
*/
int64_t
tmTapeOneCount(const TuringMachineT *this)
{
  int64_t ti;
  int64_t count = 0;

  for(ti=0; ti < this->tape_len; ti++) {
    if(this->tape[ti] == 1) {
      count ++;
    }
  }
  return count;
}




/* NAME
//   tmTapeCurse: use curses to display Turing Machine tape segment
//
//
// DESCRIPTION
//   Display a segment of a Turing Machine tape.  The length of the
//   segment which is displayed depends on the width of the terminal
//   screen.
//
//   The tape display has three parts, verticlly stacked: the tape
//   indices, the tape values, and the tape head position.
//
//   The tape display shows the tape indices above the tape values.
//   These indices are not actually used by the Turing Machine, but
//   they are useful to a human trying to keep track of what the Turing
//   Machine is doing.  The tape index 0 is the starting point for the
//   tape head.
//
//   The tape values are printed with spaces between the values.
//   Remember that, for a true Turing Machine, the tape is infinitely
//   long.  However, in this simulation, the tape's stored length is
//   finite (although the tape will "grow" indefinitely, as new
//   portions are accessed) .  When the entire stored tape will not fit
//   on the screen, then ellipsis are displayed indicating that there
//   is more of the tape than what is being displayed.  Remember that
//   if ellipsis are not visible, it means that the representation of
//   the tape is not currently being stored.  It does NOT mean that you
//   are looking at the end of the tape.  There is no end of the tape.
//   The tape is infinite.
//
//   The tape values will scroll left and right as the tape head
//   moves.  The tape head will also move, when scrolling the tape is
//   not appropriate.
//
//   Below the tape values portion of the tape display is the tape head
//   location indicator, along with the tape index where the tape head
//   sits.  Again, this index is not used by the abstract Turing
//   Machine, but it is useful for humans.
//
//
// ARGUMENTS
//   wy (in): row to print tape, where row 0 is the top
//   wx (in): column to print tape, where colunm 0 is the left
//
//
// SEE ALSO
//   tmTapePrint(), curses library
*/
void
tmTapeCurse(const TuringMachineT *this, int wy, int wx)
{
  int            size_x;
  int64_t           ti;
  int64_t           left_end;
  int64_t           right_end;
  int64_t           tape_print_left;
  int64_t           tape_print_right;
  TuringMachineT shallow_copy      = *this;

  /* Get size of the curses window */
  {
    int min_x, min_y;
    int max_x, max_y;

    getbegyx(stdscr, min_y, min_x);
    getmaxyx(stdscr, max_y, max_x);
    size_x = max_x - min_x - 16 - wx;
  }

  /* Find ends of tape */
  if(this->tape_len % 2 == 0) {
    /* last element on tape is odd, i.e. has a negative index */
    left_end  = - this->tape_len / 2;
    right_end =   this->tape_len + left_end - 1;
  } else {
    /* last element on tape is even, i.e. has a non-negative index */
    right_end =             this->tape_len / 2;
    left_end  = right_end - this->tape_len + 1;
  }

  /* Calculate what segment of the tape to display */
  tape_print_left  = this->here - size_x / 4;
  tape_print_right = this->here + size_x / 4;

  if(tape_print_left < left_end) {
    /* Make sure we do not try to refer off end of tape */
    tape_print_left  = left_end;
    /* ... but try to print as much of the tape as possible */
    tape_print_right = tape_print_left + size_x / 2;
  }

  if(tape_print_right > right_end) {
    /* Make sure we do not try to refer off end of tape */
    tape_print_right = right_end;
    /* ... but try to print as much of the tape as possible */
    tape_print_left  = tape_print_right - size_x / 2;
  }

  if(tape_print_left < left_end) {
    /* Make sure we do not try to refer off end of tape */
    tape_print_left = left_end;
  }


  /* Print tape index marker value. */
  move(wy, wx);
  printw("   ");
  for(ti=tape_print_left; (ti <= tape_print_right) && (ti%10 != 0); ti++) {
      printw("  ");
  }
  for(; ti <= tape_print_right; ti+=10) {
      printw(" %-19li", ti);
  }
  printw("\n");
  wy++;

  /* Print tape index marker. */
  move(wy, wx);
  printw("   ");
  for(ti=tape_print_left; ti <= tape_print_right; ti++) {
    if(ti%10 == 0) {
      printw(" |");
    } else {
      printw("  ");
    }
  }
  printw("\n");
  wy++;


  /* Print tape */
  move(wy, wx);
  if(tape_print_left > left_end) {
    printw("...");
  } else {
    printw("   ");
  }

  for(ti=tape_print_left; ti <= tape_print_right; ti++) {
    shallow_copy.here = ti;
    printw(" %1li", this->tape[tmTapeIndex(&shallow_copy)]);
  }

  if(tape_print_right < right_end) {
    printw(" ...");
  } else {
    printw("    ");
  }
  printw("\n");
  wy++;


  /* Print tape head marker, "^" */
  move(wy, wx);
  printw("   ");
  for(ti=tape_print_left; ti <= tape_print_right; ti++) {
    if(ti == this->here) {
      printw(" ^");
      break;
    } else {
      printw("  ");
    }
  }
  printw("\n");
  wy++;


  /* Print tape head location value. */
  move(wy, wx);
  printw("   ");
  for(ti=tape_print_left; ti <= tape_print_right; ti++) {
    if(ti == this->here) {
      printw("% li", this->here);
      break;
    } else {
      printw("  ");
    }
  }
  printw("\n");
}




/* NAME
//   tmUpdate: update the state and tape of a Turing Machine
//
//
// DESCRIPTION
//   A single step in the execution of the simulation of a Turing
//   Machine involves these steps:
//     (1) Read the current tape frame.  This is the "input".
//     (2) Look in the state transition table at the entry for the
//         current state and input tape frame value.  This entry will
//         have 3 fields:
//
//           (a) what to write (output) at the current tape frame
//           (b) which way to move the tape head
//           (c) what state to enter into next.
//
//          Alternatively, the entry could simply be a "stop"
//          instruction.
//
//     (3) If the instruction was "stop" then stop.  Otherwise, write,
//         at the current tape frame, the output
//     (4) Move the tape head.
//     (5) Change the state.
//
//   tmUpdate performs all of these steps.
//
//   Presumably, tmUpdate will be called from within a loop.  That loop
//   should break when tmUpdate reaches a "stop" instruction.
//
//
// NOTE
//   The "stop" instruction may be accompanied by a "write".  It seems
//   that the definition of a Turing Machine varies with respect to this
//   detail.
//
//
// RETURN VALUE
//   Return 1 if a "stop" statement was reached.
//   Return 0 if a "MOVE_LEFT" or "MOVE_RIGHT" was done to the tape
//   head, along with the associated other steps.
//
//
// SEE ALSO
//   tmTapeMove(), tmTapeFrame(), tmSimulate()
*/
int
tmUpdate(TuringMachineT *this)
{
  /* Read the tape here */
  const Char input  = tmTapeFrame(this);
  const Move move   = this->table[this->state][input].move;

  /* Write the value for this input */
  tmTapeFrame(this) = this->table[this->state][input].write;

  /* Move the tape */
  tmTapeMove(this, this->table[this->state][input].move);

  /* Set the machine into the new state */
  this->state = this->table[this->state][input].next;

  if(STOP == move) {
    return 1; /* Turing machine reached STOP */
  }

  return 0; /* keep going */
}




/* tmStatePrint: print the current state and tape head position
//
// For debugging.
//
// SEE ALSO
//   tmStateCurse()
*/
void
tmStatePrint(const TuringMachineT *this)
{
  // int32_t state = this->state;  /* type conversion */
  printf("state %i    head %lli\n", this->state, this->here);
}




/* NAME
//   tmSimulate: perform a Turing Machine simulation
//
//
// DESCRIPTION
//   A Turing Machine simulation is performed without any frills.
//
//
// ARGUMENTS
//   max_iters (in): maximum number of iterations (shifts) before quitting
//
//
// RETURN VALUE
//   Returns number of shifts if stop occured withing turing machine.
//   Returns -1 if max_iters was reached.
//
//
// SEE ALSO
//   tmUpdate(), tmVisualSimulate()
*/
int64_t
tmSimulate(TuringMachineT *this, int64_t max_iters, int64_t tape_len_max)
{
  int64_t       stop      = 0;
  int64_t       iters;

#ifdef BUSY_BEAVER_SEARCH
  /* The BUSY_BEAVER_SEARCH code slows this loop down by %. */
  const int64_t iter_test = this->num_states * (this->charset_max + 1);
#endif

  for(iters=0;
      !stop && (iters < max_iters) && (this->tape_len < tape_len_max);
      iters++)
  {
    stop = tmUpdate(this);
#ifdef BUSY_BEAVER_SEARCH
    if(iters < iter_test) {
      if(0 == this->state) {
        if(! tmTapeOneCount(this)) {
          return -3;
        }
      }
    }
#endif
  }

  if(stop) {
    return iters;
  }

  if(this->tape_len >= tape_len_max) {
    return -2;
  }

  return -1;
}




/* NAME
//   helpPrint: print helpful information about "visual mode" keys
//
//
// SEE ALSO
//   tmTapeCurse(), tmTableCurse(), curses library
*/
void
helpPrint(void)
{
  int max_x, max_y;

  /* Get size of the curses window */
  getmaxyx(stdscr, max_y, max_x);

  /* Clear a portion of the screen */
  max_y -= 8;
  move(max_y,1);
  clrtobot();
  printw("\n");
  printw("'Escape' to halt machine\n");
  printw("'d' to toggle debug mode (i.e. single step mode)\n");
  printw("'v' to toggle visual display of execution\n");
  printw("'?' to display this information\n");
  printw("\n");
  printw("Press any key to return to the Turing Machine visual display\n");
  refresh();

  /* Wait for a key press */
  nodelay(stdscr, FALSE);  /* blocking input */
  getchar();

  clear();
}




/* NAME
//   tmCursesDisplay: Display the "visual mode" Turing Machine display
//
//
// DESCRIPTION
//   The "visual" mode displays the state transition table, the segment
//   of the tape in the vicinity of the tape head, and some other
//   information.
//
//
// SEE ALSO
//   tmTapeCurse(), tmTableCurse(), curses library
*/
void
tmCursesDisplay(const TuringMachineT *this, int64_t iters)
{
  tmTapeCurse(this, 1, 1);

  move(7,1);
  printw("Shift %-10lli    tape length %li\n", iters, this->tape_len);
  tmTableCurse(this, 9, 1);

  refresh();

  #if __CYGWIN__
  // In 'rxvt' under Cygwin, curses refresh does not seem to work
  // unless the terminal output buffer gets flushed, with actual output.
  // This printf, with newline, makes refresh do what we want (except we get an extra newline).
  // printf("\n");
  // Sleeping for the duration of a frame refresh also seems to work.

  usleep(13333);        // Update no faster than display frame rate.
                        // 75 Hz <==> 13333 microseconds
                        // 60 Hz <==> 16667 microseconds
  #endif
}




/* NAME
//   tmVisualSimulate: perform Turing Machine simulation with display
//
//
// DESCRIPTION
//   tmVisualSimulate performs a simulation of a Turing Machine, and
//   displays information about the simulation as it runs.  The
//   information displayed includes the state transition table with the
//   current state highlighted, the current tape head position, and a
//   segment of the tape in the vicinity of the tape head.  The display
//   is all textual, and uses the "curses" screen handling package.
//
//
// RETURN VALUE
//   Return the number of iterations (aka shifts) executed.
*/
int64_t
tmVisualSimulate(TuringMachineT *this, int64_t max_iters, int64_t tape_len_max, int debug)
{
  int       input          = 0; /* input character from keyboard */
  int64_t iters       = 0; /* number of shifts Turing Machine has executed */
  int       update_display = 1; /* flag: update display of Turing Machine? */

  /* Set up curses "visual mode" */
  initscr();

  cbreak();
  noecho();  /* Do not display key presses */
  nonl();

  if(!debug) {
    /* Use non-blocking input.
    // "non-blocking" means that a read of the keyboard will not
    // suspend the process until a key is pressed.
    */
    nodelay(stdscr, TRUE);
  }

  /* Display "visual mode" stuff */
  tmCursesDisplay(this, iters);

  /* Main simulation loop: */
  /* Note that 'iters' is initialized to '1' instead of '0'.
  // This is because 'iters' is incremented after the end of the loop
  // body, but before the end of the loop body, we print out the number
  // of iterations that have passed.  Also, the very last iteration of
  // the loop ends with a 'break' statement, which means that the end of
  // the loop body is not executed on the last iteration.  Since 'iters'
  // is initialized to '1', the number of iterations that have passed is
  // correct after the point the 'tmUpdate' call is made.
  */
  for(iters=1; (iters < max_iters) && (this->tape_len < tape_len_max); iters++)
  {

    /* Read a keyboard key press, if there is one */
    input = getch();

    if(input == 27 /*Escape*/) {
      /* User pressed "Escape" key to halt machine */
      break;

    } else if(input == 'd') {
      /* Toggle debug mode (i.e., single step mode) */
      debug = !debug;
      if(debug) {
        nodelay(stdscr, FALSE);         /* blocking input */
      } else {
        nodelay(stdscr, TRUE);          /* non-blocking input */
      }

    } else if(input == 'v') {
      /* Toggle the visual progess updating */
      update_display = !update_display;

    } else if(input == '?') {
      /* Print the help screen summary */
      helpPrint();

      /* Restore the visual display */
      tmCursesDisplay(this, iters);

      /* Restore blocking or non-blocking keyboard input */
      if(debug) {
        nodelay(stdscr, FALSE);         /* blocking input */
      } else {
        nodelay(stdscr, TRUE);          /* non-blocking input */
      }
    }

    /* Step forward the Turing Machine */
    if(tmUpdate(this)) {
      /* The Turing Machine stopped, so break the simulation loop */
      break;
    }

    if(update_display) {
      /* Display "visual mode" stuff */
      tmCursesDisplay(this, iters);
    }
  } /* end of simulation loop */

  /* The Turing Machine finished (for one of several reasons) */

  /* Do a final visual update to show the final state */
  tmCursesDisplay(this, iters);

  /* Print some info about how the simulation went */
  move(0,0);
  if(iters >= max_iters) {
    printw("MAXIMUM ITERATIONS REACHED -- press a key to finish");
  } else if(input == 27) {
    printw("MACHINE HALTED BY USER after %lli shifts -- press a key to finish",
           iters);
  } else if(this->tape_len >= tape_len_max) {
    printw("TAPE TOO LONG after %lli shifts -- press a key to finish",
           iters);
  } else {
    printw("MACHINE STOPPED after %lli shifts -- press a key to finish",
           iters);
  }
  refresh();

  /* Wait for a key press so that the display does not disappear
  // without being seen by the user.
  */
  nodelay(stdscr, FALSE);  /* blocking input */
  input = getch();

  endwin();

  return iters;
}




/* NAME
//   tmTableNext: find next Turing Machine, in lexical order
//
//
// DESCRIPTION
//   tmTableNext is intended to be used to systematially generate all
//   possible Turing machine state transition tables.  Given a Turing
//   machine, the transition table is updated to the lexically next
//   table.
//
//   Lexical order:
//     Fields are incremented in this order: write, next, move.
//     Values for write start at 0 and end at charset_max.
//     Values for next start at 0 and end at (this->num_states - 1).
//     Values for move, in order, are MOVE_LEFT, MOVE_RIGHT, and STOP.
//
//     There is a further rule for lexical incrementation:  When 'move'
//     is "STOP" then, for the purposes of finding a busy beaver, you
//     might as well choose only 'write' "1" entries, and the 'next'
//     field has no meaning.
//
//
// RETURN VALUES
//   0 if the states were not exhausted.
//   1 if this went past the last state.
//
//
// SEE ALSO
//   tmTableIndex(), tmTableWeed(), tmTableBFS()
*/
int
tmTableNext(TuringMachineT *this)
{
  int si;  /* state index */
  int ii;  /* input index */

  for(si = 0; si < this->num_states; si++) {
    for(ii = 0; ii <= this->charset_max; ii++) {
      if(this->table[si][ii].write == this->charset_max) {
        /* wrap this field and advance to next field */
        this->table[si][ii].write = 0;

        if(this->table[si][ii].next == (this->num_states - 1)) {
          /* wrap this field and advance the next field */
          this->table[si][ii].next = 0;

          if(this->table[si][ii].move == STOP) {
            /* wrap entire entry and loop */
            this->table[si][ii].write = 0;
            this->table[si][ii].next = 0;
            this->table[si][ii].move = MOVE_LEFT;
          } else {
            /* increment and return */
            this->table[si][ii].move ++;
            if(this->table[si][ii].move == STOP) {
              this->table[si][ii].write = 1;
              this->table[si][ii].next = (this->num_states - 1);
            }
            return 0;
          }
        } else {
          /* increment and return */
          this->table[si][ii].next ++;
          return 0;
        }
      } else {
        /* increment and return */
        this->table[si][ii].write ++;
        return 0;
      }
    }
  }
  return 1;
}




/*
// RETURN VALUES
//   Return 0 if this table does not contain a STOP.
//   Return nonzero if this table contains a STOP.
*/
int
tmTableContainsStop(TuringMachineT *this)
{
  int si;            /* state index */
  int ii;            /* input index */

  for(si = 0; si < this->num_states; si++) {
    for(ii = 0; ii <= this->charset_max; ii++) {
      if(STOP == this->table[si][ii].move) {
        return 1;
      }
    }
  }

  return 0;
}




/* NAME
//   tmTableBFS: breadth first search of Turing Machine table
//
//
// DESCRIPTION
//   tmTableBFS does a fairly ignorant descent through a Turing Machine
//   table to see whether possibly reachable entries satisfy some
//   criterion, which is determined by 'test'.
//
//   Algorithm:
//     Push state 0 into FIFO.
//     Loop until (state counter exceeds number of states), or (FIFO is empty):
//       Pop the FIFO to get a state.
//       For each entry of this state:
//         If entry satisfies test, then return 1.
//         Push next state of this entry into FIFO.
//       Increment state conter.
//
//
// RETURN VALUES
//   Return result of 'test' if test() ever gives nonzero result.
//   Return zero otherwise.
*/
int
tmTableBFS(TuringMachineT *this, int test(const Entry * const))
{
  int count = 0;             /* iteration counter */
  int *state_pushed = NULL;  /* array of Booleans */
  int state;                 /* index of state being considered */
  int test_value;            /* value returned by test */
  int ii;                    /* table input character index */
  static FifoT *fifo = NULL; /* used for breadth first search */

  if(fifo == NULL) {
    /* We will be using a FIFO of the same size each time so there's
    // no reason to allocate it each time.  Just use one FIFO and recycle
    // it.
    */
    fifo = fifoNew(this->num_states * (this->charset_max + 1));
  }

  /* Allocate the Boolean array in stack space (automatic) */
  if((state_pushed = alloca(sizeof(int) * (this->num_states + 1))) == NULL)
  {
    fprintf(stderr, "tmTableBFS: out of memory\n");
    fifoDestroy(fifo);
    abort();
    return -1;
  }

  /* Reset Booleans to false: no states pushed yet */
  {
    int si;
    for(si=0; si <= this->num_states; si++) {
      state_pushed[si] = 0;
    }
  }

  /* Push state 0 into fifo */
  fifoAdd(fifo, (void*)1);
  state_pushed[0] = 1;
  count = 1;

  while( (count <= this->num_states) && ((state=(int)fifoPop(fifo))!=0) )
  {
    /* For the FIFO, states are indexed starting at 1, so adjust */
    state--;

    for(ii=0; ii <= this->charset_max; ii++) {
      if(test != NULL) {
        if((test_value=test(&this->table[state][ii]))) {
          fifoReset(fifo);
          return test_value;
        }
      }

      /* If 'next' state has not been in the FIFO, push it in */
      if(!state_pushed[this->table[state][ii].next]) {
        /* Push next state of this entry into fifo.
        // (For the FIFO, states are indexed starting at 1, so adjust)
        */
        fifoAdd(fifo, (void*)(this->table[state][ii].next + 1));
        state_pushed[this->table[state][ii].next] = 1;

        count ++;
      }
    }
  }

  /* Table feature was not reachable */
  fifoReset(fifo);
  if(test != NULL) {
    return 0;
  } else {
    return count;
  }
}




/* found* functions: test functions for tmTableBFS
*/
int
foundStop(const Entry * const this)
{
  if(this->move == STOP) return 1;
  return 0;
}


int
foundOne(const Entry * const this)
{
  if(this->write == 1) return 1;
  return 0;
}


int
foundLeft(const Entry * const this)
{
  if(this->move == MOVE_LEFT) return 1;
  return 0;
}


int
foundRight(const Entry * const this)
{
  if(this->move == MOVE_RIGHT) return 1;
  return 0;
}




/* NAME
//   tmTableWeed: weed out useless (for busy beaver) Turing Machine tables
//
//
// RETURN VALUES
//   Return 0 if this table is not rejected;
//   Return nonzero if this table is rejected.
*/
int
tmTableWeed(TuringMachineT *this)
{
  if(0 == this->table[0][0].next) {
    return 1;
  } else if(STOP == this->table[0][0].move) {
    return 2;
  } else if(!tmTableContainsStop(this)) {
    /* No stops present in the table: never stops */
    return 3;
  } else if(!tmTableBFS(this, foundStop)) {
    /* Stop not reachable: never stops */
    return 4;
  } else if(tmTableBFS(this, NULL) < this->num_states) {
    /* Machine does not refer to all states */
    return 5;
  } else if(!tmTableBFS(this, foundOne)) {
    /* No ones reachable: this machine can not be a useful busy beaver */
    return 6;
  } else if(tmTableBFS(this, foundLeft) && !tmTableBFS(this, foundRight)) {
    /* Machine moves only to the left */
    return 7;
  } else if(tmTableBFS(this, foundRight) && !tmTableBFS(this, foundLeft)) {
    /* Machine moves only to the right */
    return 8;
  }
  return 0;
}




/* turing_machine: global reference to current Turing machine
//   intended for use in interrupt handler
*/
static TuringMachineT *turing_machine;




/* NAME
//   handle_int: signal handler for busy beaver search
//
//
// DESCRIPTION
//   Searching for busy beavers takes a long time.  In order to be able
//   to continue the search where it left off, it is useful to trap
//   signals which would interrupt the process.  handle_int catches
//   some signals and outputs enough information to continue the search
//   for busy beavers where the search left off.
*/
void
handle_int(int signum)
{
  printf("\n");
  tmTablePrint(turing_machine);
  tmTableWrite(turing_machine, "interrupt.tm");
  exit(1);
}




/* PERIOD: how often to output a Turing machine table, for restart purposes
*/
#define PERIOD (21*21*21*21+1)


/* PATIENCE: print some output so the user knows hw the search is going
//
// Also occasionally output a table so that the search can be restarted
// without losing a lot of effort, in case of a catastrophe, like a
// system crash.
*/
#define PATIENCE \
{ \
  if(weed>5) printf("%i",weed); \
  fflush(stdout); \
  if(table_count % PERIOD == 0) { \
    tmTableWrite(this, "periodic.tm"); \
    printf("\ntable %014lli, %lli simulated\n", table_count, table_sim_count); \
    tmTablePrint(this); \
  } \
}




/* NAME
//   tmBusyBeaverSearch: Search for a busy beaver Turing machine
//
// ARGUMENTS
//   this (in/out): Turing machine
//
//   max_iters (in): maximum number of iterations for each machine
//
//   tape_len_max (in): maximum tape length for each machine
//
//   visual (in): whether to simulate machine in visual mode
//
//   debug (in): whether to start machine simulation in debug mode
//
//
// DESCRIPTION
//   A "busy beaver" is a Turing machine that prints out a lot of
//   '1's.  There is a sort of unofficial contest to find the most
//   prolific 5-state, 2-character busy beaver.  Actually, the contest
//   is somewhat official:  Scientific American had an article in
//   Computer Recreations in the mid 1980's that posed this problem as
//   a contest.  Several solutions are known, but so far nobody has
//   demonstrated that a particular solution is the best one.
//   tmBusyBeaverSearch searches for busy beavers, and is not limited
//   to 5-state, 2-input Turing machines.  Any Turing machine
//   configuration is possible, although smaller busy beavers are not
//   interesting and larger ones have an impractically large
//   configuration space.  In fact, the configuration space for the
//   5-state, 2-character busy beaver is 21^10 = 16,679,880,978,201 (16
//   trillion).
//
//   The famous "halting problem" for Turing machines and their
//   equivalent asks whether it is possible to determine whether a
//   given machine will ever stop on a given tape.  It is known that
//   this problem has no solution.  Therefore, it is not a trivial task
//   to search for busy beavers since it is not known in advance
//   whether a particular machine will ever stop.  Therefore, finite
//   limits must be set to make the search practical.  The limits are
//   the maximum number of iterations 'max_iters' and the maximum
//   length of the tape, 'tape_len_max'.  In practice, the number of
//   iterations of a busy beaver is several orders of magnitude larger
//   than its tape length.  For example, the busy beaver which
//   generates 4098 '1's has a tape of length less than 25000, but
//   executes 47,176,870.
//
//   Several simple methods are employed to attempt to determine ahead
//   of time whether a machine will make a good busy beaver.  See
//   tmTableWeed() for a list of these methods.  More sophisticated
//   methods are certainly possible (such as symmetry under state-row
//   swapping or simple loop checking), and could considerably reduce the
//   effective search space.
*/
int64_t
tmBusyBeaverSearch(TuringMachineT *this, int64_t max_iters,
                   int64_t tape_len_max, int visual, int debug)
{
  int weed;
  int64_t table_count = -1;
  int64_t table_sim_count = 0;
  int64_t iters;
  int64_t ones_max = 0;

  turing_machine = this;

  signal(SIGINT, handle_int);
  signal(SIGHUP, handle_int);

  if(table_count < 0) {
    table_count = tmTableIndex(this);
  }

  do {
    while((weed=tmTableWeed(this))) {
      tmTableNext(this);
      PATIENCE;
      table_count ++;
    }

    table_sim_count ++;
    if(debug) {
      iters = tmVisualSimulate(this, max_iters, tape_len_max, debug);
    } else {
      iters = tmSimulate(this, max_iters, tape_len_max);
    }

    /* Print some information about the how the simulation went */
    if(iters >= 0) {
      int64_t count = tmTapeOneCount(this);

      if(count >= (ones_max-1)) {
        char filename[128];

        printf("\ntable %014lli ties, with %lli\n", table_count, ones_max);

        ones_max = MAX(count, ones_max);
        printf("\n");
        tmTablePrint(this);

        sprintf(filename, "max%04lli.tm", count);
        tmTableWrite(this, filename);

        sprintf(filename, "max%04lli.tape", count);
        tmTapeWrite(this, filename);

        printf("table %014lli\n", table_count);
        printf("The machine executed %lli shifts\n", iters);
        printf("tape had %lli 1's\n", count);
        printf("tape was %lli frames long\n", this->tape_len);
      }
    } else if(-1 == iters) {
      printf("i");
      fflush(stdout);
    } else if(-2 == iters) {
      printf("t");
      fflush(stdout);
    } else if(-3 == iters) {
      printf("L");
      fflush(stdout);
    }

    /* Reset the Turing machine */
    this->state = 0;
    tmTapeBlank(this);

    PATIENCE;

    table_count ++;
  } while(!tmTableNext(this)) ;

  return 0;
}




int
main(int argc, char **argv)
{
  int debug             = 0; /* flag: debug mode (i.e. single step mode) */
  int visual            = 0; /* flag: visual mode. */
  int verbose           = 0; /* flag: verbose mode */
  int search            = 0; /* flag: search mode */

  int64_t iters = 0;   /* number of shifts the Turing Machine has executed */

  // max_iters: maximum number of iterations before the machine is stopped.
  const int64_t max_iters     = INT64_MAX ;

  const int64_t tape_len_max      = 409750;

  TuringMachine tm              = tmNew();


  /* Command line argument parsing variables.  See getopt() */
  int oc;
  int err_flag                  = 0;
  extern char *optarg;
  extern int optind, opterr, optopt;
  char *machine_file            = NULL;
  char *tape_file               = NULL;


  /* Parse command line arguments */
  while ((oc = getopt(argc, argv, "m:t:dsvV")) != -1) {
    switch (oc) {
      case 'm':
        machine_file = optarg;
      break;

      case 't':
        tape_file = optarg;
      break;

      case 'd':
        debug = 1;
        visual = 1;
      break;

      case 's':
        search = 1;
      break;

      case 'v':
        visual = 1;
      break;

      case 'V':
        verbose = 1;
      break;

      case '?':
        err_flag++;
    }
  }

  if (err_flag) {
    fprintf(stderr, "usage: %s -m machine_file -t tape_file [-d] [-v] [-V]\n",
            argv[0]);
    exit(2);
  }

  if(machine_file == NULL) {
    fprintf(stderr, "%s: must specify machine_file\n", argv[0]);
    exit(3);
  }

  if(tape_file == NULL) {
    fprintf(stderr, "%s: tape_file not specified.  Assuming blank.\n", argv[0]);
  }


  if(verbose) {
    /* max_iters is printed partly for information and partly to see
    // whether a number as large as max_iters is properly being stored
    // in the integer variable.
    */
    printf("maximum iterations = %lli\n", max_iters);
  }

  /* Read the Turing Machine state transition table */
  if(tmTableRead(tm, machine_file) < 0) exit(1);

  /* Read the Turing Machine tape */
  if(tape_file != NULL) {
    if(tmTapeRead(tm, tape_file) < 0) exit(2);
  } else {
    tmTapeBlank(tm);
  }

  if(verbose && !visual) {
    tmTablePrint(tm);
    tmTapePrint(tm);
    printf("table is lexically %014lli\n", tmTableIndex(tm));
  }

  if(search) {
    iters = tmBusyBeaverSearch(tm, max_iters, tape_len_max, visual, debug);

  } else if(visual) {
    iters = tmVisualSimulate(tm, max_iters, tape_len_max, debug);

  } else {
    /* Execute Turing Machine without "visual mode" display */
    iters = tmSimulate(tm, max_iters, tape_len_max);
  }

  if(verbose) {
    /* Print some information about the how the simulation went */

    tmTapeWrite(tm, "out.tape");
    printf("tape was %lli frames long\n", tm->tape_len);

    if(iters >= 0) {
      int64_t count = tmTapeOneCount(tm);

      tmTapePrint(tm);
      printf("The machine executed %lli shifts\n", iters);
      printf("tape had %lli 1's\n", count);
    } else if(-1 == iters) {
      printf("too many iterations\n");
    } else if(-2 == iters) {
      printf("tape too long\n");
    }
  }

  return 0;
}
