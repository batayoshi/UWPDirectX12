#include "pch.h"
#include "UWPDirectX12_newMain.h"
#include "Common\DirectXHelper.h"

using namespace UWPDirectX12_new;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

//DirectX 12 Application テンプレートについては、http://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409 で説明されています。

// アプリケーションの読み込み時にアプリケーション資産を読み込んで初期化します。
UWPDirectX12_newMain::UWPDirectX12_newMain()
{
	// TODO: 既定の可変タイムステップ モード以外のモードが必要な場合は、タイマー設定を変更してください。
	// 例: 60 FPS 固定タイムステップ更新ロジックでは、次を呼び出します:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

//レンダラーを作成して、初期化します。
void UWPDirectX12_newMain::CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	// TODO: これをアプリのコンテンツの初期化で置き換えます。
	m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(deviceResources));

	OnWindowSizeChanged();
}

// アプリケーション状態をフレームごとに 1 回更新します。
void UWPDirectX12_newMain::Update()
{
	// シーン オブジェクトを更新します。
	m_timer.Tick([&]()
	{
		// TODO: これをアプリのコンテンツの更新関数で置き換えます。
		m_sceneRenderer->Update(m_timer);
	});
}

// 現在のアプリケーション状態に応じて現在のフレームをレンダリングします。
// フレームがレンダリングされ、表示準備が完了すると、true を返します。
bool UWPDirectX12_newMain::Render()
{
	// 初回更新前にレンダリングは行わないようにしてください。
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	// シーン オブジェクトをレンダリングします。
	// TODO: これをアプリのコンテンツのレンダリング関数で置き換えます。
	return m_sceneRenderer->Render();
}

//アプリケーションの状態の更新を、ウィンドウのサイズが変更されて (デバイスの方向の変更など)、
void UWPDirectX12_newMain::OnWindowSizeChanged()
{
	// TODO: これをアプリのコンテンツのサイズに依存する初期化で置き換えます。
	m_sceneRenderer->CreateWindowSizeDependentResources();
}

//一時停止していることをアプリに通知します。
void UWPDirectX12_newMain::OnSuspending()
{
	//TODO: これをアプリの一時停止ロジックで置き換えます。

	//一時停止したアプリは、プロセス存続期間管理によっていつでも中断される可能性があります。そのため、
	//アプリが中断した状態から再始動できるように、必要な状態を保存しておくことをお勧めします。

	m_sceneRenderer->SaveState();

	//アプリケーションで、再作成の容易なビデオ メモリ割り振りが使用されている場合、
	//そのメモリを解放して、他のアプリケーションで使用できるようにすることを検討してください。
}

//一時停止が解除されたことをアプリに通知します。
void UWPDirectX12_newMain::OnResuming()
{
	//TODO: これをアプリの再起動ロジックで置き換えます。
}

// デバイス リソースを解放する必要が生じたことをレンダラーに通知します。
void UWPDirectX12_newMain::OnDeviceRemoved()
{
	//TODO: 必要なアプリケーションまたはレンダラーの状態を変更し、レンダラー
	//と有効ではなくなったそのリソースを解放します。
	m_sceneRenderer->SaveState();
	m_sceneRenderer = nullptr;
}
