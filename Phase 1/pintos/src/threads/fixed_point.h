#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H
#include <stdint.h>

typedef int fixed_point;

int power(int x,int p);
/* initialize value of poth p and q which are how a fixed-point number is represented */
void init_fixed_point(int new_p , int new_q);

/* change int to a fixed-point representation */
fixed_point int_to_FP_num(int n);

/* change fixed-point to int rounded to zero (truncated) */
int FP_num_to_int_trunc(fixed_point fp);

/* change fixed-point to int rounded to nearest */
int FP_num_to_int_round(fixed_point fp);

/* add two fixed-point numbers, returns (x + y) */
fixed_point add_FP_nums(fixed_point x , fixed_point y);

/* add fixed-point number to int, returns (x + n) */
fixed_point add_int_to_FP_num(int n , fixed_point x);

/* subtract two fixed-point numbers, returns (x - y) */
fixed_point subtract_FP_nums(fixed_point x , fixed_point y);

/* subtract int from a fixed-point number, returns (x - n) */
fixed_point subtract_int_from_FP_num(int n , fixed_point x);

/* multiply two fixed-point numbers, returns (x * y) */
fixed_point multiply_FP_nums(fixed_point x , fixed_point y);

/* multiply fixed-point number by int, returns (x * n) */
fixed_point multiply_int_and_FP_num(int n , fixed_point x);

/* divide two fixed-point numbers, returns (x / y) */
fixed_point divide_FP_nums(fixed_point x , fixed_point y);

/* divide fixed-point number on int, returns (x / n) */
fixed_point divide_FP_num_by_int(int n , fixed_point x);


#endif /* threads/fixed-point.h */
