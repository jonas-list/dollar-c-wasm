#ifndef DOLLAR_H
#define DOLLAR_H

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef int i32;
typedef int b32;
typedef double f64;

typedef struct Point Point;
typedef struct Stroke Stroke;
typedef struct Template Template;
typedef struct Result Result;

void construct_stroke(f64 *values, i32 num_points);
void load_templates();
void recognize();

#endif
