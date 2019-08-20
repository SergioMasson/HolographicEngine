#include "pch.h"
#include "GameCore.h"
#include "Vector"
#include "FileUtility.h"
#include "Graphics/GraphicsCore.h"

using namespace Microsoft::WRL;
using namespace std;
using namespace concurrency;
using namespace HolographicEngine;

struct ModelConstantBuffer
{
	DirectX::XMFLOAT4X4 model;
};

// Assert that the constant buffer remains 16-byte aligned (best practice).
static_assert((sizeof(ModelConstantBuffer) % (sizeof(float) * 4)) == 0, "Model constant buffer size must be 16-byte aligned (16 bytes is the length of four floats).");

// Used to send per-vertex data to the vertex shader.
struct VertexPositionColor
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 color;
};

class SpiningCubeApp : public HolographicEngine::GameCore::IGameApp
{
public:

	SpiningCubeApp(void) {}

	virtual void Startup(void) override;
	virtual void Cleanup(void) override;

	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;

private:
	bool m_loadingComplete = false;
	bool m_usingVprtShaders = false;
	int m_indexCount = 0;

	// Direct3D resources for cube geometry.
	// TODO(Sergio): Abstract all those thing away using the Engine API.
	ComPtr<ID3D11InputLayout>       m_inputLayout{ nullptr };
	ComPtr<ID3D11Buffer>            m_vertexBuffer{ nullptr };
	ComPtr<ID3D11Buffer>            m_indexBuffer{ nullptr };
	ComPtr<ID3D11VertexShader>      m_vertexShader{ nullptr };
	ComPtr<ID3D11GeometryShader>    m_geometryShader{ nullptr };
	ComPtr<ID3D11PixelShader>       m_pixelShader{ nullptr };
	ComPtr<ID3D11Buffer>            m_modelConstantBuffer{ nullptr };

	// System resources for cube geometry.
	ModelConstantBuffer                             m_modelConstantBufferData;
};

CREATE_APPLICATION(SpiningCubeApp)

void SpiningCubeApp::Startup()
{
	m_usingVprtShaders = Graphics::g_supportsVprt;

	// On devices that do support the D3D11_FEATURE_D3D11_OPTIONS3::
	// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature
	// we can avoid using a pass-through geometry shader to set the render
	// target array index, thus avoiding any overhead that would be
	// incurred by setting the geometry shader stage.
	std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///VprtVertexShader.cso" : L"ms-appx:///VertexShader.cso";

	// Load shaders asynchronously.
	Utility::ByteArray vertexShaderBytes = HolographicEngine::Utility::ReadFileUWPSync(vertexShaderFileName);

	Utility::ByteArray pixelShaderBytes = HolographicEngine::Utility::ReadFileUWPSync(L"ms-appx:///PixelShader.cso");

	Utility::ByteArray geometryShaderBytes;

	if (!m_usingVprtShaders)
	{
		// Load the pass-through geometry shader.
		geometryShaderBytes = HolographicEngine::Utility::ReadFileUWPSync(L"ms-appx:///GeometryShader.cso");
	}

	winrt::check_hresult(Graphics::g_Device->CreateVertexShader(vertexShaderBytes->data(), vertexShaderBytes->size(), nullptr, &m_vertexShader));

	constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 2> vertexDesc =
	{ {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	} };

	winrt::check_hresult(Graphics::g_Device->CreateInputLayout(vertexDesc.data(), static_cast<UINT>(vertexDesc.size()), vertexShaderBytes->data(),
		static_cast<UINT>(vertexShaderBytes->size()),
		&m_inputLayout
	)
	);

	winrt::check_hresult(Graphics::g_Device->CreatePixelShader(pixelShaderBytes->data(), pixelShaderBytes->size(), nullptr, &m_pixelShader));

	const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

	winrt::check_hresult(Graphics::g_Device->CreateBuffer(&constantBufferDesc, nullptr, &m_modelConstantBuffer));

	if (!m_usingVprtShaders)
	{
		winrt::check_hresult(Graphics::g_Device->CreateGeometryShader(geometryShaderBytes->data(), geometryShaderBytes->size(), nullptr, &m_geometryShader));
		return;
	}

	// Load mesh vertices. Each vertex has a position and a color.
	// Note that the cube size has changed from the default DirectX app
	// template. Windows Holographic is scaled in meters, so to draw the
	// cube at a comfortable size we made the cube width 0.2 m (20 cm).
	static const std::array<VertexPositionColor, 8> cubeVertices =
	{ {
		{ DirectX::XMFLOAT3(-0.1f, -0.1f, -0.1f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f) },
		{ DirectX::XMFLOAT3(-0.1f, -0.1f,  0.1f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ DirectX::XMFLOAT3(-0.1f,  0.1f, -0.1f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ DirectX::XMFLOAT3(-0.1f,  0.1f,  0.1f), DirectX::XMFLOAT3(0.0f, 1.0f, 1.0f) },
		{ DirectX::XMFLOAT3(0.1f, -0.1f, -0.1f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ DirectX::XMFLOAT3(0.1f, -0.1f,  0.1f), DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f) },
		{ DirectX::XMFLOAT3(0.1f,  0.1f, -0.1f), DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f) },
		{ DirectX::XMFLOAT3(0.1f,  0.1f,  0.1f), DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f) },
	} };

	D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
	vertexBufferData.pSysMem = cubeVertices.data();
	vertexBufferData.SysMemPitch = 0;
	vertexBufferData.SysMemSlicePitch = 0;

	const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionColor) * static_cast<UINT>(cubeVertices.size()), D3D11_BIND_VERTEX_BUFFER);
	winrt::check_hresult(
		Graphics::g_Device->CreateBuffer(
			&vertexBufferDesc,
			&vertexBufferData,
			&m_vertexBuffer
		)
	);

	// Load mesh indices. Each trio of indices represents
	// a triangle to be rendered on the screen.
	// For example: 2,1,0 means that the vertices with indexes
	// 2, 1, and 0 from the vertex buffer compose the
	// first triangle of this mesh.
	// Note that the winding order is clockwise by default.
	constexpr std::array<unsigned short, 36> cubeIndices =
	{ {
		2,1,0, // -x
		2,3,1,

		6,4,5, // +x
		6,5,7,

		0,1,5, // -y
		0,5,4,

		2,6,7, // +y
		2,7,3,

		0,4,6, // -z
		0,6,2,

		1,3,7, // +z
		1,7,5,
	} };

	m_indexCount = static_cast<unsigned int>(cubeIndices.size());

	D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
	indexBufferData.pSysMem = cubeIndices.data();
	indexBufferData.SysMemPitch = 0;
	indexBufferData.SysMemSlicePitch = 0;
	CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned short) * static_cast<UINT>(cubeIndices.size()), D3D11_BIND_INDEX_BUFFER);
	winrt::check_hresult(Graphics::g_Device->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_indexBuffer));

	m_loadingComplete = true;
}

void SpiningCubeApp::Cleanup()
{
	//TODO: Add resource release code here.
}

void SpiningCubeApp::Update(float deltaT)
{
	//TODO: Add logic update here.
}

void SpiningCubeApp::RenderScene()
{
	// Loading is asynchronous. Resources must be created before drawing can occur.
	if (!m_loadingComplete)
	{
		return;
	}

	// TODO(Sergio): Abstract all those thing away using the Engine API.
	const auto context = HolographicEngine::Graphics::g_Context;

	// Each vertex is one instance of the VertexPositionColor struct.
	const UINT stride = sizeof(VertexPositionColor);
	const UINT offset = 0;

	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());

	// Attach the vertex shader.
	context->VSSetShader(m_vertexShader.Get(), nullptr, 0);

	// Apply the model constant buffer to the vertex shader.
	context->VSSetConstantBuffers(0, 1, m_modelConstantBuffer.GetAddressOf());

	if (!m_usingVprtShaders)
	{
		// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
		// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
		// a pass-through geometry shader is used to set the render target
		// array index.
		context->GSSetShader(m_geometryShader.Get(), nullptr, 0);
	}

	// Attach the pixel shader.
	context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

	// Draw the objects.
	context->DrawIndexedInstanced(m_indexCount, 2, 0, 0, 0);
}