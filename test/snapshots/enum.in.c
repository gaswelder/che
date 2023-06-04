enum {
	T_ERR = 257,
	T_TRUE,
	T_FALSE,
	T_NULL,
	T_STR,
	T_NUM,
	T1	= ((1<<(T_ERR))-1) ^ 0xFF	/* 0000 0000 */
};
