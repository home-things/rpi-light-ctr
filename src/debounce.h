#include "isr.h"

void debounceImpulse(
  void (*onImpulse)(void),
  unsigned int pin,
  bool *prevState,
  unsigned long *lastHighTime
);
