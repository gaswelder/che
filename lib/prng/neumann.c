/*
 * Middle-square method of generating pseudo-random numbers
 * invented by John von Neumann in 1949.
 */

/*
 * The numbers produced are 4-digit.
 */
pub int16_t nrg_next(int16_t n)
{
	/*
	 * Square the current value
	 * producing an 8-digit number
	 */
	int32_t s = n * n;

	/*
	 * The middle 4 digits are the next value.
	 */
	s /= 100;
	s %= 10000;
	return (int16_t) s;
}
