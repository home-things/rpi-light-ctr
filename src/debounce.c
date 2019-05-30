#include <wiringPi.h>
#include <stdio.h>
#include "isr.h"

// the debounce time, seconds
unsigned long debounceDelay = 1000;

void debounceImpulse(void (*onImpulse)(void), unsigned int pin, bool *prevState, unsigned long *lastHighTime)
{
  // read the state of the switch into a local variable:
  bool state = digitalRead(pin);
  fprintf(stderr, "state: %d\n", state);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:
  // If the switch changed, due to noise or pressing:

  fprintf(stderr, "*lastHighTime: %lu\n", *lastHighTime);

  if (state != *prevState && state == HIGH)
  {
    if ((millis() - *lastHighTime) > debounceDelay)
    {
      // whatever the state is at, it's been there for longer than the debounce
      // delay, so take it as the actual current state:

      fprintf(stderr, "enough time: %lu\n", millis() - *lastHighTime);

      // only toggle the LED if the new button state is HIGH
      (*onImpulse)();
    }

    // reset the debouncing timer
    *lastHighTime = millis();
    fprintf(stderr, "state changed. time: %lu; state: %d\n", *lastHighTime, state);
  }

  // save the state. Next time through the loop, it'll be the *prevState:
  *prevState = state;
}
