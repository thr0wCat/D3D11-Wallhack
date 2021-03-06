#include "DrawNumber.h"
#include "DrawNumberPS.h"
#include "DrawNumberVS.h"
#include <cassert>

#include "readimage.h"
#include <unordered_map>

image                   g_numTextImage;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(ptr) do { \
        if(ptr) { \
            ptr->Release(); \
            ptr = nullptr; \
        } \
    } while(0)
#endif

struct DrawNumberCache
{
    ID3D11Device*               pDevice;
    ID3D11Texture2D*            pTexture;
    ID3D11ShaderResourceView*   pShaderResourceView;
    ID3D11SamplerState*         pSamplerState;
    ID3D11VertexShader*         pVertexShader;
    ID3D11PixelShader*          pPixelShader;
    ID3D11InputLayout*          pInputLayout;

    DrawNumberCache(ID3D11Device* p)
    {
        pDevice = nullptr;
        pTexture = nullptr;
        pShaderResourceView = nullptr;
        pSamplerState = nullptr;
        pVertexShader = nullptr;
        pPixelShader = nullptr;
        pInputLayout = nullptr;
        setup(p);
    }
    DrawNumberCache(const DrawNumberCache& that)
    {
        pDevice = that.pDevice;
        pTexture = that.pTexture;
        pShaderResourceView = that.pShaderResourceView;
        pSamplerState = that.pSamplerState;
        pVertexShader = that.pVertexShader;
        pPixelShader = that.pPixelShader;
        pInputLayout = that.pInputLayout;
        const_cast<DrawNumberCache&>(that).pDevice = nullptr;
        const_cast<DrawNumberCache&>(that).pTexture = nullptr;
        const_cast<DrawNumberCache&>(that).pShaderResourceView = nullptr;
        const_cast<DrawNumberCache&>(that).pSamplerState = nullptr;
        const_cast<DrawNumberCache&>(that).pVertexShader = nullptr;
        const_cast<DrawNumberCache&>(that).pPixelShader = nullptr;
        const_cast<DrawNumberCache&>(that).pInputLayout = nullptr;
    }
    ~DrawNumberCache()
    {
        SAFE_RELEASE(pTexture);
        SAFE_RELEASE(pShaderResourceView);
        SAFE_RELEASE(pSamplerState);
        SAFE_RELEASE(pInputLayout);
        SAFE_RELEASE(pVertexShader);
        SAFE_RELEASE(pPixelShader);
        SAFE_RELEASE(pDevice);
    }
    void beginDraw(ID3D11DeviceContext* pContext) const
    {
        assert(pContext);
        pContext->VSSetShader(pVertexShader, 0, 0);
        pContext->PSSetShader(pPixelShader, 0, 0);
        pContext->PSSetShaderResources(0, 1, &pShaderResourceView);
        pContext->PSSetSamplers(0, 1, &pSamplerState);
        pContext->IASetInputLayout(pInputLayout);
    }
    void endDraw(ID3D11DeviceContext* pContext) const
    {
        assert(pContext);
        pContext->VSSetShader(0, 0, 0);
        pContext->PSSetShader(0, 0, 0);
        ID3D11ShaderResourceView* pSrv = nullptr;
        pContext->PSSetShaderResources(0, 1, &pSrv);
        ID3D11SamplerState* pSs = nullptr;
        pContext->PSSetSamplers(0, 1, &pSs);
        pContext->IASetInputLayout(0);
    }

private:
    void setup(ID3D11Device* p)
    {
        assert(p);
        pDevice = p;
        pDevice->AddRef();

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = g_numTextImage.get_width();
        desc.Height = g_numTextImage.get_height();
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA subdata;
        subdata.pSysMem = g_numTextImage.get_data(0, 0);
        subdata.SysMemPitch = g_numTextImage.get_bytes_per_line();
        subdata.SysMemSlicePitch = 0;

        pDevice->CreateTexture2D(&desc, &subdata, &pTexture);
        assert(pTexture);

        pDevice->CreateShaderResourceView(pTexture, nullptr, &pShaderResourceView);
        assert(pShaderResourceView);

        D3D11_SAMPLER_DESC sampDesc;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.MipLODBias = 0.f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampDesc.BorderColor[0] = 0;
        sampDesc.BorderColor[1] = 0;
        sampDesc.BorderColor[2] = 0;
        sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0.f;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        pDevice->CreateSamplerState(&sampDesc, &pSamplerState);
        assert(pSamplerState);

        pDevice->CreateVertexShader(g_DrawNumberVS, sizeof(g_DrawNumberVS), nullptr, &pVertexShader);
        assert(pVertexShader);

        pDevice->CreatePixelShader(g_DrawNumberPS, sizeof(g_DrawNumberPS), nullptr, &pPixelShader);
        assert(pPixelShader);

        D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        pDevice->CreateInputLayout(layoutDesc, sizeof(layoutDesc) / sizeof(layoutDesc[0]), g_DrawNumberVS, sizeof(g_DrawNumberVS), &pInputLayout);
        assert(pInputLayout);
    }
};

typedef std::unordered_map<ID3D11Device*, DrawNumberCache> DrawNumberCacheMap;

DrawNumberCacheMap      g_drawNumberCacheMap;

static const byte       g_numImageSource[] =
{
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 
    0x00, 0x00, 0x00, 0x6e, 0x00, 0x00, 0x00, 0x12, 0x08, 0x02, 0x00, 0x00, 0x00, 0x35, 0xd9, 0x7c, 
    0x16, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0e, 0xc3, 0x00, 0x00, 0x0e, 
    0xc3, 0x01, 0xc7, 0x6f, 0xa8, 0x64, 0x00, 0x00, 0x0a, 0x4f, 0x69, 0x43, 0x43, 0x50, 0x50, 0x68, 
    0x6f, 0x74, 0x6f, 0x73, 0x68, 0x6f, 0x70, 0x20, 0x49, 0x43, 0x43, 0x20, 0x70, 0x72, 0x6f, 0x66, 
    0x69, 0x6c, 0x65, 0x00, 0x00, 0x78, 0xda, 0x9d, 0x53, 0x67, 0x54, 0x53, 0xe9, 0x16, 0x3d, 0xf7, 
    0xde, 0xf4, 0x42, 0x4b, 0x88, 0x80, 0x94, 0x4b, 0x6f, 0x52, 0x15, 0x08, 0x20, 0x52, 0x42, 0x8b, 
    0x80, 0x14, 0x91, 0x26, 0x2a, 0x21, 0x09, 0x10, 0x4a, 0x88, 0x21, 0xa1, 0xd9, 0x15, 0x51, 0xc1, 
    0x11, 0x45, 0x45, 0x04, 0x1b, 0xc8, 0xa0, 0x88, 0x03, 0x8e, 0x8e, 0x80, 0x8c, 0x15, 0x51, 0x2c, 
    0x0c, 0x8a, 0x0a, 0xd8, 0x07, 0xe4, 0x21, 0xa2, 0x8e, 0x83, 0xa3, 0x88, 0x8a, 0xca, 0xfb, 0xe1, 
    0x7b, 0xa3, 0x6b, 0xd6, 0xbc, 0xf7, 0xe6, 0xcd, 0xfe, 0xb5, 0xd7, 0x3e, 0xe7, 0xac, 0xf3, 0x9d, 
    0xb3, 0xcf, 0x07, 0xc0, 0x08, 0x0c, 0x96, 0x48, 0x33, 0x51, 0x35, 0x80, 0x0c, 0xa9, 0x42, 0x1e, 
    0x11, 0xe0, 0x83, 0xc7, 0xc4, 0xc6, 0xe1, 0xe4, 0x2e, 0x40, 0x81, 0x0a, 0x24, 0x70, 0x00, 0x10, 
    0x08, 0xb3, 0x64, 0x21, 0x73, 0xfd, 0x23, 0x01, 0x00, 0xf8, 0x7e, 0x3c, 0x3c, 0x2b, 0x22, 0xc0, 
    0x07, 0xbe, 0x00, 0x01, 0x78, 0xd3, 0x0b, 0x08, 0x00, 0xc0, 0x4d, 0x9b, 0xc0, 0x30, 0x1c, 0x87, 
    0xff, 0x0f, 0xea, 0x42, 0x99, 0x5c, 0x01, 0x80, 0x84, 0x01, 0xc0, 0x74, 0x91, 0x38, 0x4b, 0x08, 
    0x80, 0x14, 0x00, 0x40, 0x7a, 0x8e, 0x42, 0xa6, 0x00, 0x40, 0x46, 0x01, 0x80, 0x9d, 0x98, 0x26, 
    0x53, 0x00, 0xa0, 0x04, 0x00, 0x60, 0xcb, 0x63, 0x62, 0xe3, 0x00, 0x50, 0x2d, 0x00, 0x60, 0x27, 
    0x7f, 0xe6, 0xd3, 0x00, 0x80, 0x9d, 0xf8, 0x99, 0x7b, 0x01, 0x00, 0x5b, 0x94, 0x21, 0x15, 0x01, 
    0xa0, 0x91, 0x00, 0x20, 0x13, 0x65, 0x88, 0x44, 0x00, 0x68, 0x3b, 0x00, 0xac, 0xcf, 0x56, 0x8a, 
    0x45, 0x00, 0x58, 0x30, 0x00, 0x14, 0x66, 0x4b, 0xc4, 0x39, 0x00, 0xd8, 0x2d, 0x00, 0x30, 0x49, 
    0x57, 0x66, 0x48, 0x00, 0xb0, 0xb7, 0x00, 0xc0, 0xce, 0x10, 0x0b, 0xb2, 0x00, 0x08, 0x0c, 0x00, 
    0x30, 0x51, 0x88, 0x85, 0x29, 0x00, 0x04, 0x7b, 0x00, 0x60, 0xc8, 0x23, 0x23, 0x78, 0x00, 0x84, 
    0x99, 0x00, 0x14, 0x46, 0xf2, 0x57, 0x3c, 0xf1, 0x2b, 0xae, 0x10, 0xe7, 0x2a, 0x00, 0x00, 0x78, 
    0x99, 0xb2, 0x3c, 0xb9, 0x24, 0x39, 0x45, 0x81, 0x5b, 0x08, 0x2d, 0x71, 0x07, 0x57, 0x57, 0x2e, 
    0x1e, 0x28, 0xce, 0x49, 0x17, 0x2b, 0x14, 0x36, 0x61, 0x02, 0x61, 0x9a, 0x40, 0x2e, 0xc2, 0x79, 
    0x99, 0x19, 0x32, 0x81, 0x34, 0x0f, 0xe0, 0xf3, 0xcc, 0x00, 0x00, 0xa0, 0x91, 0x15, 0x11, 0xe0, 
    0x83, 0xf3, 0xfd, 0x78, 0xce, 0x0e, 0xae, 0xce, 0xce, 0x36, 0x8e, 0xb6, 0x0e, 0x5f, 0x2d, 0xea, 
    0xbf, 0x06, 0xff, 0x22, 0x62, 0x62, 0xe3, 0xfe, 0xe5, 0xcf, 0xab, 0x70, 0x40, 0x00, 0x00, 0xe1, 
    0x74, 0x7e, 0xd1, 0xfe, 0x2c, 0x2f, 0xb3, 0x1a, 0x80, 0x3b, 0x06, 0x80, 0x6d, 0xfe, 0xa2, 0x25, 
    0xee, 0x04, 0x68, 0x5e, 0x0b, 0xa0, 0x75, 0xf7, 0x8b, 0x66, 0xb2, 0x0f, 0x40, 0xb5, 0x00, 0xa0, 
    0xe9, 0xda, 0x57, 0xf3, 0x70, 0xf8, 0x7e, 0x3c, 0x3c, 0x45, 0xa1, 0x90, 0xb9, 0xd9, 0xd9, 0xe5, 
    0xe4, 0xe4, 0xd8, 0x4a, 0xc4, 0x42, 0x5b, 0x61, 0xca, 0x57, 0x7d, 0xfe, 0x67, 0xc2, 0x5f, 0xc0, 
    0x57, 0xfd, 0x6c, 0xf9, 0x7e, 0x3c, 0xfc, 0xf7, 0xf5, 0xe0, 0xbe, 0xe2, 0x24, 0x81, 0x32, 0x5d, 
    0x81, 0x47, 0x04, 0xf8, 0xe0, 0xc2, 0xcc, 0xf4, 0x4c, 0xa5, 0x1c, 0xcf, 0x92, 0x09, 0x84, 0x62, 
    0xdc, 0xe6, 0x8f, 0x47, 0xfc, 0xb7, 0x0b, 0xff, 0xfc, 0x1d, 0xd3, 0x22, 0xc4, 0x49, 0x62, 0xb9, 
    0x58, 0x2a, 0x14, 0xe3, 0x51, 0x12, 0x71, 0x8e, 0x44, 0x9a, 0x8c, 0xf3, 0x32, 0xa5, 0x22, 0x89, 
    0x42, 0x92, 0x29, 0xc5, 0x25, 0xd2, 0xff, 0x64, 0xe2, 0xdf, 0x2c, 0xfb, 0x03, 0x3e, 0xdf, 0x35, 
    0x00, 0xb0, 0x6a, 0x3e, 0x01, 0x7b, 0x91, 0x2d, 0xa8, 0x5d, 0x63, 0x03, 0xf6, 0x4b, 0x27, 0x10, 
    0x58, 0x74, 0xc0, 0xe2, 0xf7, 0x00, 0x00, 0xf2, 0xbb, 0x6f, 0xc1, 0xd4, 0x28, 0x08, 0x03, 0x80, 
    0x68, 0x83, 0xe1, 0xcf, 0x77, 0xff, 0xef, 0x3f, 0xfd, 0x47, 0xa0, 0x25, 0x00, 0x80, 0x66, 0x49, 
    0x92, 0x71, 0x00, 0x00, 0x5e, 0x44, 0x24, 0x2e, 0x54, 0xca, 0xb3, 0x3f, 0xc7, 0x08, 0x00, 0x00, 
    0x44, 0xa0, 0x81, 0x2a, 0xb0, 0x41, 0x1b, 0xf4, 0xc1, 0x18, 0x2c, 0xc0, 0x06, 0x1c, 0xc1, 0x05, 
    0xdc, 0xc1, 0x0b, 0xfc, 0x60, 0x36, 0x84, 0x42, 0x24, 0xc4, 0xc2, 0x42, 0x10, 0x42, 0x0a, 0x64, 
    0x80, 0x1c, 0x72, 0x60, 0x29, 0xac, 0x82, 0x42, 0x28, 0x86, 0xcd, 0xb0, 0x1d, 0x2a, 0x60, 0x2f, 
    0xd4, 0x40, 0x1d, 0x34, 0xc0, 0x51, 0x68, 0x86, 0x93, 0x70, 0x0e, 0x2e, 0xc2, 0x55, 0xb8, 0x0e, 
    0x3d, 0x70, 0x0f, 0xfa, 0x61, 0x08, 0x9e, 0xc1, 0x28, 0xbc, 0x81, 0x09, 0x04, 0x41, 0xc8, 0x08, 
    0x13, 0x61, 0x21, 0xda, 0x88, 0x01, 0x62, 0x8a, 0x58, 0x23, 0x8e, 0x08, 0x17, 0x99, 0x85, 0xf8, 
    0x21, 0xc1, 0x48, 0x04, 0x12, 0x8b, 0x24, 0x20, 0xc9, 0x88, 0x14, 0x51, 0x22, 0x4b, 0x91, 0x35, 
    0x48, 0x31, 0x52, 0x8a, 0x54, 0x20, 0x55, 0x48, 0x1d, 0xf2, 0x3d, 0x72, 0x02, 0x39, 0x87, 0x5c, 
    0x46, 0xba, 0x91, 0x3b, 0xc8, 0x00, 0x32, 0x82, 0xfc, 0x86, 0xbc, 0x47, 0x31, 0x94, 0x81, 0xb2, 
    0x51, 0x3d, 0xd4, 0x0c, 0xb5, 0x43, 0xb9, 0xa8, 0x37, 0x1a, 0x84, 0x46, 0xa2, 0x0b, 0xd0, 0x64, 
    0x74, 0x31, 0x9a, 0x8f, 0x16, 0xa0, 0x9b, 0xd0, 0x72, 0xb4, 0x1a, 0x3d, 0x8c, 0x36, 0xa1, 0xe7, 
    0xd0, 0xab, 0x68, 0x0f, 0xda, 0x8f, 0x3e, 0x43, 0xc7, 0x30, 0xc0, 0xe8, 0x18, 0x07, 0x33, 0xc4, 
    0x6c, 0x30, 0x2e, 0xc6, 0xc3, 0x42, 0xb1, 0x38, 0x2c, 0x09, 0x93, 0x63, 0xcb, 0xb1, 0x22, 0xac, 
    0x0c, 0xab, 0xc6, 0x1a, 0xb0, 0x56, 0xac, 0x03, 0xbb, 0x89, 0xf5, 0x63, 0xcf, 0xb1, 0x77, 0x04, 
    0x12, 0x81, 0x45, 0xc0, 0x09, 0x36, 0x04, 0x77, 0x42, 0x20, 0x61, 0x1e, 0x41, 0x48, 0x58, 0x4c, 
    0x58, 0x4e, 0xd8, 0x48, 0xa8, 0x20, 0x1c, 0x24, 0x34, 0x11, 0xda, 0x09, 0x37, 0x09, 0x03, 0x84, 
    0x51, 0xc2, 0x27, 0x22, 0x93, 0xa8, 0x4b, 0xb4, 0x26, 0xba, 0x11, 0xf9, 0xc4, 0x18, 0x62, 0x32, 
    0x31, 0x87, 0x58, 0x48, 0x2c, 0x23, 0xd6, 0x12, 0x8f, 0x13, 0x2f, 0x10, 0x7b, 0x88, 0x43, 0xc4, 
    0x37, 0x24, 0x12, 0x89, 0x43, 0x32, 0x27, 0xb9, 0x90, 0x02, 0x49, 0xb1, 0xa4, 0x54, 0xd2, 0x12, 
    0xd2, 0x46, 0xd2, 0x6e, 0x52, 0x23, 0xe9, 0x2c, 0xa9, 0x9b, 0x34, 0x48, 0x1a, 0x23, 0x93, 0xc9, 
    0xda, 0x64, 0x6b, 0xb2, 0x07, 0x39, 0x94, 0x2c, 0x20, 0x2b, 0xc8, 0x85, 0xe4, 0x9d, 0xe4, 0xc3, 
    0xe4, 0x33, 0xe4, 0x1b, 0xe4, 0x21, 0xf2, 0x5b, 0x0a, 0x9d, 0x62, 0x40, 0x71, 0xa4, 0xf8, 0x53, 
    0xe2, 0x28, 0x52, 0xca, 0x6a, 0x4a, 0x19, 0xe5, 0x10, 0xe5, 0x34, 0xe5, 0x06, 0x65, 0x98, 0x32, 
    0x41, 0x55, 0xa3, 0x9a, 0x52, 0xdd, 0xa8, 0xa1, 0x54, 0x11, 0x35, 0x8f, 0x5a, 0x42, 0xad, 0xa1, 
    0xb6, 0x52, 0xaf, 0x51, 0x87, 0xa8, 0x13, 0x34, 0x75, 0x9a, 0x39, 0xcd, 0x83, 0x16, 0x49, 0x4b, 
    0xa5, 0xad, 0xa2, 0x95, 0xd3, 0x1a, 0x68, 0x17, 0x68, 0xf7, 0x69, 0xaf, 0xe8, 0x74, 0xba, 0x11, 
    0xdd, 0x95, 0x1e, 0x4e, 0x97, 0xd0, 0x57, 0xd2, 0xcb, 0xe9, 0x47, 0xe8, 0x97, 0xe8, 0x03, 0xf4, 
    0x77, 0x0c, 0x0d, 0x86, 0x15, 0x83, 0xc7, 0x88, 0x67, 0x28, 0x19, 0x9b, 0x18, 0x07, 0x18, 0x67, 
    0x19, 0x77, 0x18, 0xaf, 0x98, 0x4c, 0xa6, 0x19, 0xd3, 0x8b, 0x19, 0xc7, 0x54, 0x30, 0x37, 0x31, 
    0xeb, 0x98, 0xe7, 0x99, 0x0f, 0x99, 0x6f, 0x55, 0x58, 0x2a, 0xb6, 0x2a, 0x7c, 0x15, 0x91, 0xca, 
    0x0a, 0x95, 0x4a, 0x95, 0x26, 0x95, 0x1b, 0x2a, 0x2f, 0x54, 0xa9, 0xaa, 0xa6, 0xaa, 0xde, 0xaa, 
    0x0b, 0x55, 0xf3, 0x55, 0xcb, 0x54, 0x8f, 0xa9, 0x5e, 0x53, 0x7d, 0xae, 0x46, 0x55, 0x33, 0x53, 
    0xe3, 0xa9, 0x09, 0xd4, 0x96, 0xab, 0x55, 0xaa, 0x9d, 0x50, 0xeb, 0x53, 0x1b, 0x53, 0x67, 0xa9, 
    0x3b, 0xa8, 0x87, 0xaa, 0x67, 0xa8, 0x6f, 0x54, 0x3f, 0xa4, 0x7e, 0x59, 0xfd, 0x89, 0x06, 0x59, 
    0xc3, 0x4c, 0xc3, 0x4f, 0x43, 0xa4, 0x51, 0xa0, 0xb1, 0x5f, 0xe3, 0xbc, 0xc6, 0x20, 0x0b, 0x63, 
    0x19, 0xb3, 0x78, 0x2c, 0x21, 0x6b, 0x0d, 0xab, 0x86, 0x75, 0x81, 0x35, 0xc4, 0x26, 0xb1, 0xcd, 
    0xd9, 0x7c, 0x76, 0x2a, 0xbb, 0x98, 0xfd, 0x1d, 0xbb, 0x8b, 0x3d, 0xaa, 0xa9, 0xa1, 0x39, 0x43, 
    0x33, 0x4a, 0x33, 0x57, 0xb3, 0x52, 0xf3, 0x94, 0x66, 0x3f, 0x07, 0xe3, 0x98, 0x71, 0xf8, 0x9c, 
    0x74, 0x4e, 0x09, 0xe7, 0x28, 0xa7, 0x97, 0xf3, 0x7e, 0x8a, 0xde, 0x14, 0xef, 0x29, 0xe2, 0x29, 
    0x1b, 0xa6, 0x34, 0x4c, 0xb9, 0x31, 0x65, 0x5c, 0x6b, 0xaa, 0x96, 0x97, 0x96, 0x58, 0xab, 0x48, 
    0xab, 0x51, 0xab, 0x47, 0xeb, 0xbd, 0x36, 0xae, 0xed, 0xa7, 0x9d, 0xa6, 0xbd, 0x45, 0xbb, 0x59, 
    0xfb, 0x81, 0x0e, 0x41, 0xc7, 0x4a, 0x27, 0x5c, 0x27, 0x47, 0x67, 0x8f, 0xce, 0x05, 0x9d, 0xe7, 
    0x53, 0xd9, 0x53, 0xdd, 0xa7, 0x0a, 0xa7, 0x16, 0x4d, 0x3d, 0x3a, 0xf5, 0xae, 0x2e, 0xaa, 0x6b, 
    0xa5, 0x1b, 0xa1, 0xbb, 0x44, 0x77, 0xbf, 0x6e, 0xa7, 0xee, 0x98, 0x9e, 0xbe, 0x5e, 0x80, 0x9e, 
    0x4c, 0x6f, 0xa7, 0xde, 0x79, 0xbd, 0xe7, 0xfa, 0x1c, 0x7d, 0x2f, 0xfd, 0x54, 0xfd, 0x6d, 0xfa, 
    0xa7, 0xf5, 0x47, 0x0c, 0x58, 0x06, 0xb3, 0x0c, 0x24, 0x06, 0xdb, 0x0c, 0xce, 0x18, 0x3c, 0xc5, 
    0x35, 0x71, 0x6f, 0x3c, 0x1d, 0x2f, 0xc7, 0xdb, 0xf1, 0x51, 0x43, 0x5d, 0xc3, 0x40, 0x43, 0xa5, 
    0x61, 0x95, 0x61, 0x97, 0xe1, 0x84, 0x91, 0xb9, 0xd1, 0x3c, 0xa3, 0xd5, 0x46, 0x8d, 0x46, 0x0f, 
    0x8c, 0x69, 0xc6, 0x5c, 0xe3, 0x24, 0xe3, 0x6d, 0xc6, 0x6d, 0xc6, 0xa3, 0x26, 0x06, 0x26, 0x21, 
    0x26, 0x4b, 0x4d, 0xea, 0x4d, 0xee, 0x9a, 0x52, 0x4d, 0xb9, 0xa6, 0x29, 0xa6, 0x3b, 0x4c, 0x3b, 
    0x4c, 0xc7, 0xcd, 0xcc, 0xcd, 0xa2, 0xcd, 0xd6, 0x99, 0x35, 0x9b, 0x3d, 0x31, 0xd7, 0x32, 0xe7, 
    0x9b, 0xe7, 0x9b, 0xd7, 0x9b, 0xdf, 0xb7, 0x60, 0x5a, 0x78, 0x5a, 0x2c, 0xb6, 0xa8, 0xb6, 0xb8, 
    0x65, 0x49, 0xb2, 0xe4, 0x5a, 0xa6, 0x59, 0xee, 0xb6, 0xbc, 0x6e, 0x85, 0x5a, 0x39, 0x59, 0xa5, 
    0x58, 0x55, 0x5a, 0x5d, 0xb3, 0x46, 0xad, 0x9d, 0xad, 0x25, 0xd6, 0xbb, 0xad, 0xbb, 0xa7, 0x11, 
    0xa7, 0xb9, 0x4e, 0x93, 0x4e, 0xab, 0x9e, 0xd6, 0x67, 0xc3, 0xb0, 0xf1, 0xb6, 0xc9, 0xb6, 0xa9, 
    0xb7, 0x19, 0xb0, 0xe5, 0xd8, 0x06, 0xdb, 0xae, 0xb6, 0x6d, 0xb6, 0x7d, 0x61, 0x67, 0x62, 0x17, 
    0x67, 0xb7, 0xc5, 0xae, 0xc3, 0xee, 0x93, 0xbd, 0x93, 0x7d, 0xba, 0x7d, 0x8d, 0xfd, 0x3d, 0x07, 
    0x0d, 0x87, 0xd9, 0x0e, 0xab, 0x1d, 0x5a, 0x1d, 0x7e, 0x73, 0xb4, 0x72, 0x14, 0x3a, 0x56, 0x3a, 
    0xde, 0x9a, 0xce, 0x9c, 0xee, 0x3f, 0x7d, 0xc5, 0xf4, 0x96, 0xe9, 0x2f, 0x67, 0x58, 0xcf, 0x10, 
    0xcf, 0xd8, 0x33, 0xe3, 0xb6, 0x13, 0xcb, 0x29, 0xc4, 0x69, 0x9d, 0x53, 0x9b, 0xd3, 0x47, 0x67, 
    0x17, 0x67, 0xb9, 0x73, 0x83, 0xf3, 0x88, 0x8b, 0x89, 0x4b, 0x82, 0xcb, 0x2e, 0x97, 0x3e, 0x2e, 
    0x9b, 0x1b, 0xc6, 0xdd, 0xc8, 0xbd, 0xe4, 0x4a, 0x74, 0xf5, 0x71, 0x5d, 0xe1, 0x7a, 0xd2, 0xf5, 
    0x9d, 0x9b, 0xb3, 0x9b, 0xc2, 0xed, 0xa8, 0xdb, 0xaf, 0xee, 0x36, 0xee, 0x69, 0xee, 0x87, 0xdc, 
    0x9f, 0xcc, 0x34, 0x9f, 0x29, 0x9e, 0x59, 0x33, 0x73, 0xd0, 0xc3, 0xc8, 0x43, 0xe0, 0x51, 0xe5, 
    0xd1, 0x3f, 0x0b, 0x9f, 0x95, 0x30, 0x6b, 0xdf, 0xac, 0x7e, 0x4f, 0x43, 0x4f, 0x81, 0x67, 0xb5, 
    0xe7, 0x23, 0x2f, 0x63, 0x2f, 0x91, 0x57, 0xad, 0xd7, 0xb0, 0xb7, 0xa5, 0x77, 0xaa, 0xf7, 0x61, 
    0xef, 0x17, 0x3e, 0xf6, 0x3e, 0x72, 0x9f, 0xe3, 0x3e, 0xe3, 0x3c, 0x37, 0xde, 0x32, 0xde, 0x59, 
    0x5f, 0xcc, 0x37, 0xc0, 0xb7, 0xc8, 0xb7, 0xcb, 0x4f, 0xc3, 0x6f, 0x9e, 0x5f, 0x85, 0xdf, 0x43, 
    0x7f, 0x23, 0xff, 0x64, 0xff, 0x7a, 0xff, 0xd1, 0x00, 0xa7, 0x80, 0x25, 0x01, 0x67, 0x03, 0x89, 
    0x81, 0x41, 0x81, 0x5b, 0x02, 0xfb, 0xf8, 0x7a, 0x7c, 0x21, 0xbf, 0x8e, 0x3f, 0x3a, 0xdb, 0x65, 
    0xf6, 0xb2, 0xd9, 0xed, 0x41, 0x8c, 0xa0, 0xb9, 0x41, 0x15, 0x41, 0x8f, 0x82, 0xad, 0x82, 0xe5, 
    0xc1, 0xad, 0x21, 0x68, 0xc8, 0xec, 0x90, 0xad, 0x21, 0xf7, 0xe7, 0x98, 0xce, 0x91, 0xce, 0x69, 
    0x0e, 0x85, 0x50, 0x7e, 0xe8, 0xd6, 0xd0, 0x07, 0x61, 0xe6, 0x61, 0x8b, 0xc3, 0x7e, 0x0c, 0x27, 
    0x85, 0x87, 0x85, 0x57, 0x86, 0x3f, 0x8e, 0x70, 0x88, 0x58, 0x1a, 0xd1, 0x31, 0x97, 0x35, 0x77, 
    0xd1, 0xdc, 0x43, 0x73, 0xdf, 0x44, 0xfa, 0x44, 0x96, 0x44, 0xde, 0x9b, 0x67, 0x31, 0x4f, 0x39, 
    0xaf, 0x2d, 0x4a, 0x35, 0x2a, 0x3e, 0xaa, 0x2e, 0x6a, 0x3c, 0xda, 0x37, 0xba, 0x34, 0xba, 0x3f, 
    0xc6, 0x2e, 0x66, 0x59, 0xcc, 0xd5, 0x58, 0x9d, 0x58, 0x49, 0x6c, 0x4b, 0x1c, 0x39, 0x2e, 0x2a, 
    0xae, 0x36, 0x6e, 0x6c, 0xbe, 0xdf, 0xfc, 0xed, 0xf3, 0x87, 0xe2, 0x9d, 0xe2, 0x0b, 0xe3, 0x7b, 
    0x17, 0x98, 0x2f, 0xc8, 0x5d, 0x70, 0x79, 0xa1, 0xce, 0xc2, 0xf4, 0x85, 0xa7, 0x16, 0xa9, 0x2e, 
    0x12, 0x2c, 0x3a, 0x96, 0x40, 0x4c, 0x88, 0x4e, 0x38, 0x94, 0xf0, 0x41, 0x10, 0x2a, 0xa8, 0x16, 
    0x8c, 0x25, 0xf2, 0x13, 0x77, 0x25, 0x8e, 0x0a, 0x79, 0xc2, 0x1d, 0xc2, 0x67, 0x22, 0x2f, 0xd1, 
    0x36, 0xd1, 0x88, 0xd8, 0x43, 0x5c, 0x2a, 0x1e, 0x4e, 0xf2, 0x48, 0x2a, 0x4d, 0x7a, 0x92, 0xec, 
    0x91, 0xbc, 0x35, 0x79, 0x24, 0xc5, 0x33, 0xa5, 0x2c, 0xe5, 0xb9, 0x84, 0x27, 0xa9, 0x90, 0xbc, 
    0x4c, 0x0d, 0x4c, 0xdd, 0x9b, 0x3a, 0x9e, 0x16, 0x9a, 0x76, 0x20, 0x6d, 0x32, 0x3d, 0x3a, 0xbd, 
    0x31, 0x83, 0x92, 0x91, 0x90, 0x71, 0x42, 0xaa, 0x21, 0x4d, 0x93, 0xb6, 0x67, 0xea, 0x67, 0xe6, 
    0x66, 0x76, 0xcb, 0xac, 0x65, 0x85, 0xb2, 0xfe, 0xc5, 0x6e, 0x8b, 0xb7, 0x2f, 0x1e, 0x95, 0x07, 
    0xc9, 0x6b, 0xb3, 0x90, 0xac, 0x05, 0x59, 0x2d, 0x0a, 0xb6, 0x42, 0xa6, 0xe8, 0x54, 0x5a, 0x28, 
    0xd7, 0x2a, 0x07, 0xb2, 0x67, 0x65, 0x57, 0x66, 0xbf, 0xcd, 0x89, 0xca, 0x39, 0x96, 0xab, 0x9e, 
    0x2b, 0xcd, 0xed, 0xcc, 0xb3, 0xca, 0xdb, 0x90, 0x37, 0x9c, 0xef, 0x9f, 0xff, 0xed, 0x12, 0xc2, 
    0x12, 0xe1, 0x92, 0xb6, 0xa5, 0x86, 0x4b, 0x57, 0x2d, 0x1d, 0x58, 0xe6, 0xbd, 0xac, 0x6a, 0x39, 
    0xb2, 0x3c, 0x71, 0x79, 0xdb, 0x0a, 0xe3, 0x15, 0x05, 0x2b, 0x86, 0x56, 0x06, 0xac, 0x3c, 0xb8, 
    0x8a, 0xb6, 0x2a, 0x6d, 0xd5, 0x4f, 0xab, 0xed, 0x57, 0x97, 0xae, 0x7e, 0xbd, 0x26, 0x7a, 0x4d, 
    0x6b, 0x81, 0x5e, 0xc1, 0xca, 0x82, 0xc1, 0xb5, 0x01, 0x6b, 0xeb, 0x0b, 0x55, 0x0a, 0xe5, 0x85, 
    0x7d, 0xeb, 0xdc, 0xd7, 0xed, 0x5d, 0x4f, 0x58, 0x2f, 0x59, 0xdf, 0xb5, 0x61, 0xfa, 0x86, 0x9d, 
    0x1b, 0x3e, 0x15, 0x89, 0x8a, 0xae, 0x14, 0xdb, 0x17, 0x97, 0x15, 0x7f, 0xd8, 0x28, 0xdc, 0x78, 
    0xe5, 0x1b, 0x87, 0x6f, 0xca, 0xbf, 0x99, 0xdc, 0x94, 0xb4, 0xa9, 0xab, 0xc4, 0xb9, 0x64, 0xcf, 
    0x66, 0xd2, 0x66, 0xe9, 0xe6, 0xde, 0x2d, 0x9e, 0x5b, 0x0e, 0x96, 0xaa, 0x97, 0xe6, 0x97, 0x0e, 
    0x6e, 0x0d, 0xd9, 0xda, 0xb4, 0x0d, 0xdf, 0x56, 0xb4, 0xed, 0xf5, 0xf6, 0x45, 0xdb, 0x2f, 0x97, 
    0xcd, 0x28, 0xdb, 0xbb, 0x83, 0xb6, 0x43, 0xb9, 0xa3, 0xbf, 0x3c, 0xb8, 0xbc, 0x65, 0xa7, 0xc9, 
    0xce, 0xcd, 0x3b, 0x3f, 0x54, 0xa4, 0x54, 0xf4, 0x54, 0xfa, 0x54, 0x36, 0xee, 0xd2, 0xdd, 0xb5, 
    0x61, 0xd7, 0xf8, 0x6e, 0xd1, 0xee, 0x1b, 0x7b, 0xbc, 0xf6, 0x34, 0xec, 0xd5, 0xdb, 0x5b, 0xbc, 
    0xf7, 0xfd, 0x3e, 0xc9, 0xbe, 0xdb, 0x55, 0x01, 0x55, 0x4d, 0xd5, 0x66, 0xd5, 0x65, 0xfb, 0x49, 
    0xfb, 0xb3, 0xf7, 0x3f, 0xae, 0x89, 0xaa, 0xe9, 0xf8, 0x96, 0xfb, 0x6d, 0x5d, 0xad, 0x4e, 0x6d, 
    0x71, 0xed, 0xc7, 0x03, 0xd2, 0x03, 0xfd, 0x07, 0x23, 0x0e, 0xb6, 0xd7, 0xb9, 0xd4, 0xd5, 0x1d, 
    0xd2, 0x3d, 0x54, 0x52, 0x8f, 0xd6, 0x2b, 0xeb, 0x47, 0x0e, 0xc7, 0x1f, 0xbe, 0xfe, 0x9d, 0xef, 
    0x77, 0x2d, 0x0d, 0x36, 0x0d, 0x55, 0x8d, 0x9c, 0xc6, 0xe2, 0x23, 0x70, 0x44, 0x79, 0xe4, 0xe9, 
    0xf7, 0x09, 0xdf, 0xf7, 0x1e, 0x0d, 0x3a, 0xda, 0x76, 0x8c, 0x7b, 0xac, 0xe1, 0x07, 0xd3, 0x1f, 
    0x76, 0x1d, 0x67, 0x1d, 0x2f, 0x6a, 0x42, 0x9a, 0xf2, 0x9a, 0x46, 0x9b, 0x53, 0x9a, 0xfb, 0x5b, 
    0x62, 0x5b, 0xba, 0x4f, 0xcc, 0x3e, 0xd1, 0xd6, 0xea, 0xde, 0x7a, 0xfc, 0x47, 0xdb, 0x1f, 0x0f, 
    0x9c, 0x34, 0x3c, 0x59, 0x79, 0x4a, 0xf3, 0x54, 0xc9, 0x69, 0xda, 0xe9, 0x82, 0xd3, 0x93, 0x67, 
    0xf2, 0xcf, 0x8c, 0x9d, 0x95, 0x9d, 0x7d, 0x7e, 0x2e, 0xf9, 0xdc, 0x60, 0xdb, 0xa2, 0xb6, 0x7b, 
    0xe7, 0x63, 0xce, 0xdf, 0x6a, 0x0f, 0x6f, 0xef, 0xba, 0x10, 0x74, 0xe1, 0xd2, 0x45, 0xff, 0x8b, 
    0xe7, 0x3b, 0xbc, 0x3b, 0xce, 0x5c, 0xf2, 0xb8, 0x74, 0xf2, 0xb2, 0xdb, 0xe5, 0x13, 0x57, 0xb8, 
    0x57, 0x9a, 0xaf, 0x3a, 0x5f, 0x6d, 0xea, 0x74, 0xea, 0x3c, 0xfe, 0x93, 0xd3, 0x4f, 0xc7, 0xbb, 
    0x9c, 0xbb, 0x9a, 0xae, 0xb9, 0x5c, 0x6b, 0xb9, 0xee, 0x7a, 0xbd, 0xb5, 0x7b, 0x66, 0xf7, 0xe9, 
    0x1b, 0x9e, 0x37, 0xce, 0xdd, 0xf4, 0xbd, 0x79, 0xf1, 0x16, 0xff, 0xd6, 0xd5, 0x9e, 0x39, 0x3d, 
    0xdd, 0xbd, 0xf3, 0x7a, 0x6f, 0xf7, 0xc5, 0xf7, 0xf5, 0xdf, 0x16, 0xdd, 0x7e, 0x72, 0x27, 0xfd, 
    0xce, 0xcb, 0xbb, 0xd9, 0x77, 0x27, 0xee, 0xad, 0xbc, 0x4f, 0xbc, 0x5f, 0xf4, 0x40, 0xed, 0x41, 
    0xd9, 0x43, 0xdd, 0x87, 0xd5, 0x3f, 0x5b, 0xfe, 0xdc, 0xd8, 0xef, 0xdc, 0x7f, 0x6a, 0xc0, 0x77, 
    0xa0, 0xf3, 0xd1, 0xdc, 0x47, 0xf7, 0x06, 0x85, 0x83, 0xcf, 0xfe, 0x91, 0xf5, 0x8f, 0x0f, 0x43, 
    0x05, 0x8f, 0x99, 0x8f, 0xcb, 0x86, 0x0d, 0x86, 0xeb, 0x9e, 0x38, 0x3e, 0x39, 0x39, 0xe2, 0x3f, 
    0x72, 0xfd, 0xe9, 0xfc, 0xa7, 0x43, 0xcf, 0x64, 0xcf, 0x26, 0x9e, 0x17, 0xfe, 0xa2, 0xfe, 0xcb, 
    0xae, 0x17, 0x16, 0x2f, 0x7e, 0xf8, 0xd5, 0xeb, 0xd7, 0xce, 0xd1, 0x98, 0xd1, 0xa1, 0x97, 0xf2, 
    0x97, 0x93, 0xbf, 0x6d, 0x7c, 0xa5, 0xfd, 0xea, 0xc0, 0xeb, 0x19, 0xaf, 0xdb, 0xc6, 0xc2, 0xc6, 
    0x1e, 0xbe, 0xc9, 0x78, 0x33, 0x31, 0x5e, 0xf4, 0x56, 0xfb, 0xed, 0xc1, 0x77, 0xdc, 0x77, 0x1d, 
    0xef, 0xa3, 0xdf, 0x0f, 0x4f, 0xe4, 0x7c, 0x20, 0x7f, 0x28, 0xff, 0x68, 0xf9, 0xb1, 0xf5, 0x53, 
    0xd0, 0xa7, 0xfb, 0x93, 0x19, 0x93, 0x93, 0xff, 0x04, 0x03, 0x98, 0xf3, 0xfc, 0x63, 0x33, 0x2d, 
    0xdb, 0x00, 0x00, 0x00, 0x20, 0x63, 0x48, 0x52, 0x4d, 0x00, 0x00, 0x7a, 0x25, 0x00, 0x00, 0x80, 
    0x83, 0x00, 0x00, 0xf9, 0xff, 0x00, 0x00, 0x80, 0xe9, 0x00, 0x00, 0x75, 0x30, 0x00, 0x00, 0xea, 
    0x60, 0x00, 0x00, 0x3a, 0x98, 0x00, 0x00, 0x17, 0x6f, 0x92, 0x5f, 0xc5, 0x46, 0x00, 0x00, 0x02, 
    0x39, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xec, 0x98, 0xcb, 0x81, 0xc4, 0x20, 0x08, 0x40, 0xa7, 
    0x2e, 0x0a, 0xa2, 0x1e, 0xab, 0xa1, 0x19, 0x8a, 0x71, 0x0f, 0x89, 0x86, 0x9f, 0xa0, 0xf3, 0xb9, 
    0xad, 0xc7, 0x04, 0x11, 0x9e, 0x08, 0xe8, 0xeb, 0xf5, 0x3f, 0xbe, 0x3d, 0x00, 0x89, 0xb9, 0x5f, 
    0x83, 0x99, 0x10, 0x16, 0x62, 0x8d, 0x7b, 0xef, 0x84, 0xf6, 0x33, 0x60, 0x7b, 0xe6, 0xf7, 0xce, 
    0xd4, 0xb4, 0x06, 0x27, 0xc0, 0xd4, 0x20, 0xb1, 0xa6, 0x71, 0xef, 0xbd, 0xb3, 0x90, 0x41, 0xea, 
    0xf1, 0xb0, 0xc6, 0xe8, 0x95, 0xa6, 0x8a, 0xe5, 0x7c, 0xad, 0x03, 0x1a, 0x25, 0x6e, 0xdc, 0xa0, 
    0x92, 0xff, 0xb7, 0xe5, 0x6a, 0xb0, 0x77, 0x15, 0xa0, 0x51, 0x6c, 0x7d, 0x30, 0x5f, 0x49, 0xc5, 
    0x02, 0x7e, 0x4b, 0x8c, 0xd7, 0xc7, 0x28, 0xa5, 0x9f, 0x6f, 0xa0, 0x0c, 0x65, 0x2a, 0x23, 0x94, 
    0x13, 0x38, 0x00, 0xc1, 0xb0, 0xa7, 0x40, 0xe1, 0x51, 0x12, 0x13, 0x02, 0x58, 0x87, 0xa6, 0x9c, 
    0x11, 0x98, 0x4b, 0x44, 0xfb, 0x25, 0x96, 0xf2, 0x5e, 0x70, 0x16, 0xcb, 0x43, 0x69, 0x10, 0x4b, 
    0x45, 0xfc, 0x13, 0x8a, 0xe9, 0x6d, 0x72, 0x68, 0xda, 0x0d, 0xc3, 0x69, 0x9a, 0xfa, 0xe0, 0xb8, 
    0x24, 0x02, 0xb8, 0x32, 0xec, 0x79, 0x23, 0x9a, 0x9c, 0x85, 0x99, 0xe3, 0xc1, 0xaa, 0x62, 0x9a, 
    0x9f, 0x5f, 0xa2, 0xbc, 0x6d, 0xac, 0x6d, 0x73, 0x56, 0x5c, 0x4a, 0xa3, 0x05, 0xa4, 0xce, 0xd0, 
    0x25, 0x3d, 0x29, 0xf4, 0x29, 0xfc, 0x98, 0x7c, 0x3f, 0x46, 0x19, 0x09, 0x08, 0xe5, 0xe7, 0x28, 
    0x17, 0x59, 0x7c, 0x37, 0x24, 0x33, 0x0e, 0x12, 0xb5, 0x5d, 0x40, 0x7d, 0x15, 0xd2, 0x35, 0x8a, 
    0xef, 0xa0, 0x1c, 0x19, 0x40, 0xa5, 0x39, 0x35, 0xe3, 0x18, 0xe5, 0x3b, 0x24, 0x8d, 0xe7, 0xf6, 
    0xfc, 0x1a, 0x89, 0xd8, 0x75, 0x69, 0x68, 0xec, 0xf4, 0x87, 0x28, 0xf3, 0xe9, 0x77, 0x46, 0x93, 
    0x09, 0x2d, 0xce, 0x28, 0x59, 0xc6, 0x37, 0x3d, 0xc0, 0xed, 0x34, 0xa2, 0xac, 0xde, 0x69, 0xd2, 
    0x0c, 0xe0, 0xcf, 0xa2, 0xc5, 0x84, 0x00, 0xd7, 0x86, 0xb3, 0xce, 0x94, 0x19, 0xa8, 0x5f, 0xa0, 
    0x5c, 0x4a, 0x3d, 0x3c, 0xa4, 0x9b, 0xc1, 0x62, 0x35, 0x4a, 0x9d, 0xb5, 0x17, 0x2d, 0x82, 0x09, 
    0xb2, 0x24, 0x24, 0x9f, 0xfa, 0xa8, 0xf6, 0x4b, 0xfa, 0x30, 0xab, 0xda, 0x63, 0xf8, 0x38, 0x5e, 
    0x3f, 0x42, 0x19, 0xf4, 0x84, 0xd1, 0x09, 0x17, 0x32, 0x6b, 0x0e, 0x4b, 0x45, 0xb3, 0x2f, 0xbb, 
    0x6d, 0x19, 0x8b, 0xaa, 0x3e, 0xc2, 0x15, 0xd8, 0x22, 0x1f, 0x8c, 0x8f, 0x70, 0xf5, 0xa6, 0x7e, 
    0xfd, 0x65, 0x43, 0xf5, 0x13, 0x94, 0x48, 0x9b, 0x35, 0xde, 0xd5, 0x98, 0x23, 0x94, 0x27, 0x67, 
    0x2b, 0xb8, 0x51, 0x44, 0xd2, 0x9e, 0xee, 0xd8, 0x0c, 0x21, 0x28, 0x19, 0x77, 0xa6, 0x86, 0xe8, 
    0x2a, 0xd7, 0x77, 0x50, 0xee, 0x04, 0x64, 0x71, 0xc2, 0xb6, 0x9b, 0xa9, 0xaa, 0xe2, 0x27, 0xe6, 
    0x26, 0x7d, 0x58, 0x56, 0x55, 0x6a, 0x2f, 0xbe, 0xd5, 0x0c, 0x1d, 0x71, 0xac, 0xda, 0xc0, 0x53, 
    0x94, 0x07, 0xd0, 0x8e, 0xa8, 0x57, 0xc7, 0xd0, 0xfc, 0xde, 0x68, 0xd1, 0xcb, 0x1f, 0xb0, 0x71, 
    0x19, 0x39, 0x01, 0xbf, 0x83, 0x52, 0xdb, 0xb2, 0x4b, 0x6d, 0xe9, 0x41, 0xb8, 0xb5, 0xb9, 0x1d, 
    0xde, 0xe9, 0xfa, 0xe2, 0x58, 0x9c, 0x8d, 0x8a, 0x23, 0x12, 0xdb, 0x7b, 0x23, 0xe7, 0x29, 0xd5, 
    0xba, 0x00, 0x8d, 0x99, 0x5a, 0x5a, 0x53, 0xfc, 0xbd, 0x8f, 0xfc, 0x6e, 0x25, 0x68, 0x46, 0xc6, 
    0x9e, 0x0a, 0xb2, 0xeb, 0xed, 0x53, 0xbc, 0xc3, 0xdd, 0x4b, 0xb2, 0x7d, 0xfe, 0x1c, 0x91, 0x94, 
    0x0d, 0x79, 0x51, 0xd8, 0x7d, 0xcd, 0x58, 0xa0, 0xac, 0x35, 0x44, 0xcb, 0xec, 0x85, 0x64, 0xb6, 
    0x48, 0xda, 0x91, 0x85, 0x94, 0xd3, 0x47, 0xb6, 0x0f, 0x51, 0x46, 0x6f, 0x6c, 0xf9, 0x8b, 0x83, 
    0x8b, 0x9e, 0x4d, 0x0d, 0xd0, 0x92, 0x27, 0xb0, 0x8d, 0xac, 0xa1, 0x2b, 0xb4, 0x7b, 0x09, 0x94, 
    0x57, 0x8c, 0xca, 0x85, 0xff, 0xf1, 0xf6, 0xf8, 0x1b, 0x00, 0x53, 0x87, 0x98, 0x2d, 0x10, 0xf4, 
    0xe9, 0xcd, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

DrawNumberTool::DrawNumberTool()
{
    if(!g_numTextImage.is_valid()) {
        if(!imageio::read_png_image(g_numTextImage, g_numImageSource, sizeof(g_numImageSource))) {
            MessageBoxA(NULL, "Load \"number.png\" failed.", "Error", MB_OK);
            return;
        }
    }
    assert(g_numTextImage.is_valid());
    m_bias = 0.f;
    int w = g_numTextImage.get_width();
    int h = g_numTextImage.get_height();
    m_numHeight = (float)h;
    m_numWidth = ((float)w - m_bias) / 10.f;
    m_color[0] = m_color[3] = 1.f;
    m_color[1] = m_color[2] = 0.f;
}

void DrawNumberTool::drawNumber(IDXGISwapChain* pSwapChain, int number) const
{
    assert(pSwapChain);
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice);
    pDevice->GetImmediateContext(&pContext);

    // try to retrieve swap chain dimensions
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    pSwapChain->GetDesc(&swapChainDesc);
    float width = (float)swapChainDesc.BufferDesc.Width;
    float height = (float)swapChainDesc.BufferDesc.Height;

    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    UINT nViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    pContext->RSGetViewports(&nViewports, viewports);
    if(nViewports > 0) {
        width = viewports[0].Width;
        height = viewports[0].Height;
    }

    auto f = g_drawNumberCacheMap.find(pDevice);
    if(f == g_drawNumberCacheMap.end()) {
        f = g_drawNumberCacheMap.insert(std::make_pair(pDevice, DrawNumberCache(pDevice))).first;
    }
    const DrawNumberCache& cache = f->second;

    DrawNumberVertices vertices;
    createVertices(number, width, height, vertices);
    ID3D11Buffer* pVertexBuffer = createVertexBuffer(pDevice, vertices);
    ID3D11Buffer* pIndexBuffer = createIndexBuffer(pDevice, vertices);
    assert(pVertexBuffer && pIndexBuffer);

    cache.beginDraw(pContext);
    UINT stride = sizeof(DrawNumberVertex);
    UINT offset = 0;
    pContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
    pContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    int numVertices = (int)vertices.size();
    pContext->DrawIndexed(numVertices / 4 * 6, 0, 0);
    cache.endDraw(pContext);

    ID3D11Buffer* pEmptyBuffer = nullptr;
    UINT emptyStride = 0;
    pContext->IASetVertexBuffers(0, 1, &pEmptyBuffer, &emptyStride, &offset);
    pContext->IASetIndexBuffer(pEmptyBuffer, DXGI_FORMAT_UNKNOWN, 0);

    pVertexBuffer->Release();
    pIndexBuffer->Release();
}

void DrawNumberTool::createVertices(int number, float width, float height, DrawNumberVertices& vertices) const
{
    if(number < 0)
        number = 0;
    char strNumber[32];
    _snprintf(strNumber, sizeof(strNumber), "%d", number);
    float x = 15.f, y = 15.f;
    for(int i = 0; i < 32; i ++) {
        char digit = strNumber[i];
        if(digit == 0)
            break;
        createVerticesFor(digit, x, y, width, height, vertices);
        x += m_numWidth;
    }
}

static void convertToNDC(float width, float height, float coords[2])
{
    coords[0] = 2.f * coords[0] / width - 1.f;
    coords[1] = 1.f - 2.f * coords[1] / height;
}

static void convertToTex(float width, float height, float coords[2])
{
    coords[0] /= width;
    coords[1] /= height;
}

void DrawNumberTool::createVerticesFor(char digit, float x, float y, float width, float height, DrawNumberVertices& vertices) const
{
    bool isDigit = (digit >= 48 && digit <= 57);
    if(!isDigit)
        return;
    digit -= 48;
    float tx = m_numWidth * digit + m_bias;
    float ty = 0.f;
    float texWidth = (float)g_numTextImage.get_width();
    float texHeight = (float)g_numTextImage.get_height();
    DrawNumberVertex vertex;
    memcpy(vertex.color, m_color, sizeof(m_color));
    vertex.position[2] = 0.f;
    vertex.position[3] = 1.f;
    // top left
    vertex.position[0] = x;
    vertex.position[1] = y;
    convertToNDC(width, height, vertex.position);
    vertex.tex[0] = tx;
    vertex.tex[1] = ty;
    convertToTex(texWidth, texHeight, vertex.tex);
    vertices.push_back(vertex);
    // top right
    vertex.position[0] = x + m_numWidth;
    vertex.position[1] = y;
    convertToNDC(width, height, vertex.position);
    vertex.tex[0] = tx + m_numWidth;
    vertex.tex[1] = ty;
    convertToTex(texWidth, texHeight, vertex.tex);
    vertices.push_back(vertex);
    // bottom left
    vertex.position[0] = x;
    vertex.position[1] = y + m_numHeight;
    convertToNDC(width, height, vertex.position);
    vertex.tex[0] = tx;
    vertex.tex[1] = ty + m_numHeight;
    convertToTex(texWidth, texHeight, vertex.tex);
    vertices.push_back(vertex);
    // bottom right
    vertex.position[0] = x + m_numWidth;
    vertex.position[1] = y + m_numHeight;
    convertToNDC(width, height, vertex.position);
    vertex.tex[0] = tx + m_numWidth;
    vertex.tex[1] = ty + m_numHeight;
    convertToTex(texWidth, texHeight, vertex.tex);
    vertices.push_back(vertex);
}

ID3D11Buffer* DrawNumberTool::createVertexBuffer(ID3D11Device* pDevice, const DrawNumberVertices& vertices) const
{
    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.ByteWidth = (UINT)sizeof(DrawNumberVertex) * (UINT)vertices.size();
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA data;
    ZeroMemory(&data, sizeof(data));
    data.pSysMem = &vertices.front();
    ID3D11Buffer* p = nullptr;
    pDevice->CreateBuffer(&desc, &data, &p);
    return p;
}

ID3D11Buffer* DrawNumberTool::createIndexBuffer(ID3D11Device* pDevice, const DrawNumberVertices& vertices) const
{
    int nums = (int)vertices.size() / 4;
    if(nums <= 0)
        return nullptr;
    std::vector<int32_t> indices;
    indices.reserve(nums * 6);
    int32_t bias = 0;
    for(int i = 0; i < nums; i ++, bias += 4) {
        indices.push_back(bias);
        indices.push_back(bias + 1);
        indices.push_back(bias + 3);
        indices.push_back(bias);
        indices.push_back(bias + 3);
        indices.push_back(bias + 2);
    }
    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.ByteWidth = 4 * (UINT)indices.size();
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA data;
    ZeroMemory(&data, sizeof(data));
    data.pSysMem = &indices.front();
    ID3D11Buffer* p = nullptr;
    pDevice->CreateBuffer(&desc, &data, &p);
    return p;
}
