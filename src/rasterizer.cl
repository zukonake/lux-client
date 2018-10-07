void cast_ray(float2 src, float2 dst, write_only image2d_t out_img) {
    float2 ray   = dst - src;
    float2 a     = fabs(ray);
    float  steps = max(a.x, a.y);
    float2 dt    = ray / steps;
    float2 s_floor;
    float2 f     = fract(src, &s_floor);
    float  beg   = a.x > a.y && a.x > a.z ?
        ray.x < 0 ? f.x : 1.f - f.x : ray.y < 0 ? f.y : 1.f - f.y;

    float2 iter = src + beg * dt;
    float  alpha = 0.f;

    for(uint i = 0; i < ceil(steps); ++i) {
        float3 col   = 0.f;
        if(iter.y > 100.0f) {
            col    = 1.f;
            alpha += 0.1f;
        } else {
            col = 0.f;
        }
        if(alpha >= 1.f) break;
        iter += dt;
        col *= 1.f - fast_distance(floor(src), convert_float3(iter)) /
                     fast_distance(floor(src), floor(dst));
        write_imagef(out_img, iter, 
    }
i
