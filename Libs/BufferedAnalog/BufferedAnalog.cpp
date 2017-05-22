#include "Arduino.h"
#include "BufferedAnalog.h"

BufferedAnalog::BufferedAnalog(int _pin)
{
	pin = _pin;
	index = -1;
	cnt = 0;
	for (int i = 0; i < BUFFER_SIZE; i++){
		buffer[i] = 0;
	}
}

int BufferedAnalog::read()
{
	int value = analogRead(pin);
	index ++;
	if (index >= BUFFER_SIZE) {
		index = 0;
	}
	buffer[index] = value;	
	cnt ++;
	if (cnt > BUFFER_SIZE){
		cnt = BUFFER_SIZE;
	}
	return value;
}

/*double BufferedAnalog::rms()
{
	unsigned long sum = 0;
	int i = index;
	int c = count;
	while (c > 0){
		i--;
		if (i < 0){
			i = BUFFER_SIZE - 1;
		}		
		long value = buffer[i];
		sum += value * value;
		c--;
	}	
	
	double result = sum / count;
	result = sqrt(result);
	return result;
}*/

int BufferedAnalog::count()
{
	return cnt;
}

int BufferedAnalog::get(int i)
{
	int idx = index - cnt + 1;
	if (idx < 0){
		idx += BUFFER_SIZE;
	}
	return buffer[(idx + i) % BUFFER_SIZE];
}
