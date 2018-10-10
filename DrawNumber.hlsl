Texture2D       numTexture : register(t0);
SamplerState    numSampler : register(s0);

struct VertexInput
{
    float4      position : POSITION;
    float4      color : COLOR;
    float2      tex : TEXCOORD;
};

struct PixelInput
{
    float4      position : SV_POSITION;
    float4      color : COLOR;
    float2      tex : TEXCOORD;
};

PixelInput DrawNumberVS(VertexInput input)
{
    PixelInput output;
    output.position = input.position;
    output.color = input.color;
    output.tex = input.tex;
    return output;
}

float4 DrawNumberPS(PixelInput input) : SV_TARGET
{
    float4 cr = numTexture.Sample(numSampler, input.tex);
    cr.w = cr.x;
    cr.xyz = input.color.xyz;
    return cr;
}
