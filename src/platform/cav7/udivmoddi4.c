#include "kernel.h"

unsigned long long udivmoddi4(unsigned long long num, unsigned long long den, unsigned long long *rem_p) {
	unsigned long long quot = 0, qbit = 1;
	unsigned int shift = 0;

	if (den == 0) {
		return 0;
	}

	shift = (unsigned int)__builtin_clzll(den);

	den <<= shift;
	qbit <<= shift;

	while (qbit != (unsigned long) 0) {
		if (den <= num) {
			num -= den;
			quot += qbit;
		}

		den >>= 1;
		qbit >>= 1;
	}

	if (rem_p != NULL) {
		*rem_p = num;
	}

	return quot;
}

