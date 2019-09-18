#pragma once

namespace HolographicEngine::Graphics
{
	// Constant buffer used to send the view-projection matrices to the shader pipeline.
	struct ViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 viewProjection[2];
	};

	// Assert that the constant buffer remains 16-byte aligned (best practice).
	static_assert((sizeof(ViewProjectionConstantBuffer) % (sizeof(float) * 4)) == 0, "ViewProjection constant buffer size must be 16-byte aligned (16 bytes is the length of four floats).");

	// Manages DirectX device resources that are specific to a holographic camera, such as the
	// back buffer, ViewProjection constant buffer, and viewport.
	class StereographicCameraResource
	{
	public:
		StereographicCameraResource(winrt::Windows::Graphics::Holographic::HolographicCamera const& holographicCamera);

		//Creates backbuffer resources for rendering.
		void CreateResources(
			ID3D11Device* deviceResources,
			winrt::Windows::Graphics::Holographic::HolographicCameraRenderingParameters const& cameraParameters
		);

		// Releases resources associated with a back buffer.
		void ReleaseResources(ID3D11DeviceContext* deviceResources);

		// Updates the view/projection constant buffer for a holographic camera.
		void UpdateViewProjection(
			ID3D11DeviceContext* deviceResources,
			winrt::Windows::Graphics::Holographic::HolographicCameraPose const& cameraPose,
			winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& coordinateSystem);

		// Gets the view-projection constant buffer for the HolographicCamera and attaches it
		// to the shader pipeline.
		bool AttachViewProjection(ID3D11DeviceContext* deviceResources);

		// Direct3D device resources.
		ID3D11RenderTargetView* GetRenderTargetView()     const { return m_d3dRenderTargetView.Get(); }
		ID3D11DepthStencilView* GetDepthStencilView()               const { return m_d3dDepthStencilView.Get(); }
		ID3D11Texture2D* GetColorFrameTexture2D()            const { return m_d3dBackBuffer.Get(); }
		ID3D11Texture2D* GetDepthStencilTexture2D()          const { return m_d3dDepthStencil.Get(); }
		D3D11_VIEWPORT          GetViewport()                       const { return m_d3dViewport; }
		DXGI_FORMAT             GetDXGIFormat()           const { return m_dxgiFormat; }

		// Render target properties.
		winrt::Windows::Foundation::Size GetRenderTargetSize()      const & { return m_d3dRenderTargetSize; }
		bool                    IsRenderingStereoscopic()           const { return m_isStereo; }

		// The holographic camera these resources are for.
		winrt::Windows::Graphics::Holographic::HolographicCamera const& GetHolographicCamera() const { return m_holographicCamera; }

	private:

		// Direct3D rendering objects. Required for 3D.

		//Rendering target for this camera.
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>              m_d3dRenderTargetView;

		//Depth and Stencil View for this camera.
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView>              m_d3dDepthStencilView;

		//Texture that holds the camera backbuffer.
		Microsoft::WRL::ComPtr<ID3D11Texture2D>                     m_d3dBackBuffer;

		//Texture that holds the camera depth and stencil.
		Microsoft::WRL::ComPtr<ID3D11Texture2D>                     m_d3dDepthStencil;

		// Device resource to store view and projection matrices.
		Microsoft::WRL::ComPtr<ID3D11Buffer>                        m_viewProjectionConstantBuffer;

		// Direct3D rendering properties.
		DXGI_FORMAT                                                 m_dxgiFormat;
		winrt::Windows::Foundation::Size                            m_d3dRenderTargetSize;
		D3D11_VIEWPORT                                              m_d3dViewport;

		// Indicates whether the camera supports stereoscopic rendering.
		bool                                                        m_isStereo = false;

		// Indicates whether this camera has a pending frame.
		bool                                                        m_framePending = false;

		// Pointer to the holographic camera these resources are for.
		winrt::Windows::Graphics::Holographic::HolographicCamera    m_holographicCamera{ nullptr };
	};
}