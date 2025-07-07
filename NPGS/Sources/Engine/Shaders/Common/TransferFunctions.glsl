#ifndef TRANSFERFUNCTIONS_GLSL_
#define TRANSFERFUNCTIONS_GLSL_

const mat3x3 kSrgbToBt2020 = mat3x3(
    0.6274, 0.0691, 0.0164,
    0.3293, 0.9195, 0.0880,
    0.0433, 0.0113, 0.8956
);

float PqEotf(float Vx)
{
    const float kMx1 = 2610.0 / 16384.0;
    const float kMx2 = 2523.0 / 4096.0 * 128.0;
    const float kCx1 = 3424.0 / 4096.0;
    const float kCx2 = 2413.0 / 4096.0 * 32.0;
    const float kCx3 = 2392.0 / 4096.0 * 32.0;

    Vx = pow(Vx, 1.0 / kMx2);

    if (Vx <= kCx1)
    {
        return 0.0;
    }

    return pow((Vx - kCx1) / (kCx2 - kCx3 * Vx), 1.0 / kMx1);
}

vec3 PqEotf(vec3 Color)
{
    return vec3(PqEotf(Color.r), PqEotf(Color.g), PqEotf(Color.b));
}

float InversePqEotf(float Vx)
{
    const float kMx1 = 2610.0 / 16384.0;
    const float kMx2 = 2523.0 / 4096.0 * 128.0;
    const float kCx1 = 3424.0 / 4096.0;
    const float kCx2 = 2413.0 / 4096.0 * 32.0;
    const float kCx3 = 2392.0 / 4096.0 * 32.0;

    Vx = pow(Vx, kMx1);

    return pow((kCx1 + kCx2 * Vx) / (1 + kCx3 * Vx), 1.0 / kMx2);
}

vec3 InversePqEotf(vec3 Color)
{
    return vec3(InversePqEotf(Color.r), InversePqEotf(Color.g), InversePqEotf(Color.b));
}

vec3 ScRgbToPqWithGamut(vec3 Color)
{
    vec3 Bt2020Color      = kSrgbToBt2020 * Color;
    vec3 Bright           = Bt2020Color * 80.0;
    vec3 NormalizedLinear = Bright / 10000.0;

    return InversePqEotf(NormalizedLinear);
}

#endif  // !TRANSFERFUNCTIONS_GLSL_
