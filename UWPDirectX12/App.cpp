#include "pch.h"
#include "App.h"

#include <ppltasks.h>

using namespace UWPDirectX12;

using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;

//DirectX 12 Application テンプレートについては、http://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409 で説明されています。

// main 関数は、IFrameworkView クラスを初期化する場合にのみ使用します。
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	auto direct3DApplicationSource = ref new Direct3DApplicationSource();
	CoreApplication::Run(direct3DApplicationSource);
	return 0;
}

IFrameworkView^ Direct3DApplicationSource::CreateView()
{
	return ref new App();
}

App::App() :
	m_windowClosed(false),
	m_windowVisible(true)
{
}

// IFrameworkView の作成時に最初のメソッドが呼び出されます。
void App::Initialize(CoreApplicationView^ applicationView)
{
	// アプリ ライフサイクルのイベント ハンドラーを登録します。この例にはアクティブ化が含まれているため、
	// CoreWindow をアクティブにし、ウィンドウで描画を開始できます。
	applicationView->Activated +=
		ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

	CoreApplication::Suspending +=
		ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);

	CoreApplication::Resuming +=
		ref new EventHandler<Platform::Object^>(this, &App::OnResuming);
}

//CoreWindow オブジェクトが作成 (または再作成) されるときに呼び出されます。
void App::SetWindow(CoreWindow^ window)
{
	window->SizeChanged += 
		ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &App::OnWindowSizeChanged);

	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &App::OnVisibilityChanged);

	window->Closed += 
		ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &App::OnWindowClosed);

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	currentDisplayInformation->DpiChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDpiChanged);

	currentDisplayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnOrientationChanged);

	DisplayInformation::DisplayContentsInvalidated +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDisplayContentsInvalidated);
}

// シーンのリソースを初期化するか、以前に保存したアプリ状態を読み込みます。
void App::Load(Platform::String^ entryPoint)
{
	if (m_main == nullptr)
	{
		m_main = std::unique_ptr<UWPDirectX12Main>(new UWPDirectX12Main());
	}
}

// このメソッドは、ウィンドウがアクティブになると、呼び出されます。
void App::Run()
{
	while (!m_windowClosed)
	{
		if (m_windowVisible)
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

			auto commandQueue = GetDeviceResources()->GetCommandQueue();
			PIXBeginEvent(commandQueue, 0, L"Update");
			{
				m_main->Update();
			}
			PIXEndEvent(commandQueue);

			PIXBeginEvent(commandQueue, 0, L"Render");
			{
				if (m_main->Render())
				{
					GetDeviceResources()->Present();
				}
			}
			PIXEndEvent(commandQueue);
		}
		else
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
		}
	}
}

// IFrameworkView で必要です。
// 終了イベントでは初期化解除は呼び出されません。アプリケーションが前景に表示されている間に
//IFrameworkView クラスが解体されると呼び出されます。
void App::Uninitialize()
{
}

// アプリケーション ライフサイクル イベント ハンドラー。

void App::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
	// Run() は CoreWindow がアクティブ化されるまで起動されません。
	CoreWindow::GetForCurrentThread()->Activate();
}

void App::OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args)
{
	// 遅延を要求した後にアプリケーションの状態を保存します。遅延状態を保持することは、
	//中断操作の実行でアプリケーションがビジー状態であることを示します。
	//遅延は制限なく保持されるわけではないことに注意してください。約 5 秒後に、
	// アプリケーションは強制終了されます。
	SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();

	create_task([this, deferral]()
	{
		//TODO: ここにコードを挿入します。
		m_main->OnSuspending();

		deferral->Complete();
	});
}

void App::OnResuming(Platform::Object^ sender, Platform::Object^ args)
{
	// 中断時にアンロードされたデータまたは状態を復元します。既定では、データと状態は
	// 中断から再開するときに保持されます。このイベントは、アプリが既に終了されている場合は
	//発生しません。

	//TODO: ここにコードを挿入します。
	m_main->OnResuming();
}

// ウィンドウ イベント ハンドラー。

void App::OnWindowSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args)
{
	GetDeviceResources()->SetLogicalSize(Size(sender->Bounds.Width, sender->Bounds.Height));
	m_main->OnWindowSizeChanged();
}

void App::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
	m_windowVisible = args->Visible;
}

void App::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
{
	m_windowClosed = true;
}

// DisplayInformation イベント ハンドラー。

void App::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
	GetDeviceResources()->SetDpi(sender->LogicalDpi);
	m_main->OnWindowSizeChanged();
}

void App::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
	GetDeviceResources()->SetCurrentOrientation(sender->CurrentOrientation);
	m_main->OnWindowSizeChanged();
}

void App::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
	GetDeviceResources()->ValidateDevice();
}

std::shared_ptr<DX::DeviceResources> App::GetDeviceResources()
{
	if (m_deviceResources != nullptr && m_deviceResources->IsDeviceRemoved())
	{
		//既存の D3D デバイスへのすべての参照をまず解放する必要があり、その後で新しいデバイスを
		//作成できます。

		m_deviceResources = nullptr;
		m_main->OnDeviceRemoved();
	}

	if (m_deviceResources == nullptr)
	{
		m_deviceResources = std::make_shared<DX::DeviceResources>();
		m_deviceResources->SetWindow(CoreWindow::GetForCurrentThread());
		m_main->CreateRenderers(m_deviceResources);
	}
	return m_deviceResources;
}
