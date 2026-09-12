#define PI 3.141592f

// #include "vertices.h"

static inline int abs_val(int x) {
    return (x < 0) ? -x : x;
}

float sqrt(float x) {
    if (x < 0.0) {
        return 0.0;
    }

    if (x == 0.0) {
        return 0.0;
    }

    float guess = x;

    for (int i = 0; i < 100; i++) {
        float next = 0.5 * (guess + x / guess);

        if (abs_val(next - guess) < 1e-12) {
            break;
        }

        guess = next;
    }

    return guess;
}

// Приведение угла к диапазону [-PI, PI]
float normalize_angle(float x) {
    while (x > PI) {
        x -= 2.0 * PI;
    }

    while (x < -PI) {
        x += 2.0 * PI;
    }

    return x;
}

// sin
float sin(float x) {
    x = normalize_angle(x);

    float term = x;
    float sum = x;

    for (int n = 1; n < 20; n++) {
        term *= -x * x / ((2.0 * n) * (2.0 * n + 1.0));
        sum += term;
    }

    return sum;
}

// cos
float cos(float x) {
    x = normalize_angle(x);

    float term = 1.0;
    float sum = 1.0;

    for (int n = 1; n < 20; n++) {
        term *= -x * x / ((2.0 * n - 1.0) * (2.0 * n));
        sum += term;
    }

    return sum;
}

//


// Экспонента
float exp(float x) {
    float term = 1.0;
    float sum = 1.0;

    for (int n = 1; n < 50; n++) {
        term *= x / n;
        sum += term;

        if (abs_val(term) < 1e-15) {
            break;
        }
    }

    return sum;
}

//логарифм
float log(float x) {
    if (x <= 0.0) {
        return 0;
    }

    float y = (x - 1.0) / (x + 1.0);
    float y_power = y;
    float sum = 0.0;

    for (int n = 0; n < 100; n++) {
        sum += y_power / (2.0 * n + 1.0);
        y_power *= y * y;
    }

    return 2.0 * sum;
}

float min_float(float a, float b){
    return (a < b) ? a : b;
}

float max_float(float a, float b){
    return (a < b) ? b : a;
}

int min_int(int a, int b){
    return (a < b) ? a : b;
}

int max_int(int a, int b){
    return (a < b) ? b : a;
}


// Степень для положительных оснований
float pow(float base, float exponent) {
    return exp(exponent * log(base));
}