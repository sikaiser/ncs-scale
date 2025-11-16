#include <zephyr/kernel.h>

#include "channels.h"

int main(void)
{
        zbus_print_channels_and_observers();
	return 0;
}
