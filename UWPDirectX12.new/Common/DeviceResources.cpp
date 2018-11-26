#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;

namespace DisplayMetrics
{
	// 高解像度ディスプレイは、レンダリングに多くの GPU とバッテリ電力を必要とします。
	// ゲームを完全な再現性を維持して毎秒 60 フレームでレンダリングしようとすると、
	// 高解像度の携帯電話などではバッテリの寿命の短さに悩まされる場合があります。
	// すべてのプラットフォームとフォーム ファクターにわたって完全な再現性を維持してのレンダリングは、
	// 慎重に検討して決定する必要があります。
	static const bool SupportHighResolutions = false;

	// "高解像度" ディスプレイを定義する既定のしきい値。しきい値を
	// 超え、SupportHighResolutions が false の場合は、ディメンションが
	// 50 % のスケールになります。
	static const float DpiThreshold = 192.0f;		// 標準のデスクトップの 200% 表示。
	static const float WidthThreshold = 1920.0f;	// 幅 1080p。
	static const float HeightThreshold = 1080.0f;	// 高さ 1080p。
};

// 画面の回転の計算に使用する定数。
namespace ScreenRotation
{
	// 0 度 Z 回転
	static const XMFLOAT4X4 Rotation0(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 90 度 Z 回転
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 180 度 Z 回転
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 270 度 Z 回転
	static const XMFLOAT4X4 Rotation270(
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// DeviceResources に対するコンストラクター。
DX::DeviceResources::DeviceResources() :
	m_currentFrame(0),
	m_screenViewport(),
	m_rtvDescriptorSize(0),
	m_fenceEvent(0),
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_deviceRemoved(false)
{
	ZeroMemory(m_fenceValues, sizeof(m_fenceValues));
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Direct3D デバイスに依存しないリソースを構成します。
void DX::DeviceResources::CreateDeviceIndependentResources()
{
}

// Direct3D デバイスを構成し、このハンドルとデバイスのコンテキストを保存します。
void DX::DeviceResources::CreateDeviceResources()
{
#if defined(_DEBUG)
	//プロジェクトがデバッグ ビルドにある場合、SDK レイヤーを介してデバッグを有効にします。
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}
#endif

	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));

	ComPtr<IDXGIAdapter1> adapter;
	GetHardwareAdapter(m_dxgiFactory.Get(), &adapter);

	//Direct3D 12 API デバイス オブジェクトを作成します
	HRESULT hr = D3D12CreateDevice(
		adapter.Get(),					// ハードウェア アダプター。
		D3D_FEATURE_LEVEL_11_0,			//このアプリがサポートできる最小機能レベル。
		IID_PPV_ARGS(&m_d3dDevice)		// 作成された Direct3D デバイスを返します。
		);

	if (FAILED(hr))
	{
		// 初期化が失敗した場合は、WARP デバイスにフォール バックします。
		// WARP の詳細については、次を参照してください: 
		// http://go.microsoft.com/fwlink/?LinkId=286690

		ComPtr<IDXGIAdapter> warpAdapter;
		m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)); 

		DX::ThrowIfFailed(
			D3D12CreateDevice(
				warpAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&m_d3dDevice)
				)
			);
	}

	//コマンド キューを作成します。
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	DX::ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	for (UINT n = 0; n < c_frameCount; n++)
	{
		DX::ThrowIfFailed(
			m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n]))
			);
	}

	//同期オブジェクトを作成します。
	DX::ThrowIfFailed(m_d3dDevice->CreateFence(m_fenceValues[m_currentFrame], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValues[m_currentFrame]++;

	m_fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
}

// これらのリソースは、ウィンドウ サイズが変更されるたびに再作成する必要があります。
void DX::DeviceResources::CreateWindowSizeDependentResources()
{
	// 以前の GPU 作業がすべて完了するまで待機します。
	WaitForGpu();

	//ウィンドウ サイズに特定の以前の内容を消去します。
	for (UINT n = 0; n < c_frameCount; n++)
	{
		m_renderTargets[n] = nullptr;
	}
	m_rtvHeap = nullptr;

	UpdateRenderTargetSize();

	// スワップ チェーンの幅と高さは、ウィンドウのネイティブ方向の幅と高さに
	// 基づいている必要があります。ウィンドウがネイティブではない場合は、
	// サイズを反転させる必要があります。
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	if (m_swapChain != nullptr)
	{
		// スワップ チェーンが既に存在する場合は、そのサイズを変更します。
		HRESULT hr = m_swapChain->ResizeBuffers(
			c_frameCount,
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// 何らかの理由でデバイスを削除した場合、新しいデバイスとスワップ チェーンを作成する必要があります。
			m_deviceRemoved = true;

			//このメソッドの実行を続けないでください。DeviceResources が消去された後に、再作成されます。
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// それ以外の場合は、既存の Direct3D デバイスと同じアダプターを使用して、新規作成します。
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);	// ウィンドウのサイズと一致させます。
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;			// これは、最も一般的なスワップ チェーンのフォーマットです。
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;							// マルチサンプリングは使いません。
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = c_frameCount;					//トリプル バッファリングを使用して遅延を最小化します。
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	//すべての Windows Universal アプリは、 _FLIP_ SwapEffects を使用する必要があります
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			m_dxgiFactory->CreateSwapChainForCoreWindow(
				m_commandQueue.Get(),								// スワップ チェーンには DirectX 12 のコマンド キューへの参照が必要です。
				reinterpret_cast<IUnknown*>(m_window.Get()),
				&swapChainDesc,
				nullptr,
				&swapChain
				)
			);

		DX::ThrowIfFailed(swapChain.As(&m_swapChain));
	}

	//スワップ チェーンのために適切な方向を設定して、回転したスワップ チェーンにレンダリングするための、
	// 3D マトリックス変換を生成します。
	//3D マトリックスが、丸めエラーを防止するために明示的に指定されます。

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
		);

	// スワップ チェーンのバック バッファーのレンダー ターゲット ビューを作成します。
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = c_frameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DX::ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
		m_rtvHeap->SetName(L"Render Target View Descriptor Heap");

		m_currentFrame = 0;
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (UINT n = 0; n < c_frameCount; n++)
		{
			DX::ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_d3dDevice->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvDescriptor);
			rtvDescriptor.Offset(m_rtvDescriptorSize);

			WCHAR name[25];
			swprintf_s(name, L"Render Target %d", n);
			m_renderTargets[n]->SetName(name);
		}
	}

	// 深度ステンシル ビューを作成します。
	{
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

		D3D12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_D32_FLOAT,
			static_cast<UINT>(m_d3dRenderTargetSize.Width),
			static_cast<UINT>(m_d3dRenderTargetSize.Height),
			1,
			0,
			1,
			0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthResourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&m_depthStencil)
			));

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		m_d3dDevice->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	//保留中の GPU 作業はすべて完了しています。追跡されているフェンス値を更新してください。
	//通知された最後の値になるときに行います。
	for (UINT n = 0; n < c_frameCount; n++)
	{
		m_fenceValues[n] = m_fenceValues[m_currentFrame];
	}

	// 3D レンダリング ビューポートをウィンドウ全体をターゲットにするように設定します。
	m_screenViewport = { 0.0f, 0.0f, m_d3dRenderTargetSize.Width, m_d3dRenderTargetSize.Height, 0.0f, 1.0f };
}

// レンダー ターゲットのディメンションを決定し、それをスケール ダウンするかどうかを判断します。
void DX::DeviceResources::UpdateRenderTargetSize()
{
	m_effectiveDpi = m_dpi;

	// 高解像度のデバイスのバッテリ寿命を上げるためには、より小さいレンダー ターゲットにレンダリングして
	// 出力が提示された場合は GPU で出力をスケーリングできるようにします。
	if (!DisplayMetrics::SupportHighResolutions && m_dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
		float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

		if (width > DisplayMetrics::WidthThreshold && height > DisplayMetrics::HeightThreshold)
		{
			// アプリをスケーリングするには有効な DPI を変更します。論理サイズは変更しません。
			m_effectiveDpi /= 2.0f;
		}
	}

	// 必要なレンダリング ターゲットのサイズをピクセル単位で計算します。
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// サイズ 0 の DirectX コンテンツが作成されることを防止します。
	m_outputSize.Width = max(m_outputSize.Width, 1);
	m_outputSize.Height = max(m_outputSize.Height, 1);
}

//このメソッドは、CoreWindow オブジェクトが作成 (または再作成) されるときに呼び出されます。
void DX::DeviceResources::SetWindow(CoreWindow^ window)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_window = window;
	m_logicalSize = Windows::Foundation::Size(window->Bounds.Width, window->Bounds.Height);
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	m_dpi = currentDisplayInformation->LogicalDpi;

	CreateWindowSizeDependentResources();
}

// このメソッドは、SizeChanged イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// このメソッドは、DpiChanged イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;

		// ディスプレイ DPI の変更時に、ウィンドウの論理サイズ (Dip 単位) も変更されるため、更新する必要があります。
		m_logicalSize = Windows::Foundation::Size(m_window->Bounds.Width, m_window->Bounds.Height);

		CreateWindowSizeDependentResources();
	}
}

// このメソッドは、OrientationChanged イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// このメソッドは、DisplayContentsInvalidated イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::ValidateDevice()
{
	//デバイスが作成された後に既定のアダプターが変更された、
	// またはこのデバイスが削除された場合は、D3D デバイスが有効でなくなります。

	// まず、デバイスが作成された時点から、アダプターに関する LUID を取得します。

	LUID previousAdapterLuid = m_d3dDevice->GetAdapterLuid();

	// 次に、現在の既定のアダプターの情報を取得します。

	ComPtr<IDXGIFactory2> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc(&currentDesc));

	// アダプターの LUID が一致しない、またはデバイスで LUID が削除されたとの報告があった場合は、
	// 新しい D3D デバイスを作成する必要があります。

	if (previousAdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousAdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		m_deviceRemoved = true;
	}
}

// スワップ チェーンの内容を画面に表示します。
void DX::DeviceResources::Present()
{
	// 最初の引数は、DXGI に VSync までブロックするよう指示し、アプリケーションを次の VSync まで
	// スリープさせます。これにより、画面に表示されることのないフレームをレンダリングして
	// サイクルを無駄にすることがなくなります。
	HRESULT hr = m_swapChain->Present(1, 0);

	//デバイスが切断またはドライバーの更新によって削除された場合は、
	// すべてのデバイス リソースを再作成する必要があります。
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		m_deviceRemoved = true;
	}
	else
	{
		DX::ThrowIfFailed(hr);

		MoveToNextFrame();
	}
}

//保留中の GPU 作業が完了するまで待機します。
void DX::DeviceResources::WaitForGpu()
{
	//キュー内のシグナル コマンドをスケジューリングします。
	DX::ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_currentFrame]));

	//フェンスがクロスされるまで待機します。
	DX::ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentFrame], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	//現在のフレームのフェンス値を増分します。
	m_fenceValues[m_currentFrame]++;
}

//次のフレームのレンダリングを準備します。
void DX::DeviceResources::MoveToNextFrame()
{
	//キュー内のシグナル コマンドをスケジューリングします。
	const UINT64 currentFenceValue = m_fenceValues[m_currentFrame];
	DX::ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	//フレーム インデックスを前に進めます。
	m_currentFrame = (m_currentFrame + 1) % c_frameCount;

	//次のフレームが開始可能かどうかを確認します。
	if (m_fence->GetCompletedValue() < m_fenceValues[m_currentFrame])
	{
		DX::ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentFrame], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	//次のフレームのフェンス値を設定します。
	m_fenceValues[m_currentFrame] = currentFenceValue + 1;
}

// このメソッドは、表示デバイスのネイティブの方向と、現在の表示方向との間での
// 回転を決定します。
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// メモ: DisplayOrientations 列挙型に他の値があっても、NativeOrientation として使用できるのは、
	// Landscape または Portrait のどちらかのみです。
	switch (m_nativeOrientation)
	{
	case DisplayOrientations::Landscape:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;
		}
		break;

	case DisplayOrientations::Portrait:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;
		}
		break;
	}
	return rotation;
}

// このメソッドでは、Direct3D 12 をサポートする利用可能な最初のハードウェア アダプターが取得されます。
// そのようなアダプターが見つからない場合、*ppAdapter が nullptr に設定されます。
void DX::DeviceResources::GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter)
{
	ComPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// 基本レンダリング ドライバーのアダプターは選択しないでください。
			continue;
		}

		// アダプターが Direct3D 12 をサポートしているかを確認します。ただし
		// 実際のデバイスはまだ作成しないでください。
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}
