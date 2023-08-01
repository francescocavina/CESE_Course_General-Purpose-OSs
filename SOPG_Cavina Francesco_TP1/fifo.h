/**
*	File: "fifo.h"
*	Author: Francesco Cavina
*
*/

/* includes ----------------------------------------------------------------- */

/* defines ------------------------------------------------------------------ */

/* public function prototypes ----------------------------------------------- */
void createNamedFifo(const char *fifoName);
int32_t openNamedFifo(const char *fifoName, int8_t openFlag);
