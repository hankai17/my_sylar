#include <iostream>
#include <cmath>

int main()
{
	uint64_t send_time = 1649860682199;
	float loss_delay = 38.25;
	
	uint64_t loss_time = send_time + loss_delay;
	std::cout << loss_time << std::endl;

	uint64_t loss_time1 = send_time + std::ceil(loss_delay);
	std::cout << loss_time1 << std::endl;

	uint64_t l1 = std::ceil(loss_delay);
	std::cout << l1 << std::endl;
	uint64_t l2 = l1 + send_time;
	std::cout << l2 << std::endl;

	return 0;
}
