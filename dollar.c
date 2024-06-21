#include "dollar.h"

#include <stdlib.h>
#include <emscripten.h>
#include <stdio.h>
#include <math.h>

#define TRUE 1
#define FALSE 0

#define MAX_NUM_POINTS 1024
#define TEMPLATE_NAME_SIZE 32
#define PI 3.14159265358979323846

typedef struct Point {
	f64 x, y;
} Point; 

typedef struct Stroke {
	Point *points;
} Stroke;

typedef struct Template {
	Stroke *stroke;
	char name[TEMPLATE_NAME_SIZE];	
} Template;

typedef struct Result {
	Template *template;
	f64 score;
} Result;

static void update_status(Result *result);
static f64 distance(Point *p1, Point *p2);
static f64 length(Stroke *stroke);
static Point centroid(Stroke *stroke);
static void translate(Stroke *stroke, Point *p);
static void resample(Stroke *stroke, u32 n);
static void vectorize(Stroke *stroke, b32 orientation_sensitive);
static f64 optimal_cosine_distance(Stroke *s1, Stroke *s2);

Template *templates;
Stroke *stroke;

EMSCRIPTEN_KEEPALIVE void load_templates()
{
	const char* path = "data.txt";
	u32 num_templates;

	FILE *file = fopen(path, "r");
		
	fscanf(file, "%d", &num_templates);
	templates = malloc((num_templates + 1) * sizeof(Template));

	EM_ASM({console.log('loaded ' + $0 + ' templates');}, num_templates);

	
	for (u32 i = 0; i < num_templates; i++)
	{
		Stroke *stroke = malloc(sizeof(Stroke));
		
		Point *points;
		u32 num_points;
	
		fscanf(file, "%s %d", templates[i].name, &num_points);
		points = malloc((num_points + 1) * sizeof(Point));

		for (u32 i2 = 0; i2 < num_points; i2++)
		{
			f64 x, y;
			
			fscanf(file, "%lf", &x);
			fscanf(file, "%lf", &y);

			points[i2] = (Point) {x, y};
		}

		points[num_points] = (Point) {NAN, NAN};

		stroke->points = points;

		resample(stroke, 16);
		vectorize(stroke, FALSE);
		
		templates[i].stroke = stroke;
	}

	templates[num_templates] = (Template) { 0 };

	fclose(file);
}

EMSCRIPTEN_KEEPALIVE void construct_stroke(f64 *values, i32 num_points)
{
	free(stroke);
	stroke = malloc(sizeof(Stroke));

	Point *points = malloc((num_points + 1) * sizeof(Point));
	
	for (u32 i = 0; i < num_points; i++)
	{
		points[i] = (Point) {values[i*2], values[i*2+1]};
	}

	points[num_points] = (Point) {NAN, NAN};

	stroke->points = points;

	/*
	for (u32 i = 0; i < num_points; i++)
	{
		EM_ASM({console.log($0 + ' ' + $1);}, stroke->points[i].x, stroke->points[i].y);
	}
	*/	
	
	resample(stroke, 16);
	vectorize(stroke, FALSE);
}

EMSCRIPTEN_KEEPALIVE void recognize()
{
	Result *result = malloc(sizeof(Result));	
	
	f64 max_score = 0;
	Template* max_score_template = templates;

	for (u32 i = 0; templates[i].name[0] != 0; i++)
	{
		f64 distance = optimal_cosine_distance(stroke, templates[i].stroke);
		f64 score = 1 / distance;
		
		if (score > max_score)
		{
			max_score = score;
			max_score_template = &templates[i];
		}
	}

	result->template = max_score_template;
	result->score = max_score;

	update_status(result);	
}

static void update_status(Result *result)
{
	EM_ASM({document.getElementById("status").innerHTML= UTF8ToString($0) + ": " + $1.toFixed(2);}, result->template->name, result->score);
}

static f64 distance(Point *p1, Point *p2)
{
	return sqrt(pow(p1->x - p2->x, 2) + pow(p1->y - p2->y, 2));
}

static f64 length(Stroke *stroke)
{
	f64 l = 0;

	for (u32 i = 1; !isnan(stroke->points[i].x); i++)
	{
		l += distance(&stroke->points[i-1],&stroke->points[i]);
	}

	return l;
}

static Point centroid(Stroke *stroke)
{
	f64 sum_x = 0, sum_y = 0;
	i32 i = 0;

	for (; !isnan(stroke->points[i].x); i++)
	{
		sum_x += stroke->points[i].x;
		sum_y += stroke->points[i].y;
	}

	return (Point) {sum_x / i, sum_y / i};
}

static void translate(Stroke *stroke, Point *p)
{
	for (u32 i = 0; !isnan(stroke->points[i].x); i++)
	{
		stroke->points[i].x -= p->x;
		stroke->points[i].y -= p->y;
	}
}
 
static void resample(Stroke *stroke, u32 n)
{
	f64 l = length(stroke) / (n - 1);
	f64 D = 0;

	Point points[n];
	points[0] = stroke->points[0];
	u32 i = 1, i_new = 1;

	while (!isnan(stroke->points[i].x))
	{
		f64 d = distance(&stroke->points[i-1], &stroke->points[i]);
		
		if (D+d >= l)
		{
			f64 new_x = stroke->points[i-1].x + ((l - D) / d) * (stroke->points[i].x - stroke->points[i-1].x);
			f64 new_y = stroke->points[i-1].y + ((l - D) / d) * (stroke->points[i].y - stroke->points[i-1].y);
			
			Point new_point = {new_x, new_y};
			points[i_new] = new_point;
			stroke->points[i-1] = new_point;

			D = 0;
			i_new++;
		}
		else
		{
			D += d;
			i++;
		}
	}

	if (i_new == n - 1)
	{
		points[i_new] = stroke->points[i-1];
		// printf("manually added last point (roundoff error)\n");
	}

	for (u32 i = 0; i < n; i++)
	{
		stroke->points[i] = points[i];
	}
	stroke->points[n] = (Point) {NAN, NAN};
}

static void vectorize(Stroke *stroke, b32 orientation_sensitive)
{
	Point c = centroid(stroke);
	translate(stroke, &c);

	f64 indicative_angle = atan2(stroke->points[0].y, stroke->points[0].x);
	f64 delta;

	if (orientation_sensitive)
	{
		f64 base_orientation = (PI / 4) * floor((indicative_angle + PI / 8) / (PI / 4));
		delta = base_orientation - indicative_angle;	
	}
	else
	{
		delta = -indicative_angle;
	}

	f64 sum = 0;

	for (u32 i = 0;!isnan(stroke->points[i].x); i++)
	{
		f64 new_x = stroke->points[i].x * cos(delta) - stroke->points[i].y * sin(delta);
		f64 new_y = stroke->points[i].y * cos(delta) + stroke->points[i].x * sin(delta);	
		
		stroke->points[i] = (Point) {new_x, new_y};
		sum += new_x * new_x + new_y * new_y;
	}
	
	f64 magnitude = sqrtf(sum);

	for(u32 i = 0; !isnan(stroke->points[i].x); i++)
	{
		stroke->points[i].x /= magnitude;
		stroke->points[i].y /= magnitude;
	}
}

static f64 optimal_cosine_distance(Stroke *s1, Stroke *s2)
{
	f64 a = 0, b = 0;
	
	for (u32 i = 0; !isnan(s1->points[i].x); i++)
	{
		a += s1->points[i].x * s2->points[i].x + s1->points[i].y * s2->points[i].y;
		b += s1->points[i].x * s2->points[i].y - s1->points[i].y * s2->points[i].x;
	}

	f64 angle = atan(b / a);
	f64 x = a * cos(angle) + b * sin(angle);

	if (x > 1.0)
	{
		x = 1.0;
		// printf("clamped acos argument to 1 (roundoff error)\n");
	}

	return acos(x);
}
