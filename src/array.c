#include <stdlib.h>


void array_free(void* array[], const int len)
{
	for(int i=0; i<len; i++) free(array[i]);
	free(array);
	array = NULL;
}

void* array_pop(void* array[], const int len)
{
	void* result = array[0];

	// Remove first position of array (move all elements one position before)
	int i=1;
	for(; i<len; i++)
		array[i-1] = array[i];

	// Free last position of argv
	i--;
	free(array[i]);
	array[i] = NULL;

	return result;
}
