#include "SimpleTask.h"

#include <Arduino.h>

void SimpleTask::loop()
{
    int time_ms = millis();

    if (time_ms > this->_last + this->_interval)
    {
        this->_last = time_ms;
        this->run(this->_count++);
    }
}