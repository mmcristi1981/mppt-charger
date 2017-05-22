/*
  BufferedRead.h - Library for smoothing analog input to reduce jitter.
  Created by Mircea M. Moise, March 16, 2014.
*/
#ifndef BufferedAnalog_h
#define BufferedAnalog_h

#include "Arduino.h"

static const int BUFFER_SIZE = 50;

class BufferedAnalog
{
  public:
    BufferedAnalog(int pin);
    int read();
	int count();
	int get(int i);
  private:
	int pin;
	int index;
	int cnt;
	int buffer[BUFFER_SIZE];
};

#endif