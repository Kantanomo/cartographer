#pragma once

union point2d
{
	int16 v[2];
	struct { int16 x, y; };
};
ASSERT_STRUCT_SIZE(point2d, sizeof(int16) * 2);

union short_bounds
{
	int16 v[2];
	struct { int16 lower, upper; };
};
ASSERT_STRUCT_SIZE(short_bounds, sizeof(int16) * 2);

union rectangle2d
{
	int16 v[4];
	struct { int16 top, left, bottom, right; };
	struct { int16 y0, x0, y1, x1; };
};
ASSERT_STRUCT_SIZE(rectangle2d, sizeof(int16) * 4);


inline void point2d_scale(point2d* point, int16 scale)
{
	point->v[0] *= scale;
	point->v[1] *= scale;
}

inline int16 rectangle2d_width(const rectangle2d* rect)
{
	return rect->right - rect->left;
}

inline int16 rectangle2d_height(const rectangle2d* rect)
{
	return rect->bottom - rect->top;
}

inline rectangle2d* set_rectangle2d(rectangle2d* rectangle, int16 x0, int16 y0, int16 x1, int16 y1)
{
	rectangle->x0 = x0;
	rectangle->y0 = y0;
	rectangle->x1 = x1;
	rectangle->y1 = y1;
	return rectangle;
}

inline rectangle2d* offset_rectangle2d(rectangle2d* rectangle, int16 dx, int16 dy)
{
	rectangle->left += dx;
	rectangle->left += dx;
	rectangle->right += dx;
	rectangle->top += dy;
	rectangle->bottom += dy;
	return rectangle;
}


inline void rectangle2d_scale(rectangle2d* rect, int16 scale)
{
	rect->v[0] *= scale;
	rect->v[1] *= scale;
	rect->v[2] *= scale;
	rect->v[3] *= scale;
	return;
}

inline void rectangle2d_to_rect(const rectangle2d* rect2d, RECT* rect)
{
	const int16 width = rectangle2d_width(rect2d);
	const int16 height = rectangle2d_height(rect2d);
	*rect =
	{
		rect2d->left,
		rect2d->top,
		rect2d->left + width,
		rect2d->top + height
	};
	return;
}
