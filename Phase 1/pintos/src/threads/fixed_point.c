#include "threads/fixed_point.h"



int p = 17,q = 14;
int f = 16384; // pow(2,14)

/* calculate power */
int power(int x,int p){
	int i;
	int prev = 1;
	for(i=0;i<p;i++){
		prev = x * prev;
	}
	return prev;
}

/* initialize value of poth p and q which are how a fixed-point number is represented */
void init_fixed_point(int new_p , int new_q){
	p = new_p;
	q = new_q;
	f = power(2,q);
}

/* change int to a fixed-point representation */
fixed_point int_to_FP_num(int n){
	return n * f;
}

/* change fixed-point to int rounded to zero (truncated) */
int FP_num_to_int_trunc(fixed_point fp){
	return fp / f;
}

/* change fixed-point to int rounded to nearest */
int FP_num_to_int_round(fixed_point fp){
	if(fp < 0)
		return (fp - f/2)/f;
	return (fp + f/2)/f;
}

/* add two fixed-point numbers, returns (x + y) */
fixed_point add_FP_nums(fixed_point x , fixed_point y){
	return x + y;
}

/* add fixed-point number to int, returns (x + n) */
fixed_point add_int_to_FP_num(int n , fixed_point x){
	return x + n * f;
}

/* subtract two fixed-point numbers, returns (x - y) */
fixed_point subtract_FP_nums(fixed_point x , fixed_point y){
	return x - y;
}

/* subtract int from a fixed-point number, returns (x - n) */
fixed_point subtract_int_from_FP_num(int n , fixed_point x){
	return x - n * f;
}

/* multiply two fixed-point numbers, returns (x * y) */
fixed_point multiply_FP_nums(fixed_point x , fixed_point y){
	return ((int64_t) x) * y / f;
}

/* multiply fixed-point number by int, returns (x * n) */
fixed_point multiply_int_and_FP_num(int n , fixed_point x){
	return x * n;
}

/* divide two fixed-point numbers, returns (x / y) */
fixed_point divide_FP_nums(fixed_point x , fixed_point y){
	return ((int64_t) x) * f / y;
}

/* divide fixed-point number on int, returns (x / n) */
fixed_point divide_FP_num_by_int(int n , fixed_point x){
	return x / n;
}