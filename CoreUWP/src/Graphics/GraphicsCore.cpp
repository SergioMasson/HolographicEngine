#include "pch.h"
#include "GraphicsCore.h"
#include "GameCore.h"
#include "StereographicCameraResource.h"

using namespace HolographicEngine::Math;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace std::placeholders;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x != nullptr) { x->Release(); x = nullptr; }
#endif

namespace HolographicEngine::GameCore
{
	//TODO: Do we really need this ?
	extern winrt::agile_ref<winrt::Windows::UI::Core::CoreWindow> g_window;
}

namespace
{
	float s_FrameTime = 0.0f;
	uint64_t s_FrameIndex = 0;
	int64_t s_FrameStartTick = 0;
}

namespace HolographicEngine::Graphics
{
	ID3D11Device* g_Device;
	ID3D11DeviceContext* g_Context;
	bool				 g_supportsVprt;
}

D3D_FEATURE_LEVEL g_D3DFeatureLevel = D3D_FEATURE_LEVEL_11_0;

ComPtr<IDXGIAdapter>                   g_dxgiAdapter;

ComPtr<ID2D1Factory>                   g_d2dFactory;
ComPtr<IDWriteFactory>                 g_dwriteFactory;
ComPtr<ID3D11Device>                   g_d3dDevice;
ComPtr<ID3D11DeviceContext>            g_d3dContext;

bool								   g_canGetHolographicDisplayForCamera;
bool								   g_canCommitDirect3D11DepthBuffer;

HolographicSpace m_holographicSpace = nullptr;

// Back buffer resources, etc. for attached holographic cameras.
std::map<UINT32, std::unique_ptr<HolographicEngine::Graphics::StereographicCameraResource>>      g_cameraResources;
std::mutex												    g_cameraResourcesLock;

winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice g_winRTD3DDevice;

void CreateDeviceIndependetResources()
{
	// Initialize Direct2D resources.
	D2D1_FACTORY_OPTIONS options{};

#if defined(_DEBUG)
	// If the project is in a debug build, enable Direct2D debugging via SDK Layers.
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Initialize the Direct2D Factory.
	winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory2), &options, &g_d2dFactory));

	// Initialize the DirectWrite Factory.
	winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2), &g_dwriteFactory));
}

//Create ID3D11Device and ID3D11DeviceContext
void CreateDeviceResources()
{
	// This flag adds support for surfaces with a different color channel ordering
	// than the API default. It is required for compatibility with Direct2D.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (HolographicEngine::Graphics::SdkLayersAvailable())
	{
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0
	};

	D3D_DRIVER_TYPE driverType = g_dxgiAdapter == nullptr ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN;

	HRESULT hr = D3D11CreateDevice(
		g_dxgiAdapter.Get(),        // Either nullptr, or the primary adapter determined by Windows Holographic.
		driverType,                 // Create a device using the hardware graphics driver.
		0,                          // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
		creationFlags,              // Set debug and Direct2D compatibility flags.
		featureLevels,              // List of feature levels this app can support.
		ARRAYSIZE(featureLevels),   // Size of the list above.
		D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Runtime apps.
		&g_d3dDevice,                    // Returns the Direct3D device created.
		&g_D3DFeatureLevel,         // Returns feature level of device created.
		&g_d3dContext                    // Returns the device immediate context.
	);

	// If the initialization fails, fall back to the WARP device.
	if (FAILED(hr))
	{
		winrt::check_hresult(
			D3D11CreateDevice(
				nullptr,              // Use the default DXGI adapter for WARP.
				D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
				0,
				creationFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				&g_d3dDevice,
				&g_D3DFeatureLevel,
				&g_d3dContext
			));
	}

	// Acquire the DXGI interface for the Direct3D device.
	ComPtr<IDXGIDevice> dxgiDevice;
	winrt::check_hresult(g_d3dDevice.As(&dxgiDevice));

	// Wrap the native device using a WinRT interop object.
	winrt::com_ptr<::IInspectable> object;
	winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), reinterpret_cast<IInspectable**>(winrt::put_abi(object))));

	g_winRTD3DDevice = object.as<IDirect3DDevice>();

	// Cache the DXGI adapter.
	// This is for the case of no preferred DXGI adapter, or fallback to WARP.
	ComPtr<IDXGIAdapter> dxgiAdapter;
	winrt::check_hresult(dxgiDevice->GetAdapter(&dxgiAdapter));
	winrt::check_hresult(dxgiAdapter.As(&g_dxgiAdapter));

	// Check for device support for the optional feature that allows setting the render target array index from the vertex shader stage.
	D3D11_FEATURE_DATA_D3D11_OPTIONS3 options;
	g_d3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &options, sizeof(options));

	if (options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer)
		HolographicEngine::Graphics::g_supportsVprt = true;

	HolographicEngine::Graphics::g_Device = g_d3dDevice.Get();
	HolographicEngine::Graphics::g_Context = g_d3dContext.Get();
}

// Recreate all device resources and set them back to the current state.
// Locks the set of holographic camera resources until the function exits.
void HandleDeviceLost()
{
}

void HolographicEngine::Graphics::Initialize(void)
{
	//Initialize Direct2D and IDWrite.
	CreateDeviceIndependetResources();

	//Create DeviceContext and ID3DDevice
	CreateDeviceResources();
}

// Call this method when the app suspends. It provides a hint to the driver that the app
// is entering an idle state and that temporary buffers can be reclaimed for use by other apps.
void HolographicEngine::Graphics::Trim(void)
{
	g_d3dContext->ClearState();

	ComPtr<IDXGIDevice3> dxgiDevice;
	winrt::check_hresult(g_d3dDevice.As(&dxgiDevice));
	dxgiDevice->Trim();
}

void HolographicEngine::Graphics::AttachHolographicSpace(HolographicSpace const& space)
{
	//Create a Holographic space for this window.
	space.SetDirect3D11Device(g_winRTD3DDevice);
}

void HolographicEngine::Graphics::AddHolographicCamera(winrt::Windows::Graphics::Holographic::HolographicCamera const& camera)
{
	{
		std::lock_guard<std::mutex> guard(g_cameraResourcesLock);
		g_cameraResources[camera.Id()] = std::make_unique<StereographicCameraResource>(camera);
	}
}

void HolographicEngine::Graphics::EnsureHolographicCameraResources(winrt::Windows::Graphics::Holographic::HolographicFrame const& frame, winrt::Windows::Graphics::Holographic::HolographicFramePrediction const& prediction)
{
	{
		std::lock_guard<std::mutex> guard(g_cameraResourcesLock);

		for (HolographicCameraPose pose : prediction.CameraPoses())
		{
			HolographicCameraRenderingParameters renderingParameters = frame.GetRenderingParameters(pose);
			StereographicCameraResource* pCameraResources = g_cameraResources[pose.HolographicCamera().Id()].get();

			pCameraResources->CreateResources(g_Device, renderingParameters);
		}
	}
}

void HolographicEngine::Graphics::RemoveHolographicCamera(winrt::Windows::Graphics::Holographic::HolographicCamera const& camera)
{
	{
		std::lock_guard<std::mutex> guard(g_cameraResourcesLock);

		StereographicCameraResource* pCameraResources = g_cameraResources[camera.Id()].get();

		if (pCameraResources != nullptr)
		{
			pCameraResources->ReleaseResources(g_Context);
			g_cameraResources.erase(camera.Id());
		}
	}
}

void HolographicEngine::Graphics::Resize(uint32_t width, uint32_t height)
{
	//TODO: Add resize logic (if needed.)
}

void HolographicEngine::Graphics::Terminate(void)
{
	//TODO: Implement resource release.
}

void HolographicEngine::Graphics::Shutdown(void)
{
	SAFE_RELEASE(g_Device);
	SAFE_RELEASE(g_Context);

	g_dxgiAdapter->Release();
	g_d2dFactory->Release();
	g_dwriteFactory->Release();
}

void HolographicEngine::Graphics::Present(winrt::Windows::Graphics::Holographic::HolographicFrame const& frame)
{
	int64_t CurrentTick = SystemTime::GetCurrentTick();

	HolographicFramePresentResult presentResult = frame.PresentUsingCurrentPrediction();

	// The PresentUsingCurrentPrediction API will detect when the graphics device
	// changes or becomes invalid. When this happens, it is considered a Direct3D
	// device lost scenario.
	if (presentResult == HolographicFramePresentResult::DeviceRemoved)
	{
		// The Direct3D device, context, and resources should be recreated.
		HandleDeviceLost();
	}

	s_FrameTime = (float)SystemTime::TimeBetweenTicks(s_FrameStartTick, CurrentTick);

	s_FrameStartTick = CurrentTick;
	++s_FrameIndex;
}

bool HolographicEngine::Graphics::Render(GameCore::IGameApp& app, HolographicFrame const& holographicFrame, SpatialStationaryFrameOfReference const& m_stationaryReferenceFrame)
{
	holographicFrame.UpdateCurrentPrediction();

	HolographicFramePrediction prediction = holographicFrame.CurrentPrediction();

	bool atLeastOneCameraRendered = false;
	{
		std::lock_guard<std::mutex> guard(g_cameraResourcesLock);

		for (auto cameraPose : prediction.CameraPoses())
		{
			StereographicCameraResource* pCameraResources = g_cameraResources[cameraPose.HolographicCamera().Id()].get();

			ID3D11DepthStencilView* const depthStencilView = pCameraResources->GetDepthStencilView();
			ID3D11RenderTargetView* const targets[1] = { pCameraResources->GetRenderTargetView() };

			g_Context->OMSetRenderTargets(1, targets, depthStencilView);

			// Clear the back buffer and depth stencil view.
			if (g_canGetHolographicDisplayForCamera && cameraPose.HolographicCamera().Display().IsOpaque())
			{
				g_Context->ClearRenderTargetView(targets[0], DirectX::Colors::CornflowerBlue);
			}
			else
			{
				g_Context->ClearRenderTargetView(targets[0], DirectX::Colors::Transparent);
			}

			g_Context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			// The view and projection matrices for each holographic camera will change
			// every frame. This function refreshes the data in the constant buffer for
			// the holographic camera indicated by cameraPose.
			if (m_stationaryReferenceFrame)
			{
				pCameraResources->UpdateViewProjection(g_Context, cameraPose, m_stationaryReferenceFrame.CoordinateSystem());
			}

			// Attach the view/projection constant buffer for this camera to the graphics pipeline.
			bool cameraActive = pCameraResources->AttachViewProjection(g_Context);

			// Only render world-locked content when positional tracking is active.
			if (cameraActive)
			{
				// Draw the sample hologram.
				app.RenderScene();

				if (g_canCommitDirect3D11DepthBuffer)
				{
					// On versions of the platform that support the CommitDirect3D11DepthBuffer API, we can
					// provide the depth buffer to the system, and it will use depth information to stabilize
					// the image at a per-pixel level.
					HolographicCameraRenderingParameters renderingParameters = holographicFrame.GetRenderingParameters(cameraPose);

					ComPtr<ID3D11Texture2D> spDepthStencil = pCameraResources->GetDepthStencilTexture2D();

					// Direct3D interop APIs are used to provide the buffer to the WinRT API.
					ComPtr<IDXGIResource1> depthStencilResource;

					winrt::check_hresult(spDepthStencil.As(&depthStencilResource));

					ComPtr<IDXGISurface2> depthDxgiSurface;

					winrt::check_hresult(depthStencilResource->CreateSubresourceSurface(0, &depthDxgiSurface));

					winrt::com_ptr<::IInspectable> object;

					winrt::check_hresult(CreateDirect3D11SurfaceFromDXGISurface(depthDxgiSurface.Get(), reinterpret_cast<IInspectable**>(winrt::put_abi(object))));

					IDirect3DSurface depthD3DSurface = object.as<IDirect3DSurface>();

					// Calling CommitDirect3D11DepthBuffer causes the system to queue Direct3D commands to
					// read the depth buffer. It will then use that information to stabilize the image as
					// the HolographicFrame is presented.
					renderingParameters.CommitDirect3D11DepthBuffer(depthD3DSurface);
				}
			}
			atLeastOneCameraRendered = true;
		}
	}

	return atLeastOneCameraRendered;
}

uint64_t HolographicEngine::Graphics::GetFrameCount(void)
{
	return s_FrameIndex;
}

float HolographicEngine::Graphics::GetFrameTime(void)
{
	return s_FrameTime == 0.0f ? 0.0f : 1.0f / s_FrameTime;
}

float HolographicEngine::Graphics::GetFrameRate(void)
{
	return 0.0f;
}