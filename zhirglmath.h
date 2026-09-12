#include "zhirmath.h"

typedef struct {
    float x, y;
}vec2;

vec2 vec2_create(float x, float y) {
    vec2 result;

    result.x = x;
    result.y = y;

    return result;
}

//ВСЕ ФУНКЦИИ VEC3 (вектора)
typedef struct {
    float x, y ,z;
}vec3;

vec3 vec3_create(float x, float y, float z) {
    vec3 result;

    result.x = x;
    result.y = y;
    result.z = z;

    return result;
}

vec3 vec3_add(vec3 a, vec3 b) {
    return vec3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

vec3 vec3_sub(vec3 a, vec3 b) {
    return vec3_create(a.x - b.x, a.y - b.y, a.z - b.z);
}

vec3 vec3_mul(vec3 vector, float scalar) {
    return vec3_create(vector.x * scalar, vector.y * scalar, vector.z * scalar);
}

float vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 vec3_cross(vec3 a, vec3 b) {
    return vec3_create(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

float vec3_lenght(vec3 vector) {
    return sqrt(vec3_dot(vector, vector));
}

vec3 vec3_normalize(vec3 vector) {
    float lenght = vec3_lenght(vector);

    if (lenght == 0.0f)
        vec3_create(0.0f, 0.0f, 0.0f);

        return vec3_mul(vector, 1.0f / lenght);
}

typedef struct {
    float x, y, z, w;
}vec4;

//ВСЕ ФУНКЦИИ MAT4 (матрицы)
typedef struct {
    float m[4][4];
}mat4;

void mat4_set(mat4* matrix, int row, int column, float value) {
    matrix->m[column][row] = value;
}

float mat4_get(const mat4* matrix, int row, int column) {
    return matrix->m[column][row];
}

const float* mat4_data(const mat4* matrix) {
    return &matrix->m[0][0];
}

mat4 mat4_identify(void) {
    mat4 result = {0};

    mat4_set(&result, 0, 0, 1.0f);
    mat4_set(&result, 1, 1, 1.0f);
    mat4_set(&result, 2, 2, 1.0f);
    mat4_set(&result, 3, 3, 1.0f);

    return result;
}

mat4 mat4_zero(void) {
    mat4 result = {0};

    return result;
}

mat4 mat4_rotate_z(float angle) {
    mat4 result = {0};

    angle = angle*PI/180;

    mat4_set(&result, 0, 0, cos(angle));
    mat4_set(&result, 0, 1, -sin(angle));
    mat4_set(&result, 1, 0, sin(angle));
    mat4_set(&result, 1, 1, cos(angle));

    return result;
}

mat4 mat4_rotate_x(float angle) {
    mat4 result = {0};

    angle = angle*PI/180;

    mat4_set(&result, 1, 1, cos(angle));
    mat4_set(&result, 1, 2, -sin(angle));
    mat4_set(&result, 2, 1, sin(angle));
    mat4_set(&result, 2, 2, cos(angle));

    return result;
}

mat4 mat4_rotate_y(float angle) {
    mat4 result = {0};

    angle = angle*PI/180;

    mat4_set(&result, 0, 0, cos(angle));
    mat4_set(&result, 0, 2, sin(angle));
    mat4_set(&result, 2, 0, -sin(angle));
    mat4_set(&result, 2, 2, cos(angle));

    return result;
}

//ВСЕ ФУНКЦИИ MAT3 (матрицы)
typedef struct {
    float m[2][2];
}mat3;

void mat3_set(mat3* matrix, int row, int column, float value) {
    matrix->m[column][row] = value;
}

float mat3_get(const mat3* matrix, int row, int column) {
    return matrix->m[column][row];
}

const float* mat3_data(const mat3* matrix) {
    return &matrix->m[0][0];
}

mat3 mat3_identify(void) {
    mat3 result = {0};

    mat3_set(&result, 0, 0, 1.0f);
    mat3_set(&result, 1, 1, 1.0f);
    mat3_set(&result, 2, 2, 1.0f);

    return result;
}

mat3 mat3_zero(void) {
    mat3 result = {0};

    return result;
}

mat3 mat3_rotate(float angle) {
    mat3 result = {0};

    angle = angle*PI/180;

    mat3_set(&result, 0, 0, cos(angle));
    mat3_set(&result, 0, 1, -sin(angle));
    mat3_set(&result, 1, 0, sin(angle));
    mat3_set(&result, 1, 1, cos(angle));

    return result;
}

// mat3 mat3_rotate_vertex(Point *points, int size,float angle) {
//     mat3 result = mat3_rotate(angle);

//     for (int i = 0; i < size; i++){
//         x = points[i].x;
//         y = points[i].y;

        
//     }




// }