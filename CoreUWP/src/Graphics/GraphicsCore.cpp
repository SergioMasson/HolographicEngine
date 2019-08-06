#include "pch.h"
#include "GraphicsCore.h"
#include "StereographicCamera.h"

using namespace Math;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace std::placeholders;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

namespace GameCore
{
	//TODO: Do we really need this ?
	extern winrt::agile_ref<winrt::Windows::UI::Core::CoreWindow> g_window;
}

D3D_FEATURE_LEVEL g_D3DFeatureLevel = D3D_FEATURE_LEVEL_11_0;

ComPtr<ID3D11Device>                   m_d3dDevice;
ComPtr<ID3D11DeviceContext>            m_d3dContext;
ComPtr<IDXGIAdapter>                   m_dxgiAdapter;

// Direct2D factories.
ComPtr<ID2D1Factory2>                   m_d2dFactory;
ComPtr<IDWriteFactory2>                 m_dwriteFactory;

bool									m_supportsVprt;

// The holographic space provides a preferred DXGI adapter ID.
HolographicSpace m_holographicSpace = nullptr;

// Camera that will render to Hololens Device directly.
std::shared_ptr<StereographicCamera>	m_mainCamera;

// Direct3D interop objects.

winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_d3dInteropDevice;

void OnCameraAddedHandler(HolographicSpace const& space, HolographicSpaceCameraAddedEventArgs const& e)
{
	m_mainCamera = std::shared_ptr<StereographicCamera>(new StereographicCamera(e.Camera()));
}

void OnCameraRemovedHandler(HolographicSpace const& space, HolographicSpaceCameraRemovedEventArgs const& e) 
{
	//TODO: Handle case where more camera will be created.
}

void CreateDeviceIndependetResources() 
{
	// Initialize Direct2D resources.
	D2D1_FACTORY_OPTIONS options{};

#if defined(_DEBUG)
	// If the project is in a debug build, enable Direct2D debugging via SDK Layers.
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Initialize the Direct2D Factory.
	winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory2), &options, &m_d2dFactory));

	// Initialize the DirectWrite Factory.
	winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2), &m_dwriteFactory));
}

//Create ID3D11Device and ID3D11DeviceContext
void CreateDeviceResources() 
{
	// This flag adds support for surfaces with a different color channel ordering
	// than the API default. It is required for compatibility with Direct2D.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (Graphics::SdkLayersAvailable())
	{
		// If the project is in a debug build, enable debugging via SDK Layers with this flag.
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	// This array defines the set of DirectX hardware feature levels this app will support.
	// Note the ordering should be preserved.
	// Note that HoloLens supports feature level 11.1. The HoloLens emulator is also capable
	// of running on graphics cards starting with feature level 10.0.
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0
	};

	// Create the Direct3D 11 API device object and a corresponding context.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	D3D_DRIVER_TYPE driverType = m_dxgiAdapter == nullptr ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN;

	HRESULT hr = D3D11CreateDevice(
		m_dxgiAdapter.Get(),        // Either nullptr, or the primary adapter determined by Windows Holographic.
		driverType,                 // Create a device using the hardware graphics driver.
		0,                          // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
		creationFlags,              // Set debug and Direct2D compatibility flags.
		featureLevels,              // List of feature levels this app can support.
		ARRAYSIZE(featureLevels),   // Size of the list above.
		D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Runtime apps.
		&device,                    // Returns the Direct3D device created.
		&g_D3DFeatureLevel,         // Returns feature level of device created.
		&context                    // Returns the device immediate context.
	);

	if (FAILED(hr))
	{
		// If the initialization fails, fall back to the WARP device.
		// For more information on WARP, see:
		// http://go.microsoft.com/fwlink/?LinkId=286690
		winrt::check_hresult(
			D3D11CreateDevice(
				nullptr,              // Use the default DXGI adapter for WARP.
				D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
				0,
				creationFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				&device,
				&g_D3DFeatureLevel,
				&context
			));
	}

	// Store pointers to the Direct3D device and immediate context.
	winrt::check_hresult(device.As(&m_d3dDevice));
	winrt::check_hresult(context.As(&m_d3dContext));

	// Acquire the DXGI interface for the Direct3D device.
	ComPtr<IDXGIDevice3> dxgiDevice;
	winrt::check_hresult(m_d3dDevice.As(&dxgiDevice));

	// Wrap the native device using a WinRT interop object.
	winrt::com_ptr<::IInspectable> object;
	winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), reinterpret_cast<IInspectable * *>(winrt::put_abi(object))));

	m_d3dInteropDevice = object.as<IDirect3DDevice>();

	// Cache the DXGI adapter.
	// This is for the case of no preferred DXGI adapter, or fallback to WARP.
	ComPtr<IDXGIAdapter> dxgiAdapter;
	winrt::check_hresult(dxgiDevice->GetAdapter(&dxgiAdapter));
	winrt::check_hresult(dxgiAdapter.As(&m_dxgiAdapter));

	// Check for device support for the optional feature that allows setting the render target array index from the vertex shader stage.
	D3D11_FEATURE_DATA_D3D11_OPTIONS3 options;
	m_d3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &options, sizeof(options));

	if (options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer)
		m_supportsVprt = true;
}

void Graphics::Initialize(void)
{
	//Initialize Direct2D and IDWrite.
	CreateDeviceIndependetResources();

	//Create DeviceContext and ID3DDevice
	CreateDeviceResources();
}

void Graphics::CreateHolographicScene(winrt::Windows::UI::Core::CoreWindow const& window)
{
	//Create a Holographic space for this window.
	m_holographicSpace = HolographicSpace::CreateForCoreWindow(window);

	m_holographicSpace.SetDirect3D11Device(m_d3dInteropDevice);

	m_holographicSpace.CameraAdded(std::bind(OnCameraAddedHandler, _1, _2));
	m_holographicSpace.CameraRemoved(std::bind(OnCameraRemovedHandler, _1, _2));
}

void Graphics::Resize(uint32_t width, uint32_t height)
{
	//TODO: Add resize logic (if needed.)
}

void Graphics::Terminate(void)
{
	//TODO: Implement resource release.
}

void Graphics::Shutdown(void)
{
	//Implement resource release.
}

uint64_t Graphics::GetFrameCount(void)
{
	return uint64_t();
}

float Graphics::GetFrameTime(void)
{
	return 0.0f;
}

float Graphics::GetFrameRate(void)
{
	return 0.0f;
}
