[vk_binding(0)] RWTexture2D in_colour;
[vk_binding(1)] RWTexture2D in_normal;
[vk_binding(2)] RWTexture2D in_motionvector;
[vk_binding(3)] Sampler2D<uint16_t2> in_factor;
[vk_binding(4)] Sampler2D in_image_prev;
[vk_binding(5)] RWTexture2D<uint16_t2> out_factor;
[vk_binding(6)] RWTexture2D out_image;

static float denoiseStrength = 3.0f;

[shader("compute")]
[numthreads(16,16,1)]
void Denoise(uint3 threadId : SV_DispatchThreadID) {
    int2 pos = int2(threadId.xy);
    float2 offset[25];
    offset[0] = float2(-2,-2);
    offset[1] = float2(-1,-2);
    offset[2] = float2(0,-2);
    offset[3] = float2(1,-2);
    offset[4] = float2(2,-2);

    offset[5] = float2(-2,-1);
    offset[6] = float2(-1,-1);
    offset[7] = float2(0,-1);
    offset[8] = float2(1,-1);
    offset[9] = float2(2,-1);

    offset[10] = float2(-2,0);
    offset[11] = float2(-1,0);
    offset[12] = float2(0,0);
    offset[13] = float2(1,0);
    offset[14] = float2(2,0);

    offset[15] = float2(-2,1);
    offset[16] = float2(-1,1);
    offset[17] = float2(0,1);
    offset[18] = float2(1,1);
    offset[19] = float2(2,1);

    offset[20] = float2(-2,2);
    offset[21] = float2(-1,2);
    offset[22] = float2(0,2);
    offset[23] = float2(1,2);
    offset[24] = float2(2,2);


    float kernel[25];
    kernel[0] = 1.0f/256.0f;
    kernel[1] = 1.0f/64.0f;
    kernel[2] = 3.0f/128.0f;
    kernel[3] = 1.0f/64.0f;
    kernel[4] = 1.0f/256.0f;

    kernel[5] = 1.0f/64.0f;
    kernel[6] = 1.0f/16.0f;
    kernel[7] = 3.0f/32.0f;
    kernel[8] = 1.0f/16.0f;
    kernel[9] = 1.0f/64.0f;

    kernel[10] = 3.0f/128.0f;
    kernel[11] = 3.0f/32.0f;
    kernel[12] = 9.0f/64.0f;
    kernel[13] = 3.0f/32.0f;
    kernel[14] = 3.0f/128.0f;

    kernel[15] = 1.0f/64.0f;
    kernel[16] = 1.0f/16.0f;
    kernel[17] = 3.0f/32.0f;
    kernel[18] = 1.0f/16.0f;
    kernel[19] = 1.0f/64.0f;

    kernel[20] = 1.0f/256.0f;
    kernel[21] = 1.0f/64.0f;
    kernel[22] = 3.0f/128.0f;
    kernel[23] = 1.0f/64.0f;
    kernel[24] = 1.0f/256.0f;

    float4 mv = in_motionvector[pos];
    float2 output_dimensions;
    out_image.GetDimensions(output_dimensions.x, output_dimensions.y);
    mv.xy *= output_dimensions;
    float2 texcoord = float2(threadId.xy);

    //pos+(0.5,0.5) to get the center of the desired pixel
    float2 hist_pos = pos + float2(0.5, 0.5) - mv.xy;
    float4 hist_colour = in_image_prev.SampleLevel(hist_pos, 0);

    float3 sum = float3(0.0);
    float c_phi = 1.0;
    float n_phi = 0.5;
    float p_phi = 0.3;
    float4 brdf = in_colour[pos];
    float3 cval = brdf.rgb;
    float4 nval = in_normal[pos];
    float pval = brdf.a;

    float cum_w = 0.0;
    for(int denoise=1; denoise<denoiseStrength; denoise++)
    {
        for(int i=0; i<25; i++)
        {
            float2 uv = pos+offset[i]*denoise;

            float4 brdf_tmp = in_colour[int2(uv)];
            float3 ctmp = brdf_tmp.rgb;
            float3 t = cval - ctmp;
            float dist2 = dot(t,t);
            float c_w = min(exp(-(dist2)/c_phi), 1.0);

            float4 ntmp = in_normal[int2(uv)];
            float4 t4 = nval - ntmp;
            dist2 = max(dot(t4,t4), 0.0);
            float n_w = min(exp(-(dist2)/n_phi), 1.0);

            /*
            dist2 = pval - brdf_tmp.a;
            float p_w = min(exp(-(dist2)/p_phi), 1.0);
            */
            float p_w = pval == brdf_tmp.a && nval.w == ntmp.w ? 1.0 : 0.0;

            //float weight = c_w*n_w*p_w;
            float weight = n_w * p_w;
            sum += ctmp*weight*kernel[i];
            cum_w += weight*kernel[i];
        }
    }
    uint16_t2 hist_buffer = in_factor.SampleLevel(hist_pos, 0);
    uint16_t factor = min(uint16_t(15), hist_buffer.x);
    uint16_t id = hist_buffer.y;

    if (nval.w != hist_colour.w)
        factor = 0;
    if (uint16_t(pval) != id)
        factor = 0;
    float3 color = hist_colour.xyz * factor;
    color = (color + (sum / cum_w)) / float3(factor + 1);
    out_factor[pos] = uint16_t2(factor + 1, uint16_t(pval));
    out_image[pos] = float4(color, nval.w);
}

