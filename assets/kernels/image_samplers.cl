#ifndef IMAGE_SAMPLERS_CL
#define IMAGE_SAMPLERS_CL

__constant sampler_t NON_NORMALIZED_NEAREST_CLAMP_TO_EDGE_SAMPLER =
    CLK_NORMALIZED_COORDS_FALSE | 
    CLK_ADDRESS_CLAMP_TO_EDGE | 
    CLK_FILTER_NEAREST;

#endif // IMAGE_SAMPLERS_CL