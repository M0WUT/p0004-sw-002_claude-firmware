// GTE Core firmware. Core owns startup (issue #9); see lib/gte_core/src/gte/Gte.h.
#include <Arduino.h>
#include "gte/Gte.h"

void setup()
{
    gte::start();
}

void loop()
{
}
