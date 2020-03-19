#include "Model.h"
#include "GeometryGenerator.h"
#include "ShadowMap.h"
#include "PlayerController.h"

HWND hWnd = nullptr;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

UINT numFrameResource = 3;

enum class RenderLayer : int {
	opaque = 0,
	skinnedOpaque,
	skybox,
	count
};

struct RenderItem {
	DirectX::XMFLOAT4X4 worldMatrix;
	DirectX::XMFLOAT4X4 texTransform;

	int numFramesDirty = numFrameResource;

	int objCBIndex = -1;

	Material* material = nullptr;
	MeshGeometry* geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT indexCount = 0;
	UINT startIndexCount = 0;
	int baseVertexLocation = 0;

	int skinnedCBIndex = -1;
	SkinnedModelInstance* skinnedModelInst = nullptr;
};

void DrawSceneToShadow(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* descHeap, const std::vector<RenderItem*>& ritems, FrameResource* currFrameResource);
void DrawRenderItems(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* descHeap, const std::vector<RenderItem*>& ritems, FrameResource* currFrameResource);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	const UINT nFrameBackBufCount = 3u;

	int iWidth = 1920;
	int iHeight = 1080;
	UINT nFrameIndex = 0;
	UINT nFrame = 0;

	UINT nDXGIFactoryFlags = 0U;
	UINT nRTVDescriptorSize = 0U;

	MSG	msg = {};

	UINT64 n64FenceValue = 0ui64;
	HANDLE hFenceEvent = nullptr;

	CD3DX12_VIEWPORT stViewPort(0.0f, 0.0f, static_cast<float>(iWidth), static_cast<float>(iHeight));
	CD3DX12_RECT stScissorRect(0, 0, static_cast<LONG>(iWidth), static_cast<LONG>(iHeight));

	IDXGIFactory6* dxgi_factory;
	IDXGIAdapter1* d3d12_adapter;
	IDXGISwapChain1* dxgi_swapChainOld;
	IDXGISwapChain3* dxgi_swapChain;

	ID3D12Device* d3d12_device;
	ID3D12CommandQueue* d3d12_commandQueue;
	ID3D12CommandAllocator* d3d12_commandAllocator;
	ID3D12GraphicsCommandList* d3d12_commandList;

	ID3D12DescriptorHeap* d3d12_RTVDescriptorHeap;
	ID3D12Resource* d3d12_RTVBuffer[nFrameBackBufCount];
	ID3D12Resource* d3d12_depthBuffer;
	ID3D12DescriptorHeap* d3d12_depthDescriptorHeap;
	ID3D12DescriptorHeap* d3d12_samplerHeap;
	ID3D12DescriptorHeap* CBVSRVHeap;
	ID3D12Fence* d3d12_fence;

	IDirectInput8* input_system;
	IDirectInputDevice8* input_keyBoardDevice;
	char keyBuffersLastFrame[256] = {};

	/*辅助结构体*/
	UINT currFrameResourceIndex = 0;
	std::vector<std::unique_ptr<FrameResource>> frameResources;
	std::vector<std::unique_ptr<RenderItem>> ritems;
	std::vector<RenderItem*> ritemLayers[(int)RenderLayer::count];
	FrameResource* currFrameResource = nullptr;
	PassConstants mainPassCB;

	WNDCLASSEX wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_GLOBALCLASS;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wcex.lpszClassName = L"MainWindowClass";
	RegisterClassEx(&wcex);

	hWnd = CreateWindowW(L"MainWindowClass", L"Base 3D Project", WS_OVERLAPPED | WS_SYSMENU, CW_USEDEFAULT, 0, iWidth, iHeight, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

#if defined(_DEBUG)
	{
		ID3D12Debug* debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			// 打开附加的调试支持
			nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
		debugController->Release();
	}
#endif

	{
		if (FAILED(DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID * *)& input_system, NULL))) {
			MessageBox(NULL, L"DirectInputSystem创建失败", L"Error!!!", MB_OK);
			return FALSE;
		}

		if (FAILED(input_system->CreateDevice(GUID_SysKeyboard, &input_keyBoardDevice, NULL))) {
			MessageBox(NULL, L"键盘输入设备创建失败", L"Error!!!", MB_OK);
			return FALSE;
		}

		if (FAILED(input_keyBoardDevice->SetDataFormat(&c_dfDIKeyboard))) {
			MessageBox(NULL, L"转换键盘格式失败", L"Error!!!", MB_OK);
			return FALSE;
		}

		if (FAILED(input_keyBoardDevice->SetCooperativeLevel(hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE))) {
			MessageBox(NULL, L"设置设备协同类型失败", L"Error!!!", MB_OK);
			return FALSE;
		}
	}

	if (FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&dxgi_factory)))) {
		MessageBox(NULL, L"创建DXGI类厂失败", L"Error!!!", MB_OK);
		return FALSE;
	}

	if (FAILED(dxgi_factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER))) {
		MessageBox(NULL, L"关闭全屏功能失败", L"Error!!!", MB_OK);
		return FALSE;
	}

	for (UINT adapterIndex = 1; DXGI_ERROR_NOT_FOUND != dxgi_factory->EnumAdapters1(adapterIndex, &d3d12_adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc = {};
		d3d12_adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(d3d12_adapter, D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	if (FAILED(D3D12CreateDevice(d3d12_adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&d3d12_device)))) {
		MessageBox(NULL, L"创建D3D12类厂失败", L"Error!!!", MB_OK);
		return FALSE;
	}

	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (FAILED(d3d12_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3d12_commandQueue)))) {
			MessageBox(NULL, L"命令队列创建失败", L"Error!!!", MB_OK);
			return FALSE;
		}
	}

	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = nFrameBackBufCount;
		swapChainDesc.Width = iWidth;
		swapChainDesc.Height = iHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fallScreenDesc = {};
		fallScreenDesc.Windowed = true;
		fallScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER::DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
		fallScreenDesc.Scaling = DXGI_MODE_SCALING::DXGI_MODE_SCALING_CENTERED;
		fallScreenDesc.RefreshRate.Numerator = 60;
		fallScreenDesc.RefreshRate.Denominator = 1;

		if (FAILED(dxgi_factory->CreateSwapChainForHwnd(d3d12_commandQueue, hWnd, &swapChainDesc, &fallScreenDesc, nullptr, &dxgi_swapChainOld))) {
			MessageBox(NULL, L"交换链缓冲创建失败", L"Error!!!", MB_OK);
			return FALSE;
		}

		if (FAILED(dxgi_swapChainOld->QueryInterface(IID_PPV_ARGS(&dxgi_swapChain)))) {
			MessageBox(NULL, L"交换链缓冲更新失败", L"Error!!!", MB_OK);
			return FALSE;
		}

		nFrameIndex = dxgi_swapChain->GetCurrentBackBufferIndex();
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = nFrameBackBufCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		if (FAILED(d3d12_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&d3d12_RTVDescriptorHeap)))) {
			MessageBox(NULL, L"RTV描述符堆创建失败", L"Error!!!", MB_OK);
			return FALSE;
		}

		nRTVDescriptorSize = d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(d3d12_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < nFrameBackBufCount; i++)
		{
			if (FAILED(dxgi_swapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12_RTVBuffer[i])))) {
				MessageBox(NULL, L"获取交换链缓冲创建失败", L"Error!!!", MB_OK);
				return FALSE;
			}
			d3d12_device->CreateRenderTargetView(d3d12_RTVBuffer[i], nullptr, rtvHandle);
			rtvHandle.Offset(1, nRTVDescriptorSize);
		}
	}

	std::unordered_map<std::string, ID3D12RootSignature*> rootSignature;
	D3D12_FEATURE_DATA_ROOT_SIGNATURE desc_featureData;

	desc_featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(d3d12_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &desc_featureData, sizeof(desc_featureData))))
		desc_featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	/*Create Default Root Signature*/
	{
		CD3DX12_DESCRIPTOR_RANGE1 desc_range[4] = {};
		desc_range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
		desc_range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
		desc_range[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0);
		desc_range[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 1, 0);

		CD3DX12_ROOT_PARAMETER1 desc_parameter[8] = {};
		desc_parameter[0].InitAsDescriptorTable(1, &desc_range[0], D3D12_SHADER_VISIBILITY_PIXEL);
		desc_parameter[1].InitAsDescriptorTable(1, &desc_range[2], D3D12_SHADER_VISIBILITY_PIXEL);
		desc_parameter[2].InitAsDescriptorTable(1, &desc_range[3], D3D12_SHADER_VISIBILITY_PIXEL);
		desc_parameter[3].InitAsConstantBufferView(0, 0);
		desc_parameter[4].InitAsConstantBufferView(1, 0);
		desc_parameter[5].InitAsConstantBufferView(2, 0);
		desc_parameter[6].InitAsDescriptorTable(1, &desc_range[1], D3D12_SHADER_VISIBILITY_PIXEL);
		desc_parameter[7].InitAsConstantBufferView(3, 0);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc_rootSignature;
		desc_rootSignature.Init_1_1(8, desc_parameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ID3DBlob* signatureBlob;
		ID3DBlob* errorBlob;

		D3DX12SerializeVersionedRootSignature(&desc_rootSignature, desc_featureData.HighestVersion, &signatureBlob, &errorBlob);
		d3d12_device->CreateRootSignature(0x1, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature["default"]));
	
		signatureBlob->Release();
	}
	/*Create Shadow Root Signature*/
	{
		CD3DX12_ROOT_PARAMETER1 parameter[3] = {};
		parameter[0].InitAsConstantBufferView(0, 0);
		parameter[1].InitAsConstantBufferView(1, 0);
		parameter[2].InitAsConstantBufferView(3, 0);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc_rootSignature;
		desc_rootSignature.Init_1_1(3, parameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ID3DBlob* signatureBlob;
		ID3DBlob* errorBlob;

		D3DX12SerializeVersionedRootSignature(&desc_rootSignature, desc_featureData.HighestVersion, &signatureBlob, &errorBlob);
		d3d12_device->CreateRootSignature(0x1, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature["shadow"]));
	
		signatureBlob->Release();
	}

	std::unordered_map<std::string, ID3D12PipelineState*> pipelineStates;
	{
		/*创建默认管线*/
#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#else
		UINT compileFlags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#endif
		TCHAR vertexShaderCode[] = _T("VertexShader.hlsl");
		TCHAR pixelShaderCode[] = _T("PixelShader.hlsl");
		
		ID3DBlob* defaultVertexShader;
		ID3DBlob* defaultPixelShader;

		if (FAILED(D3DCompileFromFile(vertexShaderCode, nullptr, nullptr, "main", "vs_5_1", compileFlags, 0, &defaultVertexShader, nullptr))) {
			MessageBox(NULL, L"VertexShader解码失败", L"Error!!!", MB_OK);
			return FALSE;
		}
		if (FAILED(D3DCompileFromFile(pixelShaderCode, nullptr, nullptr, "main", "ps_5_1", compileFlags, 0, &defaultPixelShader, nullptr))) {
			MessageBox(NULL, L"PixelShader解码失败", L"Error!!!", MB_OK);
			return FALSE;
		}

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(DirectX::XMFLOAT3), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT2), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = rootSignature["default"];
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(defaultVertexShader);
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(defaultPixelShader);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = {};
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		psoDesc.BlendState.AlphaToCoverageEnable = false;
		psoDesc.BlendState.IndependentBlendEnable = false;
		psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].LogicOpEnable = false;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		if (FAILED(d3d12_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStates["opaque"]))))
			MessageBox(NULL, L"Create Default Pipeline Error", NULL, NULL);

		defaultVertexShader->Release();

		//创建阴影管线
		TCHAR shadowVertexShaderCode[] = _T("ShadowVertexShader.hlsl");
		TCHAR shadowPixelShaderCode[] = _T("ShadowPixelShader.hlsl");

		ID3DBlob* shadowVertexShader;
		ID3DBlob* shadowPixelShader;

		D3DCompileFromFile(shadowVertexShaderCode, nullptr, nullptr, "main", "vs_5_1", compileFlags, 0, &shadowVertexShader, nullptr);
		D3DCompileFromFile(shadowPixelShaderCode, nullptr, nullptr, "main", "ps_5_1", compileFlags, 0, &shadowPixelShader, nullptr);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPSODesc = psoDesc;
		shadowPSODesc.pRootSignature = rootSignature["shadow"];
		shadowPSODesc.RasterizerState.DepthBias = 5000;
		shadowPSODesc.RasterizerState.DepthBiasClamp = 0.0f;
		shadowPSODesc.RasterizerState.SlopeScaledDepthBias = 5.0f;
		shadowPSODesc.VS = CD3DX12_SHADER_BYTECODE(shadowVertexShader);
		shadowPSODesc.PS = CD3DX12_SHADER_BYTECODE(shadowPixelShader);
		shadowPSODesc.NumRenderTargets = 0;
		shadowPSODesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

		if (FAILED(d3d12_device->CreateGraphicsPipelineState(&shadowPSODesc, IID_PPV_ARGS(&pipelineStates["shadow"]))))
			MessageBox(NULL, L"Create Shadow Pipeline Error", NULL, NULL);

		//创建天空球管线
		TCHAR skyboxVSCode[] = _T("SkyboxVS.hlsl");
		TCHAR skyboxPSCode[] = _T("SkyboxPS.hlsl");

		ID3DBlob* skyboxVS;
		ID3DBlob* skyboxPS;

		D3DCompileFromFile(skyboxVSCode, nullptr, nullptr, "main", "vs_5_1", compileFlags, 0, &skyboxVS, nullptr);
		D3DCompileFromFile(skyboxPSCode, nullptr, nullptr, "main", "ps_5_1", compileFlags, 0, &skyboxPS, nullptr);
		
		D3D12_GRAPHICS_PIPELINE_STATE_DESC skyboxPSO = psoDesc;
		skyboxPSO.VS = CD3DX12_SHADER_BYTECODE(skyboxVS);
		skyboxPSO.PS = CD3DX12_SHADER_BYTECODE(skyboxPS);
		skyboxPSO.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		if (FAILED(d3d12_device->CreateGraphicsPipelineState(&skyboxPSO, IID_PPV_ARGS(&pipelineStates["skybox"]))))
			MessageBox(NULL, L"Create Skybox Pipeline Error", NULL, NULL);

		skyboxVS->Release();
		skyboxPS->Release();

		//创建蒙皮动画渲染管线
		D3D12_INPUT_ELEMENT_DESC skinnedIED[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(DirectX::XMFLOAT3), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT2), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 2 * sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT2), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 3 * sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT2), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		TCHAR skinnedVSCode[] = _T("SkinnedVS.hlsl");
		ID3DBlob* skinnedVS;
		D3DCompileFromFile(skinnedVSCode, nullptr, nullptr, "main", "vs_5_1", compileFlags, 0, &skinnedVS, nullptr);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedPSO = psoDesc;
		skinnedPSO.VS = CD3DX12_SHADER_BYTECODE(skinnedVS);

		skinnedPSO.InputLayout.NumElements = _countof(skinnedIED);
		skinnedPSO.InputLayout.pInputElementDescs = skinnedIED;
		d3d12_device->CreateGraphicsPipelineState(&skinnedPSO, IID_PPV_ARGS(&pipelineStates["skinnedOpaque"]));
		
		defaultPixelShader->Release();

		//蒙皮动画阴影渲染管线
		D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedShadowPSO = shadowPSODesc;
		skinnedShadowPSO.VS = CD3DX12_SHADER_BYTECODE(skinnedVS);
		skinnedShadowPSO.InputLayout.NumElements = _countof(skinnedIED);
		skinnedShadowPSO.InputLayout.pInputElementDescs = skinnedIED;
		d3d12_device->CreateGraphicsPipelineState(&skinnedShadowPSO, IID_PPV_ARGS(&pipelineStates["skinnedShadow"]));

		skinnedVS->Release();
		shadowVertexShader->Release();
		shadowPixelShader->Release();
	}

	//创建深度模板视图
	{
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D24_UNORM_S8_UINT, iWidth, iHeight, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clearValue.DepthStencil.Depth = 1;
		clearValue.DepthStencil.Stencil = 0;
		d3d12_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&d3d12_depthBuffer));

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		heapDesc.NumDescriptors = 2;
		heapDesc.NodeMask = 0;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		d3d12_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&d3d12_depthDescriptorHeap));

		D3D12_DEPTH_STENCIL_VIEW_DESC depthDesc = {};
		depthDesc.Flags = D3D12_DSV_FLAG_NONE;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		d3d12_device->CreateDepthStencilView(d3d12_depthBuffer, &depthDesc, d3d12_depthDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	}

	//创建命令列表分配器
	if (FAILED(d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12_commandAllocator)))) {
		MessageBox(NULL, L"命令列表分配器创建失败", L"Error!!!", MB_OK);
		return FALSE;
	}
	//创建命令列表
	if (FAILED(d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12_commandAllocator, pipelineStates["opaque"], IID_PPV_ARGS(&d3d12_commandList)))) {
		MessageBox(NULL, L"命令列表创建失败", L"Error!!!", MB_OK);
		return FALSE;
	}

	/*Create A Fence*/
	if (FAILED(d3d12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12_fence)))) {
		MessageBox(NULL, L"创建围栏失败", L"Error!!!", MB_OK);
		return FALSE;
	}
	UINT64 currFenceValue = 1;

	/*Load Model From File*/
	Model morisaModel("D:\\ProgrammingFiles\\VS_Projects\\VulkanGraphicsEngine\\Assets\\skinnedModel.fbx");

	/*Load Plane Texture From File*/
	Texture planeTexture;
	{
		ImageInfo imageInfo;
		const wchar_t* planeTexturePath = L"Assets\\brickTexture.jpg";
		LoadPixelWithWIC(planeTexturePath, GUID_WICPixelFormat32bppRGBA, imageInfo);
		planeTexture.LoadImageInfo(imageInfo);
		planeTexture.InitBuffer(d3d12_device);
		planeTexture.SetCopyCommand(d3d12_commandList);
	}

	/*Load SkyBox Texture From File*/
	Texture skyboxTexture;
	{
		ImageInfo imageInfo;
		const wchar_t* skyboxTexturePath = L"Assets\\skybox.png";
		LoadPixelWithWIC(skyboxTexturePath, GUID_WICPixelFormat32bppRGBA, imageInfo);
		skyboxTexture.LoadImageInfo(imageInfo);
		skyboxTexture.InitBufferAsCubeMap(d3d12_device, d3d12_commandList);
	}

	/*Load Texture From File*/
	morisaModel.LoadTexturesFromFile(d3d12_device, d3d12_commandList);
	d3d12_commandList->Close();
	d3d12_commandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&d3d12_commandList));

	/*Flush Command Queue*/
	currFenceValue++;
	d3d12_commandQueue->Signal(d3d12_fence, currFenceValue);
	if (d3d12_fence->GetCompletedValue() < currFenceValue)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		d3d12_fence->SetEventOnCompletion(currFenceValue, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	/*Put All The Textures In A Vector*/
	std::vector<Texture> textures;
	textures.insert(textures.end(), morisaModel.textureHasLoaded.begin(), morisaModel.textureHasLoaded.end());
	textures.push_back(planeTexture);
	textures.push_back(skyboxTexture);

	/*Build Descrptor Heap Only For SRV*/
	{
		UINT numDescriptors = textures.size() + 1;

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = numDescriptors;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		d3d12_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&CBVSRVHeap));
	}

	/*Create SRV For Each Texture*/
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipLevels = 1;
		desc.Texture2D.PlaneSlice = 0;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.ResourceMinLODClamp = 0.0f;
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(CBVSRVHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < textures.size() - 1; i++)
		{
			d3d12_device->CreateShaderResourceView(textures[i].GetTexture(), &desc, handle);
			handle.Offset(1, d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		}

		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		desc.TextureCube.MipLevels = 1;
		desc.TextureCube.MostDetailedMip = 0;
		desc.TextureCube.ResourceMinLODClamp = 0.0f;
		d3d12_device->CreateShaderResourceView(skyboxTexture.GetTexture(), &desc, handle);
	}

	/*Setup Vertex And Index Buffer*/
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> meshGeometries;
	/*Skinned Mesh*/
	{
		auto meshGeometry = std::make_unique<MeshGeometry>();
		std::vector<SkinnedVertex> vertices;
		std::vector<UINT> indices;

		for (UINT i = 0; i < morisaModel.renderInfo.size(); i++)
		{
			SubmeshGeometry submesh;
			submesh.baseVertexLocation = vertices.size();
			submesh.startIndexLocation = indices.size();
			submesh.indexCount = morisaModel.renderInfo[i].indices.size();

			vertices.insert(vertices.end(), morisaModel.renderInfo[i].vertices.begin(), morisaModel.renderInfo[i].vertices.end());
			indices.insert(indices.end(), morisaModel.renderInfo[i].indices.begin(), morisaModel.renderInfo[i].indices.end());

			meshGeometry->drawArgs.push_back(submesh);
		}

		UINT vertexBufferSize = sizeof(SkinnedVertex) * vertices.size();
		UINT indexBufferSize = sizeof(UINT) * indices.size();

		meshGeometry->vertexBufferSize = vertexBufferSize;
		meshGeometry->indexBufferSize = indexBufferSize;
		meshGeometry->vertexBufferStride = sizeof(SkinnedVertex);

		d3d12_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&meshGeometry->vertexBufferGPU));
		d3d12_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&meshGeometry->indexBufferGPU));

		BYTE* ptr = nullptr;
		meshGeometry->vertexBufferGPU->Map(0, nullptr, reinterpret_cast<void**>(&ptr));
		memcpy(ptr, vertices.data(), vertexBufferSize);
		meshGeometry->vertexBufferGPU->Unmap(0, nullptr);

		meshGeometry->indexBufferGPU->Map(0, nullptr, reinterpret_cast<void**>(&ptr));
		memcpy(ptr, indices.data(), indexBufferSize);
		meshGeometry->indexBufferGPU->Unmap(0, nullptr);

		D3DCreateBlob(vertexBufferSize, &meshGeometry->vertexBufferCPU);
		CopyMemory(meshGeometry->vertexBufferCPU->GetBufferPointer(), vertices.data(), vertexBufferSize);
		D3DCreateBlob(indexBufferSize, &meshGeometry->indexBufferCPU);
		CopyMemory(meshGeometry->indexBufferCPU->GetBufferPointer(), indices.data(), indexBufferSize);

		meshGeometries["skinnedModel"] = std::move(meshGeometry);
	}
	{
		auto meshGeometry = std::make_unique<MeshGeometry>();
		std::vector<Vertex> vertices;
		std::vector<UINT> indices;

		GeometryGenerator geoGen;

		/*Plane*/
		{
			GeometryGenerator::MeshData meshData = geoGen.CreatePlane(50.0f, 50.0f, 20, 20);

			SubmeshGeometry submesh;
			submesh.baseVertexLocation = vertices.size();
			submesh.startIndexLocation = indices.size();
			submesh.indexCount = meshData.indices.size();

			vertices.insert(vertices.end(), meshData.vertices.begin(), meshData.vertices.end());
			indices.insert(indices.end(), meshData.indices.begin(), meshData.indices.end());

			meshGeometry->drawArgs.push_back(submesh);
		}

		/*Skybox Sphere*/
		{
			GeometryGenerator::MeshData meshData = geoGen.CreateGeosphere(0.5f, 20);

			SubmeshGeometry submesh;
			submesh.baseVertexLocation = vertices.size();
			submesh.startIndexLocation = indices.size();
			submesh.indexCount = meshData.indices.size();

			vertices.insert(vertices.end(), meshData.vertices.begin(), meshData.vertices.end());
			indices.insert(indices.end(), meshData.indices.begin(), meshData.indices.end());

			meshGeometry->drawArgs.push_back(submesh);
		}

		UINT vertexBufferSize = sizeof(Vertex) * vertices.size();
		UINT indexBufferSize = sizeof(UINT) * indices.size();

		meshGeometry->vertexBufferSize = vertexBufferSize;
		meshGeometry->indexBufferSize = indexBufferSize;
		meshGeometry->vertexBufferStride = sizeof(Vertex);

		d3d12_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&meshGeometry->vertexBufferGPU));
		d3d12_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&meshGeometry->indexBufferGPU));

		BYTE* ptr = nullptr;
		meshGeometry->vertexBufferGPU->Map(0, nullptr, reinterpret_cast<void**>(&ptr));
		memcpy(ptr, vertices.data(), vertexBufferSize);
		meshGeometry->vertexBufferGPU->Unmap(0, nullptr);

		meshGeometry->indexBufferGPU->Map(0, nullptr, reinterpret_cast<void**>(&ptr));
		memcpy(ptr, indices.data(), indexBufferSize);
		meshGeometry->indexBufferGPU->Unmap(0, nullptr);

		D3DCreateBlob(vertexBufferSize, &meshGeometry->vertexBufferCPU);
		CopyMemory(meshGeometry->vertexBufferCPU->GetBufferPointer(), vertices.data(), vertexBufferSize);
		D3DCreateBlob(indexBufferSize, &meshGeometry->indexBufferCPU);
		CopyMemory(meshGeometry->indexBufferCPU->GetBufferPointer(), indices.data(), indexBufferSize);

		meshGeometries["scene"] = std::move(meshGeometry);
	}

	/*Build Up Materials*/
	std::vector<std::unique_ptr<Material>> materials;
	for (UINT i = 0; i < morisaModel.materials.size(); i++)
	{
		auto material = std::make_unique<Material>();
		material->matCBIndex = i;
		material->diffuseSRVIndex = morisaModel.materials[i].diffuseMaps;
		material->frensnelR0 = DirectX::XMFLOAT3(0.02f, 0.02f, 0.02f);
		material->roughness = 0.3f;
		material->diffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material->numFramesDirty = numFrameResource;
		materials.push_back(std::move(material));
	}

	/*Plane Material*/
	{
		auto material = std::make_unique<Material>();
		material->matCBIndex = materials.size();
		material->diffuseSRVIndex = materials.size();
		material->frensnelR0 = DirectX::XMFLOAT3(0.2f, 0.2f, 0.2f);
		material->roughness = 0.8f;
		material->diffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material->numFramesDirty = numFrameResource;
		materials.push_back(std::move(material));
	}

	/*Skybox Material*/
	{
		auto material = std::make_unique<Material>();
		material->matCBIndex = materials.size();
		material->diffuseSRVIndex = materials.size();
		material->frensnelR0 = DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f);
		material->roughness = 1.0f;
		material->diffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material->numFramesDirty = numFrameResource;
		materials.push_back(std::move(material));
	}

	/*Load Bone Animations*/
	auto skinnedModelInst = std::make_unique<SkinnedModelInstance>();
	SkinnedData skinndData;
	{
		std::vector<int> boneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> boneOffsets;
		std::vector<DirectX::XMFLOAT4X4> defaultTransform;
		morisaModel.GetBoneHierarchy(boneHierarchy);
		morisaModel.GetBoneOffsets(boneOffsets);
		morisaModel.GetNodeOffsets(defaultTransform);

		std::unordered_map<std::string, AnimationClip> animations;
		morisaModel.GetAnimations(animations);

		animations["walk"].boneAnimations.resize(defaultTransform.size());
		for (size_t i = 0; i < defaultTransform.size(); i++)
			animations["walk"].boneAnimations[i].defaultTransform = defaultTransform[i];
			
		skinndData.Set(boneHierarchy, boneOffsets, animations);
	
		skinnedModelInst->skinnedInfo = &skinndData;
		skinnedModelInst->timePos = 0.0f;
		skinnedModelInst->finalTransforms.resize(skinndData.GetBoneCount());
		skinnedModelInst->clipName = "walk";
	}

	UINT materialIndex = 0;

	/*Build Render Items For Model*/
	for (UINT i = 0; i < morisaModel.renderInfo.size(); i++)
	{
		auto renderItem = std::make_unique<RenderItem>();
		DirectX::XMStoreFloat4x4(&renderItem->texTransform, DirectX::XMMatrixIdentity());

		renderItem->objCBIndex = materialIndex;
		renderItem->geo = meshGeometries["skinnedModel"].get();
		renderItem->material = materials[materialIndex].get();
		renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		renderItem->indexCount = renderItem->geo->drawArgs[i].indexCount;
		renderItem->startIndexCount = renderItem->geo->drawArgs[i].startIndexLocation;
		renderItem->baseVertexLocation = renderItem->geo->drawArgs[i].baseVertexLocation;

		renderItem->skinnedCBIndex = 0;
		renderItem->skinnedModelInst = skinnedModelInst.get();

		ritemLayers[(int)RenderLayer::skinnedOpaque].push_back(renderItem.get());
		ritems.push_back(std::move(renderItem));

		materialIndex++;
	}

	UINT ritemIndex = 0;

	/*Plane Render Item*/
	{
		auto renderItem = std::make_unique<RenderItem>();
		DirectX::XMStoreFloat4x4(&renderItem->worldMatrix, DirectX::XMMatrixIdentity());
		DirectX::XMStoreFloat4x4(&renderItem->texTransform, DirectX::XMMatrixIdentity());

		renderItem->objCBIndex = materialIndex;
		renderItem->geo = meshGeometries["scene"].get();
		renderItem->material = materials[materialIndex].get();
		renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		renderItem->indexCount = renderItem->geo->drawArgs[ritemIndex].indexCount;
		renderItem->startIndexCount = renderItem->geo->drawArgs[ritemIndex].startIndexLocation;
		renderItem->baseVertexLocation = renderItem->geo->drawArgs[ritemIndex].baseVertexLocation;

		ritemLayers[(int)RenderLayer::opaque].push_back(renderItem.get());
		ritems.push_back(std::move(renderItem));
	}

	ritemIndex++;
	materialIndex++;

	/*Skybox Render Item*/
	{
		auto renderItem = std::make_unique<RenderItem>();
		DirectX::XMStoreFloat4x4(&renderItem->worldMatrix, DirectX::XMMatrixIdentity());
		DirectX::XMStoreFloat4x4(&renderItem->texTransform, DirectX::XMMatrixIdentity());

		UINT ritemIndex = 1;

		renderItem->objCBIndex = materialIndex;
		renderItem->geo = meshGeometries["scene"].get();
		renderItem->material = materials[materialIndex].get();
		renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		renderItem->indexCount = renderItem->geo->drawArgs[ritemIndex].indexCount;
		renderItem->startIndexCount = renderItem->geo->drawArgs[ritemIndex].startIndexLocation;
		renderItem->baseVertexLocation = renderItem->geo->drawArgs[ritemIndex].baseVertexLocation;

		ritemLayers[(int)RenderLayer::skybox].push_back(renderItem.get());
		ritems.push_back(std::move(renderItem));
	}

	/*Build Dynamic Sampler*/
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.NumDescriptors = 2;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		desc.NodeMask = 0x1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (FAILED(d3d12_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&d3d12_samplerHeap)))) {
			MessageBox(NULL, L"创建采样器描述符堆失败", L"Error!!!", MB_OK);
			return FALSE;
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(d3d12_samplerHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.BorderColor[0] = 0;
		samplerDesc.BorderColor[1] = 0;
		samplerDesc.BorderColor[2] = 0;
		samplerDesc.BorderColor[3] = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.MinLOD = 0;
		samplerDesc.MipLODBias = 0;

		d3d12_device->CreateSampler(&samplerDesc, handle);

		handle.Offset(1, d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER));

		D3D12_SAMPLER_DESC shadowSamplerDesc = {};
		shadowSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSamplerDesc.BorderColor[0] = 0;
		shadowSamplerDesc.BorderColor[1] = 0;
		shadowSamplerDesc.BorderColor[2] = 0;
		shadowSamplerDesc.BorderColor[3] = 0;
		shadowSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		shadowSamplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		shadowSamplerDesc.MaxAnisotropy = 16;
		shadowSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		shadowSamplerDesc.MinLOD = 0;
		shadowSamplerDesc.MipLODBias = 0;

		d3d12_device->CreateSampler(&shadowSamplerDesc, handle);
	}

	UINT objCount = ritems.size();

	/*Build Up Frame Resource*/
	for (size_t i = 0; i < numFrameResource; i++)
		frameResources.push_back(std::make_unique<FrameResource>(d3d12_device, 2, objCount, materials.size(), 1));

	/*Set Up Directional Light*/
	Light directionalLight = {};
	DirectX::XMStoreFloat3(&directionalLight.direction, DirectX::XMVector3Normalize(DirectX::XMVectorSet(-2.0f, -3.0f, 2.0f, 0.0f)));
	directionalLight.strength = { 1.0f, 1.0f, 1.0f };

	/*Create Directional Light Shadow Map*/
	ShadowMap shadowMap(d3d12_device, iWidth, iHeight);
	shadowMap.SetLightTransformMatrix(directionalLight.direction);

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(d3d12_depthDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
	CD3DX12_CPU_DESCRIPTOR_HANDLE SRVcpuHandle(CBVSRVHeap->GetCPUDescriptorHandleForHeapStart(), textures.size(), d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	CD3DX12_GPU_DESCRIPTOR_HANDLE SRVgpuHandle(CBVSRVHeap->GetGPUDescriptorHandleForHeapStart(), textures.size(), d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	shadowMap.BuildDescriptors(SRVcpuHandle, SRVgpuHandle, dsvHandle);

	//Create Player Controller
	ActorController playerController;
	DirectX::XMVECTOR pos = DirectX::XMVectorSet(0.0f, 10.0f, -8.0f, 1.0f);
	DirectX::XMVECTOR target = DirectX::XMVectorSet(0.0f, 0.0f, 2.0f, 1.0f);
	playerController.SetCamera(pos, target, DirectX::XM_PI / 18, DirectX::XM_PI / 18);
	DirectX::XMVECTOR modelForward = DirectX::XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f);
	DirectX::XMVECTOR handleForward = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	playerController.SetModel(0.0f, 0.0f, 0.01f, 1.0f);

	//创建定时器对象
	HANDLE phWait = CreateWaitableTimer(NULL, FALSE, NULL);
	LARGE_INTEGER liDueTime = {};
	liDueTime.QuadPart = -1i64;
	SetWaitableTimer(phWait, &liDueTime, 30.0f, NULL, NULL, 0);

	DWORD dwRet = 0;
	BOOL bExit = FALSE;
	while (!bExit)
	{
		dwRet = MsgWaitForMultipleObjects(1, &phWait, FALSE, INFINITE, QS_ALLINPUT);
		switch (dwRet - WAIT_OBJECT_0)
		{
		case 0:
		case WAIT_TIMEOUT:
		{
			/*Update*/

			/*Get Key Borad Input*/
			input_keyBoardDevice->Acquire();
			char keyBuffers[256] = {};
			input_keyBoardDevice->GetDeviceState(sizeof(keyBuffers), keyBuffers);
			
			/*Actor Move*/
			playerController.Update(keyBuffers, 0.05f);
			for (size_t i = 0; i < ritemLayers[(int)RenderLayer::skinnedOpaque].size(); i++) {
				playerController.GetWorldMatrix(ritemLayers[(int)RenderLayer::skinnedOpaque][i]->worldMatrix);
				ritemLayers[(int)RenderLayer::skinnedOpaque][i]->numFramesDirty = numFrameResource;
			}

			/*Get Current Frame Resource*/
			currFrameResourceIndex = (currFrameResourceIndex + 1) % numFrameResource;
			currFrameResource = frameResources[currFrameResourceIndex].get();

			/*Set Fence*/
			if (currFrameResource->fence != 0 && d3d12_fence->GetCompletedValue() < currFrameResource->fence)
			{
				HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
				d3d12_fence->SetEventOnCompletion(currFrameResource->fence, eventHandle);
				WaitForSingleObject(eventHandle, INFINITE);
				CloseHandle(eventHandle);
			}
			currFrameResource->fence = ++currFenceValue;

			/*Reset Command List*/
			auto currCmdAlloc = currFrameResource->cmdAlloc;
			currCmdAlloc->Reset();
			d3d12_commandList->Reset(currCmdAlloc, pipelineStates["opaque"]);

			UINT64 passCBByteSize = CalcUpper(sizeof(PassConstants));

			/*Update Object Constants*/
			auto currObjectCB = currFrameResource->objCB.get();
			for (auto& e : ritems)
			{
				if (e->numFramesDirty > 0) {
					ObjectConstants objectConstants;
					objectConstants.worldMatrix = e->worldMatrix;
					objectConstants.texTransform = e->texTransform;
					currFrameResource->objCB->CopyData(e->objCBIndex, objectConstants);
					e->numFramesDirty--;
				}
			}

			/*Update Skinned Constants*/
			if (playerController.isWalk)
				skinnedModelInst->UpdateSkinnedAnimation(0.05f);
			else
				skinnedModelInst->timePos = 0.0f;

			auto currSkinnedCB = currFrameResource->skinnedCB.get();
			SkinnedConstants skinnedConstants;
			std::copy(std::begin(skinnedModelInst->finalTransforms), std::end(skinnedModelInst->finalTransforms), skinnedConstants.boneTransforms);
			currSkinnedCB->CopyData(0, skinnedConstants);

			/*Update Shadow Pass Constants*/
			PassConstants passConstants = {};
			passConstants.viewMatrix = shadowMap.GetViewMatrix();
			passConstants.projMatrix = shadowMap.GetProjMatrix();

			auto currPassCB = currFrameResource->passCB.get();
			currPassCB->CopyData(1, passConstants);

			/*Draw Scene To Shadow Map*/
			d3d12_commandList->RSSetViewports(1, &shadowMap.GetViewport());
			d3d12_commandList->RSSetScissorRects(1, &shadowMap.GetScissorRect());

			D3D12_RESOURCE_BARRIER readToWrite = CD3DX12_RESOURCE_BARRIER::Transition(shadowMap.GetResource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			d3d12_commandList->ResourceBarrier(1, &readToWrite);

			d3d12_commandList->ClearDepthStencilView(shadowMap.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0.0f, 0, nullptr);
			d3d12_commandList->OMSetRenderTargets(0, nullptr, false, &shadowMap.GetDSV());

			d3d12_commandList->SetGraphicsRootSignature(rootSignature["shadow"]);

			d3d12_commandList->SetGraphicsRootConstantBufferView(1, currPassCB->uploadBuffer->GetGPUVirtualAddress() + passCBByteSize);

			d3d12_commandList->SetPipelineState(pipelineStates["skinnedShadow"]);
			DrawSceneToShadow(d3d12_device, d3d12_commandList, CBVSRVHeap, ritemLayers[(int)RenderLayer::skinnedOpaque], currFrameResource);

			d3d12_commandList->SetPipelineState(pipelineStates["shadow"]);
			DrawSceneToShadow(d3d12_device, d3d12_commandList, CBVSRVHeap, ritemLayers[(int)RenderLayer::opaque], currFrameResource);

			D3D12_RESOURCE_BARRIER writeToRead = CD3DX12_RESOURCE_BARRIER::Transition(shadowMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
			d3d12_commandList->ResourceBarrier(1, &writeToRead);

			/*Update Material Constants*/
			auto currMaterialCB = currFrameResource->matCB.get();
			for (auto& e : materials)
			{
				if (e->numFramesDirty > 0) {
					MaterialConstants materialConstants;
					materialConstants.diffuseAlbedo = e->diffuseAlbedo;
					materialConstants.fresnelR0 = e->frensnelR0;
					materialConstants.matTransform = e->matTransform;
					materialConstants.roughness = e->roughness;
					currMaterialCB->CopyData(e->matCBIndex, materialConstants);
					e->numFramesDirty--;
				}
			}

			/*Update Pass Constants*/
			playerController.GetViewProjMatrix(passConstants.viewMatrix, passConstants.projMatrix);
			passConstants.shadowTransform = shadowMap.GetShadowTranformMatrix();
			DirectX::XMFLOAT3 pos3f = playerController.GetCameraPosition();
			passConstants.eyePos = { pos3f.x, pos3f.y, pos3f.z, 1.0f };
			passConstants.ambientLight = { 0.5f, 0.5f, 0.5f, 1.0f };
			passConstants.lights[0] = directionalLight;
			
			currPassCB->CopyData(0, passConstants);

			/*Render Pass*/
			d3d12_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(d3d12_RTVBuffer[nFrameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(d3d12_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), nFrameIndex, nRTVDescriptorSize);
			CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(d3d12_depthDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
			d3d12_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

			d3d12_commandList->RSSetViewports(1, &stViewPort);
			d3d12_commandList->RSSetScissorRects(1, &stScissorRect);

			const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			d3d12_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
			d3d12_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			/*Bind Root Signature*/
			d3d12_commandList->SetGraphicsRootSignature(rootSignature["default"]);

			ID3D12DescriptorHeap* descHeaps[] = { d3d12_samplerHeap, CBVSRVHeap };
			d3d12_commandList->SetDescriptorHeaps(2, descHeaps);

			/*Bind Pass Constant Buffer And Dynamic Sampler*/
			d3d12_commandList->SetGraphicsRootConstantBufferView(4, currPassCB->uploadBuffer->GetGPUVirtualAddress());

			CD3DX12_GPU_DESCRIPTOR_HANDLE samplerHandle(d3d12_samplerHeap->GetGPUDescriptorHandleForHeapStart());
			d3d12_commandList->SetGraphicsRootDescriptorTable(1, samplerHandle);
			samplerHandle.Offset(1, d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER));
			d3d12_commandList->SetGraphicsRootDescriptorTable(2, samplerHandle);

			d3d12_commandList->SetGraphicsRootDescriptorTable(6, shadowMap.GetSRV());

			/*Draw Skinned Model*/
			d3d12_commandList->SetPipelineState(pipelineStates["skinnedOpaque"]);
			DrawRenderItems(d3d12_device, d3d12_commandList, CBVSRVHeap, ritemLayers[(int)RenderLayer::skinnedOpaque], currFrameResource);

			/*Draw Opaque Render Item*/
			d3d12_commandList->SetPipelineState(pipelineStates["opaque"]);
			DrawRenderItems(d3d12_device, d3d12_commandList, CBVSRVHeap, ritemLayers[(int)RenderLayer::opaque], currFrameResource);

			/*Draw Sky Sphere*/
			d3d12_commandList->SetPipelineState(pipelineStates["skybox"]);
			DrawRenderItems(d3d12_device, d3d12_commandList, CBVSRVHeap, ritemLayers[(int)RenderLayer::skybox], currFrameResource);

			d3d12_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(d3d12_RTVBuffer[nFrameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
			
			d3d12_commandList->Close();

			d3d12_commandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&d3d12_commandList));
			
			dxgi_swapChain->Present(0, 0);
			nFrameIndex = dxgi_swapChain->GetCurrentBackBufferIndex();

			d3d12_commandQueue->Signal(d3d12_fence, currFenceValue);
		}

		break;
		case 1:
		{//处理消息
			while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (WM_QUIT != msg.message)
				{
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
				}
				else
				{
					bExit = TRUE;
				}
			}
		}
		break;
		default:
			break;
		}
	}
}

void DrawSceneToShadow(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* descHeap, const std::vector<RenderItem*>& ritems, FrameResource* currFrameResource) {
	auto currObjectCB = currFrameResource->objCB.get();
	auto currSkinnedCB = currFrameResource->skinnedCB.get();

	UINT64 objCBByteSize = CalcUpper(sizeof(ObjectConstants));
	UINT64 skinnedCBByteSize = CalcUpper(sizeof(SkinnedConstants));

	for (UINT i = 0; i < ritems.size(); i++)
	{
		auto ri = ritems[i];

		/*Bind Object Constant Buffer*/
		D3D12_GPU_VIRTUAL_ADDRESS objAddress = currObjectCB->uploadBuffer->GetGPUVirtualAddress() + static_cast<UINT64>(ri->objCBIndex)* objCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objAddress);

		/*Bind Skinned Constant Buffer*/
		if (ri->skinnedModelInst != nullptr) {
			D3D12_GPU_VIRTUAL_ADDRESS skinnedAddress = currSkinnedCB->uploadBuffer->GetGPUVirtualAddress() + static_cast<UINT64>(ri->skinnedCBIndex)* skinnedCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(2, skinnedAddress);
		}
		else
			cmdList->SetGraphicsRootConstantBufferView(2, 0);

		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);
		cmdList->IASetVertexBuffers(0, 1, &ri->geo->GetVertexBufferView());
		cmdList->IASetIndexBuffer(&ri->geo->GetIndexBufferView());
		cmdList->DrawIndexedInstanced(ri->indexCount, 1, ri->startIndexCount, ri->baseVertexLocation, 0);
	}
}

void DrawRenderItems(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* descHeap, const std::vector<RenderItem*>& ritems, FrameResource* currFrameResource) {
	auto currObjectCB = currFrameResource->objCB.get();
	auto currMaterialCB = currFrameResource->matCB.get();
	auto currSkinnedCB = currFrameResource->skinnedCB.get();

	UINT64 objCBByteSize = CalcUpper(sizeof(ObjectConstants));
	UINT64 matCBByteSize = CalcUpper(sizeof(MaterialConstants));
	UINT64 skinnedCBByteSize = CalcUpper(sizeof(SkinnedConstants));
	
	for (UINT i = 0; i < ritems.size(); i++)
	{
		auto ri = ritems[i];

		/*Bind Shader Resource*/
		UINT SRVOffset = ri->material->diffuseSRVIndex;
		CD3DX12_GPU_DESCRIPTOR_HANDLE SRVHandle(descHeap->GetGPUDescriptorHandleForHeapStart(), SRVOffset, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		cmdList->SetGraphicsRootDescriptorTable(0, SRVHandle);

		/*Bind Object Constant Buffer*/
		D3D12_GPU_VIRTUAL_ADDRESS objAddress = currObjectCB->uploadBuffer->GetGPUVirtualAddress() + static_cast<UINT64>(ri->objCBIndex) * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matAddress = currMaterialCB->uploadBuffer->GetGPUVirtualAddress() + static_cast<UINT64>(ri->material->matCBIndex) * matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(3, objAddress);
		cmdList->SetGraphicsRootConstantBufferView(5, matAddress);

		/*Bind Skinned Constant Buffer*/
		if (ri->skinnedModelInst != nullptr) {
			D3D12_GPU_VIRTUAL_ADDRESS skinnedAddress = currSkinnedCB->uploadBuffer->GetGPUVirtualAddress() + static_cast<UINT64>(ri->skinnedCBIndex)* skinnedCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(7, skinnedAddress);
		}else
			cmdList->SetGraphicsRootConstantBufferView(7, 0);

		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);
		cmdList->IASetVertexBuffers(0, 1, &ri->geo->GetVertexBufferView());
		cmdList->IASetIndexBuffer(&ri->geo->GetIndexBufferView());
		cmdList->DrawIndexedInstanced(ri->indexCount, 1, ri->startIndexCount, ri->baseVertexLocation, 0);
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}