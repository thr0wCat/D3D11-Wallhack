#pragma once

#include <d3d11.h>
#include <vector>

struct DrawNumberVertex
{
    float               position[4];
    float               color[4];
    float               tex[2];
};

typedef std::vector<DrawNumberVertex> DrawNumberVertices;

class DrawNumberTool
{
public:
    static DrawNumberTool& instance()
    {
        static DrawNumberTool inst;
        return inst;
    }
    void drawNumber(IDXGISwapChain* pSwapChain, int number) const;

private:
    DrawNumberTool();

private:
    float           m_bias;
    float           m_numWidth;
    float           m_numHeight;
    float           m_color[4];

private:
    void createVertices(int number, float width, float height, DrawNumberVertices& vertices) const;
    void createVerticesFor(char digit, float x, float y, float width, float height, DrawNumberVertices& vertices) const;
    ID3D11Buffer* createVertexBuffer(ID3D11Device* pDevice, const DrawNumberVertices& vertices) const;
    ID3D11Buffer* createIndexBuffer(ID3D11Device* pDevice, const DrawNumberVertices& vertices) const;
};
