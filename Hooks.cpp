#include <functional>
#include <intrin.h>
#include <string>
#include <Windows.h>
#include <Psapi.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

#include "MinHook/MinHook.h"

#include "Config.h"
#include "EventListener.h"
#include "GameData.h"
#include "GUI.h"
#include "Hooks.h"
#include "Interfaces.h"
#include "Memory.h"

#include "Hacks/Aimbot.h"
#include "Hacks/Animations.h"
#include "Hacks/AntiAim.h"
#include "Hacks/Backtrack.h"
#include "Hacks/Chams.h"
#include "Hacks/EnginePrediction.h"
#include "Hacks/StreamProofESP.h"
#include "Hacks/Glow.h"
#include "Hacks/Misc.h"
#include "Hacks/SkinChanger.h"
#include "Hacks/Triggerbot.h"
#include "Hacks/Visuals.h"

#include "SDK/Engine.h"
#include "SDK/Entity.h"
#include "SDK/EntityList.h"
#include "SDK/FrameStage.h"
#include "SDK/GameEvent.h"
#include "SDK/GameUI.h"
#include "SDK/GlobalVars.h"
#include "SDK/Input.h"
#include "SDK/InputSystem.h"
#include "SDK/MaterialSystem.h"
#include "SDK/ModelRender.h"
#include "SDK/NetworkChannel.h"
#include "SDK/Panel.h"
#include "SDK/RenderContext.h"
#include "SDK/SoundInfo.h"
#include "SDK/SoundEmitter.h"
#include "SDK/StudioRender.h"
#include "SDK/Surface.h"
#include "SDK/UserCmd.h"
#include "SDK/ConVar.h"
#include "SDK/ViewSetup.h"

static LRESULT __stdcall wndProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
    [[maybe_unused]] static const auto initGlobal = [](HWND window) noexcept {
        netvars = std::make_unique<Netvars>();
        eventListener = std::make_unique<EventListener>();

        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window);
        config = std::make_unique<Config>("NEPS");
        gui = std::make_unique<GUI>();

		hooks->install();

		const char *loaded = config->load(u8"default", false) ? "\n\nConfig \"default\" has been automatically loaded." : "";

		gui->updateColors();
		SkinChanger::scheduleHudUpdate();

		std::string welcomeMsg =
R"_msg(We do have a resolver now >:)

Let's get started!
To open UI press %button on your keyboard.%loaded

RECENT CHANGELOG:
  - Added PBR pipeline chams
  - Added multipoint
  - Added static props chams and color
  - Added extended desync (very broken, don't use it)
  - Improved desync visualisation
  - Improved keybinds
  - Added smoothness to zoom and third person
  - Added kill effects
  - Added smoothness to kill and hit effects

RECENT FIXES:
  - Fixed directional autostrafer understeer
  - Fixed chams not showing thrown weapons
  - Fixed string loading issues
  - Fixed custom font crash
  - Revised GUI labels)_msg";

		Helpers::replace(welcomeMsg, "%button", interfaces->inputSystem->virtualKeyToString(config->misc.menuKey));
		Helpers::replace(welcomeMsg, "%loaded", loaded);

		interfaces->gameUI->createCommandMsgBox("Welcome to NEPS", welcomeMsg.c_str());

        return true;
    }(window);

    if (msg == WM_KEYDOWN && LOWORD(wParam) == config->misc.menuKey
        || ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) && config->misc.menuKey == VK_LBUTTON)
        || ((msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) && config->misc.menuKey == VK_RBUTTON)
        || ((msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) && config->misc.menuKey == VK_MBUTTON)
        || ((msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) && config->misc.menuKey == HIWORD(wParam) + 4)) {
        gui->open = !gui->open;
        if (!gui->open) {
            interfaces->inputSystem->resetInputState();
        }
    }

	LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	ImGui_ImplWin32_WndProcHandler(window, msg, wParam, lParam);

	interfaces->inputSystem->enableInput(!gui->open);

	return CallWindowProcW(hooks->originalWndProc, window, msg, wParam, lParam);
}

static HRESULT __stdcall present(IDirect3DDevice9* device, const RECT* src, const RECT* dest, HWND windowOverride, const RGNDATA* dirtyRegion) noexcept
{
    [[maybe_unused]] static bool imguiInit{ ImGui_ImplDX9_Init(device) };

    if (config->loadScheduledFonts())
		ImGui_ImplDX9_DestroyFontsTexture();

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    StreamProofESP::render();
    Misc::purchaseList();
    Misc::drawOffscreenEnemies(ImGui::GetBackgroundDrawList());
	Visuals::hitMarker(nullptr, ImGui::GetBackgroundDrawList());
	Visuals::drawMolotovHull(ImGui::GetBackgroundDrawList());
    Misc::noscopeCrosshair(ImGui::GetBackgroundDrawList());
    Misc::recoilCrosshair(ImGui::GetBackgroundDrawList());
	Misc::drawBombTimer();
	Misc::indicators(ImGui::GetBackgroundDrawList());

	#ifndef LEGACY_WATERMARK
	Misc::spectatorList(ImGui::GetBackgroundDrawList());
	Misc::watermark(ImGui::GetBackgroundDrawList());
	#endif // !LEGACY_WATERMARK

    if (gui->open)
        gui->render();

    ImGui::EndFrame();
    ImGui::Render();

    if (device->BeginScene() == D3D_OK) {
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        device->EndScene();
    }

    return hooks->originalPresent(device, src, dest, windowOverride, dirtyRegion);
}

static HRESULT __stdcall reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) noexcept
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    return hooks->originalReset(device, params);
}

static bool __fastcall sendNetMsg(NetworkChannel *networkchannel, void *edx, NetworkMessage &msg, bool forceReliable, bool voice) noexcept
{
	if (msg.getType() == 14 && config->exploits.bypassPure) // Return and don't send messsage if its FileCRCCheck
		return false;

	return hooks->engine.callOriginal<bool, 40>(networkchannel, edx, msg, forceReliable, voice);
}

static bool netChInitialized = false;

static bool __stdcall createMove(float inputSampleTime, UserCmd *cmd) noexcept
{
	auto result = hooks->clientMode.callOriginal<bool, 24>(inputSampleTime, cmd);

	if (!cmd->commandNumber)
		return result;

	uintptr_t *framePointer;
	__asm mov framePointer, ebp;
	bool &sendPacket = *reinterpret_cast<bool *>(*framePointer - 0x1C);

	static auto previousViewAngles = cmd->viewangles;
	const auto currentViewAngles = cmd->viewangles;

	memory->globalVars->serverTime(cmd);
	Misc::changeConVarsTick();
	Misc::antiAfkKick(cmd);
	Misc::fastStop(cmd);
	Misc::runReportbot();
	Misc::autoStrafe(cmd);
	Misc::bunnyHop(cmd);
	Misc::removeCrouchCooldown(cmd);
	Misc::autoPistol(cmd);
	Misc::useSpam(cmd);
	Misc::autoReload(cmd);
	Misc::updateClanTag();
	Misc::fakeBan();
	Misc::stealNames();
	Misc::revealRanks(cmd);
	Misc::quickReload(cmd);
	Misc::quickHealthshot(cmd);
	Misc::fixTabletSignal();
	Misc::slowwalk(cmd);
	Misc::blockBot(cmd);

	EnginePrediction::run(cmd);

	Aimbot::run(cmd);
	Triggerbot::run(cmd);
	Backtrack::run(cmd);
	Misc::edgejump(cmd);
	Misc::fastPlant(cmd);

	AntiAim::run(cmd, currentViewAngles, sendPacket);

	Misc::prepareRevolver(cmd); // Bruh, I know...

	auto viewAnglesDelta{cmd->viewangles - previousViewAngles};
	viewAnglesDelta.normalize();
	viewAnglesDelta.x = std::clamp(viewAnglesDelta.x, -config->misc.maxAngleDelta, config->misc.maxAngleDelta);
	viewAnglesDelta.y = std::clamp(viewAnglesDelta.y, -config->misc.maxAngleDelta, config->misc.maxAngleDelta);

	cmd->viewangles = previousViewAngles + viewAnglesDelta;

	cmd->viewangles.normalize();

	Misc::fixMovement(cmd, currentViewAngles.y);
	Misc::moonwalk(cmd, sendPacket);

	cmd->viewangles.x = std::clamp(cmd->viewangles.x, -89.0f, 89.0f);
	cmd->viewangles.y = std::clamp(cmd->viewangles.y, -180.0f, 180.0f);
	cmd->viewangles.z = 0.0f;
	cmd->forwardmove = std::clamp(cmd->forwardmove, -450.0f, 450.0f);
	cmd->sidemove = std::clamp(cmd->sidemove, -450.0f, 450.0f);

	previousViewAngles = cmd->viewangles;

	auto &global = GameData::global();

	global.sentPacket = sendPacket;
	global.lastCmd = *cmd;

	if (config->antiAim.desync)
		Animations::clientLerped(global.lerpedBones, cmd, sendPacket, &global.indicators.desyncHead, &global.indicators.deltaLby);

	//if (interfaces->engine->isInGame())
	//{
	//	static NetworkChannel *old = nullptr;
	//	NetworkChannel *current = interfaces->engine->getNetworkChannel();
	//	if (current && old != current)
	//	{
	//		netChInitialized = false;
	//		old = current;
	//		hooks->networkChannel.init(current);
	//		hooks->networkChannel.hookAt(42, sendNetMsg);
	//		netChInitialized = true;
	//	}
	//}

	return false;
}

static int __stdcall doPostScreenEffects(int param) noexcept
{
    if (interfaces->engine->isInGame()) {
		Visuals::thirdperson();
        Visuals::reduceFlashEffect();
        Visuals::updateBrightness();
        Glow::render();
    }
    return hooks->clientMode.callOriginal<int, 44>(param);
}

static float __stdcall getViewModelFov() noexcept
{
    float additionalFov = static_cast<float>(config->visuals.viewmodel.fov);
    if (localPlayer) {
        if (const auto activeWeapon = localPlayer->getActiveWeapon(); activeWeapon && activeWeapon->getClientClass()->classId == ClassId::Tablet || !config->visuals.viewmodel.enabled)
            additionalFov = 0.0f;
    }

    return hooks->clientMode.callOriginal<float, 35>() + additionalFov;
}

static void __stdcall drawModelExecute(void* ctx, void* state, const ModelRenderInfo& info, matrix3x4* customBoneToWorld) noexcept
{
    if (interfaces->studioRender->isForcedMaterialOverride())
        return hooks->modelRender.callOriginal<void, 21>(ctx, state, std::cref(info), customBoneToWorld);

    if (Visuals::removeHands(info.model->name) || Visuals::removeSleeves(info.model->name) || Visuals::removeWeapons(info.model->name))
        return;

    static Chams chams;
    if (!chams.render(ctx, state, info, customBoneToWorld))
        hooks->modelRender.callOriginal<void, 21>(ctx, state, std::cref(info), customBoneToWorld);
    interfaces->studioRender->forcedMaterialOverride(nullptr);
}

static bool __fastcall svCheatsGetBool(void* _this) noexcept
{
	if (uintptr_t(_ReturnAddress()) == memory->cameraThink && config->visuals.thirdPerson.keyMode)
		return true;

	return hooks->svCheats.getOriginal<bool, 13>()(_this);
}

static void __stdcall paintTraverse(unsigned int panel, bool forceRepaint, bool allowForce) noexcept
{
    if (interfaces->panel->getName(panel) == "MatSystemTopPanel") {
		#ifdef LEGACY_WATERMARK
        Misc::spectatorList();
        Misc::watermark();
		#endif // LEGACY_WATERMARK
	} else if (interfaces->panel->getName(panel) == "GameConsole")
	{
		
	}
    hooks->panel.callOriginal<void, 41>(panel, forceRepaint, allowForce);
}

static void __stdcall frameStageNotify(FrameStage stage) noexcept
{
	[[maybe_unused]] static auto backtrackInit = (Backtrack::init(), false);

	if (interfaces->engine->isConnected() && !interfaces->engine->isInGame())
		Misc::changeName(true, nullptr, 0.0f);

	Misc::changeConVarsFrame(stage);

	if (stage == FrameStage::START)
		GameData::update();

	if (stage == FrameStage::RENDER_START)
	{
		Misc::preserveKillfeed();
		Visuals::colorWorld();
		Misc::fakePrime();
		Misc::fixAnimation();
	}

	if (interfaces->engine->isInGame())
	{
		Visuals::skybox(stage);
		Visuals::removeBlur(stage);
		Visuals::removeGrass(stage);
		Visuals::modifySmoke(stage);
		Visuals::playerModel(stage);
		Visuals::disablePostProcessing(stage);
		Visuals::removeVisualRecoil(stage);
		Misc::fixAnimationLOD(stage);
		Backtrack::update(stage);
		SkinChanger::run(stage);
	}

	hooks->client.callOriginal<void, 37>(stage);
}

struct SoundData
{
	int filter;
	int entityIndex;
	int channel;
	const char *soundEntry;
	PAD(8)
	float volume;
	PAD(44)
};

static void __stdcall emitSound(SoundData data) noexcept
{
    auto modulateVolume = [&data](int(*get)(int)) {
        if (const auto entity = interfaces->entityList->getEntity(data.entityIndex); localPlayer && entity && entity->isPlayer()) {
            if (data.entityIndex == localPlayer->index())
                data.volume *= get(0) / 100.0f;
            else if (!entity->isOtherEnemy(localPlayer.get()))
                data.volume *= get(1) / 100.0f;
            else
                data.volume *= get(2) / 100.0f;
        }
    };

    modulateVolume([](int index) { return config->sound.players[index].masterVolume; });

    if (strstr(data.soundEntry, "Weapon") && strstr(data.soundEntry, "Single")) {
        modulateVolume([](int index) { return config->sound.players[index].weaponVolume; });
    } else if (config->misc.autoAccept && !strcmp(data.soundEntry, "UIPanorama.popup_accept_match_beep")) {
        memory->acceptMatch("");
		auto window = hooks->getProcessWindow();
        FLASHWINFO flash{ sizeof(FLASHWINFO), window, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0 };
        FlashWindowEx(&flash);
        ShowWindow(window, SW_RESTORE);
    }
    data.volume = std::clamp(data.volume, 0.0f, 1.0f);
    hooks->sound.callOriginal<void, 5>(data);
}

static bool __stdcall shouldDrawFog() noexcept
{
	if constexpr (std::is_same_v<HookType, MinHook>)
	{
		#ifdef _DEBUG_NEPS
		// Check if we always get the same return address
		if (*static_cast<std::uint32_t *>(_ReturnAddress()) == 0x6274C084)
		{
			static const auto returnAddress = std::uintptr_t(_ReturnAddress());
			assert(returnAddress == std::uintptr_t(_ReturnAddress()));
		}
		#endif // _DEBUG_NEPS

		if (*static_cast<std::uint32_t *>(_ReturnAddress()) != 0x6274C084)
			return hooks->clientMode.callOriginal<bool, 17>();
	}

	return !config->visuals.noFog;
}

static bool __stdcall shouldDrawViewModel() noexcept
{
    if (localPlayer && localPlayer->fov() < 45 && localPlayer->fovStart() < 45)
        return false;
    return hooks->clientMode.callOriginal<bool, 27>();
}

static void __stdcall lockCursor() noexcept
{
    if (gui->open)
        return interfaces->surface->unlockCursor();
    return hooks->surface.callOriginal<void, 67>();
}

static void __stdcall setDrawColor(int r, int g, int b, int a) noexcept
{
#ifdef _DEBUG_NEPS
    // Check if we always get the same return address
    if (*static_cast<std::uint32_t*>(_ReturnAddress()) == 0x20244C8B) {
        static const auto returnAddress = std::uintptr_t(_ReturnAddress());
        assert(returnAddress == std::uintptr_t(_ReturnAddress()));
    }
    if (*reinterpret_cast<std::uint32_t*>(std::uintptr_t(_ReturnAddress()) + 6) == 0x01ACB7FF) {
        static const auto returnAddress = std::uintptr_t(_ReturnAddress());
        assert(returnAddress == std::uintptr_t(_ReturnAddress()));
    }
#endif // _DEBUG_NEPS

    if (config->visuals.noScopeOverlay && (*static_cast<std::uint32_t*>(_ReturnAddress()) == 0x20244C8B || *reinterpret_cast<std::uint32_t*>(std::uintptr_t(_ReturnAddress()) + 6) == 0x01ACB7FF))
        a = 0;
    hooks->surface.callOriginal<void, 15>(r, g, b, a);
}

static void __stdcall overrideView(ViewSetup *setup) noexcept
{
    const float fov = static_cast<float>(config->visuals.fov);
    const float zoomFov = fov * (1.0f - (static_cast<float>(config->visuals.zoomFac) / 100.0f));
	static float curFov = fov;
	static Helpers::KeyBindState zoom;
	curFov = zoom[config->visuals.zoom] ?
		Helpers::approachValSmooth(zoomFov, curFov, memory->globalVars->frametime * 10.0f) :
		Helpers::approachValSmooth(fov, curFov, memory->globalVars->frametime * 10.0f);

	if (localPlayer)
	{
		if (localPlayer->isAlive())
		{
			if ((!localPlayer->isScoped() || config->visuals.forceFov))
				setup->fov = curFov;
			
			if (static Helpers::KeyBindState fakeDuck; fakeDuck[config->antiAim.fakeDuck])
				setup->origin.z = localPlayer->origin().z + PLAYER_EYE_HEIGHT;

			if (config->visuals.viewmodel.enabled && !localPlayer->isScoped() && !memory->input->isCameraInThirdPerson)
			{
				const auto viewModel = interfaces->entityList->getEntityFromHandle(localPlayer->viewModel());
				if (viewModel)
				{
					Vector forward = Vector::fromAngle(setup->angles);
					Vector up = Vector::fromAngle(setup->angles - Vector{-90.0f, 0.0f, 0.0f});
					Vector side = forward.crossProduct(up);
					Vector offset = forward * config->visuals.viewmodel.x + side * config->visuals.viewmodel.y + up * config->visuals.viewmodel.z;
					memory->setAbsOrigin(viewModel, viewModel->getRenderOrigin() + offset);
					memory->setAbsAngle(viewModel, setup->angles + Vector{0.0f, 0.0f, config->visuals.viewmodel.roll});
				}
			}
		}
		else if (auto observed = localPlayer->getObserverTarget(); observed && localPlayer->getObserverMode() == ObsMode::InEye)
		{
			if ((!observed->isScoped() || config->visuals.forceFov))
				setup->fov = curFov;

			if (config->visuals.viewmodel.enabled && !observed->isScoped())
			{
				const auto viewModel = interfaces->entityList->getEntityFromHandle(observed->viewModel());
				if (viewModel)
				{
					Vector forward = Vector::fromAngle(setup->angles);
					Vector up = Vector::fromAngle(setup->angles - Vector{-90.0f, 0.0f, 0.0f});
					Vector side = forward.crossProduct(up);
					Vector offset = forward * config->visuals.viewmodel.x + side * config->visuals.viewmodel.y + up * config->visuals.viewmodel.z;
					memory->setAbsOrigin(viewModel, viewModel->getRenderOrigin() + offset);
					memory->setAbsAngle(viewModel, setup->angles + Vector{0.0f, 0.0f, config->visuals.viewmodel.roll});
				}
			}
		}
		else
		{
			setup->fov = curFov;
		}
	}

	setup->farZ = float(config->visuals.farZ * 10);
	hooks->clientMode.callOriginal<void, 18>(setup);
}

struct RenderableInfo {
    Entity* renderable;
    std::byte pad[18];
    uint16_t flags;
    uint16_t flags2;
};

static int __stdcall listLeavesInBox(const Vector& mins, const Vector& maxs, unsigned short* list, int listMax) noexcept
{
    if (std::uintptr_t(_ReturnAddress()) == memory->listLeaves) {
        if (const auto info = *reinterpret_cast<RenderableInfo**>(std::uintptr_t(_AddressOfReturnAddress()) + 0x14); info && info->renderable) {
            if (const auto ent = VirtualMethod::call<Entity*, 7>(info->renderable - 4); ent && ent->isPlayer()) {
                if (config->misc.disableModelOcclusion) {
                    // FIXME: sometimes players are rendered above smoke, maybe sort render list?
                    info->flags &= ~0x100;
                    info->flags2 |= 0x40;

                    constexpr float maxCoord = 16384.0f;
                    constexpr float minCoord = -maxCoord;
                    constexpr Vector min{ minCoord, minCoord, minCoord };
                    constexpr Vector max{ maxCoord, maxCoord, maxCoord };
                    return hooks->bspQuery.callOriginal<int, 6>(std::cref(min), std::cref(max), list, listMax);
                }
            }
        }
    }
    return hooks->bspQuery.callOriginal<int, 6>(std::cref(mins), std::cref(maxs), list, listMax);
}

static int __fastcall dispatchSound(SoundInfo& soundInfo) noexcept
{
    if (const char* soundName = interfaces->soundEmitter->getSoundName(soundInfo.soundIndex)) {
        auto modulateVolume = [&soundInfo](int(*get)(int)) {
            if (auto entity{ interfaces->entityList->getEntity(soundInfo.entityIndex) }; entity && entity->isPlayer()) {
                if (localPlayer && soundInfo.entityIndex == localPlayer->index())
                    soundInfo.volume *= get(0) / 100.0f;
                else if (!entity->isOtherEnemy(localPlayer.get()))
                    soundInfo.volume *= get(1) / 100.0f;
                else
                    soundInfo.volume *= get(2) / 100.0f;
            }
        };

        modulateVolume([](int index) { return config->sound.players[index].masterVolume; });

        if (!strcmp(soundName, "Player.DamageHelmetFeedback"))
            modulateVolume([](int index) { return config->sound.players[index].headshotVolume; });
        else if (strstr(soundName, "Step"))
            modulateVolume([](int index) { return config->sound.players[index].footstepVolume; });
        else if (strstr(soundName, "Chicken"))
            soundInfo.volume *= config->sound.chickenVolume / 100.0f;
    }
    soundInfo.volume = std::clamp(soundInfo.volume, 0.0f, 1.0f);
    return hooks->originalDispatchSound(soundInfo);
}

static int __stdcall render2dEffectsPreHud(int param) noexcept
{
    Visuals::applyScreenEffects();
    Visuals::hitEffect();
    Visuals::killEffect();
    return hooks->viewRender.callOriginal<int, 39>(param);
}

static const DemoPlaybackParameters* __stdcall getDemoPlaybackParameters() noexcept
{
    const auto params = hooks->engine.callOriginal<const DemoPlaybackParameters*, 218>();

#ifdef _DEBUG_NEPS
    // Check if we always get the same return address
    if (*static_cast<std::uint64_t*>(_ReturnAddress()) == 0x79801F74C985C88B) {
        static const auto returnAddress = std::uintptr_t(_ReturnAddress());
        assert(returnAddress == std::uintptr_t(_ReturnAddress()));
    }
#endif // _DEBUG_NEPS

    if (params && config->misc.revealSuspect && *static_cast<std::uint64_t*>(_ReturnAddress()) != 0x79801F74C985C88B) { // client.dll : 8B C8 85 C9 74 1F 80 79 10 00 , there game decides whether to show overwatch panel
        static DemoPlaybackParameters customParams;
        customParams = *params;
        customParams.anonymousPlayerIdentity = false;
        return &customParams;
    }

    return params;
}

static bool __stdcall isPlayingDemo() noexcept
{
#ifdef _DEBUG_NEPS
    // Check if we always get the same return address
    if (*static_cast<std::uintptr_t*>(_ReturnAddress()) == 0x0975C084
        && **reinterpret_cast<std::uintptr_t**>(std::uintptr_t(_AddressOfReturnAddress()) + 4) == 0x0C75C084) {
        static const auto returnAddress = std::uintptr_t(_ReturnAddress());
        assert(returnAddress == std::uintptr_t(_ReturnAddress()));
    }
#endif // _DEBUG_NEPS

    if (config->misc.revealMoney
        && *static_cast<uintptr_t*>(_ReturnAddress()) == 0x0975C084 // client.dll : 84 C0 75 09 38 05
        && **reinterpret_cast<uintptr_t**>(uintptr_t(_AddressOfReturnAddress()) + 4) == 0x0C75C084) { // client.dll : 84 C0 75 0C 5B
        return true;
    }
    return hooks->engine.callOriginal<bool, 82>();
}

static void __stdcall updateColorCorrectionWeights() noexcept
{
    hooks->clientMode.callOriginal<void, 58>();

    if (const auto& cfg = config->visuals.colorCorrection; cfg.enabled) {
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + 0x498) = cfg.blue;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + 0x4A0) = cfg.red;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + 0x4A8) = cfg.mono;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + 0x4B0) = cfg.saturation;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + 0x4C0) = cfg.ghost;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + 0x4C8) = cfg.green;
        *reinterpret_cast<float*>(std::uintptr_t(memory->clientMode) + 0x4D0) = cfg.yellow;
    }

    if (config->visuals.noScopeOverlay)
        *memory->vignette = 0.0f;
}

static float __stdcall getScreenAspectRatio(int width, int height) noexcept
{
    if (config->visuals.aspectratio)
        return config->visuals.aspectratio;
    return hooks->engine.callOriginal<float, 101>(width, height);
}

static void __stdcall renderSmokeOverlay(bool update) noexcept
{
    if (config->visuals.noSmoke || config->visuals.wireframeSmoke)
        *reinterpret_cast<float*>(std::uintptr_t(memory->viewRender) + 0x588) = 0.0f;
    else
        hooks->viewRender.callOriginal<void, 41>(update);
}

Hooks::Hooks(HMODULE moduleHandle) noexcept
{
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    this->moduleHandle = moduleHandle;

    // Interfaces and memory shouldn't be initialized in wndProc because they show MessageBox on error which would cause deadlock
    interfaces = std::make_unique<const Interfaces>();
    memory = std::make_unique<const Memory>();

    window = FindWindowW(L"Valve001", nullptr);
    originalWndProc = WNDPROC(SetWindowLongPtrW(window, GWLP_WNDPROC, LONG_PTR(wndProc)));
}

void Hooks::install() noexcept
{
	originalPresent = **reinterpret_cast<decltype(originalPresent) **>(memory->present);
	**reinterpret_cast<decltype(present) ***>(memory->present) = present;
	originalReset = **reinterpret_cast<decltype(originalReset) **>(memory->reset);
	**reinterpret_cast<decltype(reset) ***>(memory->reset) = reset;

	if constexpr (std::is_same_v<HookType, MinHook>)
		MH_Initialize();

	bspQuery.init(interfaces->engine->getBSPTreeQuery());
	client.init(interfaces->client);
	clientMode.init(memory->clientMode);
	engine.init(interfaces->engine);
	modelRender.init(interfaces->modelRender);
	panel.init(interfaces->panel);
	sound.init(interfaces->sound);
	surface.init(interfaces->surface);
	svCheats.init(interfaces->cvar->findVar("sv_cheats"));
	viewRender.init(memory->viewRender);

	bspQuery.hookAt(6, listLeavesInBox);
	client.hookAt(37, frameStageNotify);
	clientMode.hookAt(17, shouldDrawFog);
	clientMode.hookAt(18, overrideView);
	clientMode.hookAt(24, createMove);
	clientMode.hookAt(27, shouldDrawViewModel);
	clientMode.hookAt(35, getViewModelFov);
	clientMode.hookAt(44, doPostScreenEffects);
	clientMode.hookAt(58, updateColorCorrectionWeights);
	engine.hookAt(82, isPlayingDemo);
	engine.hookAt(101, getScreenAspectRatio);
	engine.hookAt(218, getDemoPlaybackParameters);
	modelRender.hookAt(21, drawModelExecute);
	panel.hookAt(41, paintTraverse);
	sound.hookAt(5, emitSound);
	surface.hookAt(15, setDrawColor);
	surface.hookAt(67, lockCursor);
	svCheats.hookAt(13, svCheatsGetBool);
	viewRender.hookAt(39, render2dEffectsPreHud);
	viewRender.hookAt(41, renderSmokeOverlay);

	if (DWORD oldProtection; VirtualProtect(memory->dispatchSound, 4, PAGE_EXECUTE_READWRITE, &oldProtection))
	{
		originalDispatchSound = decltype(originalDispatchSound)(uintptr_t(memory->dispatchSound + 1) + *memory->dispatchSound);
		*memory->dispatchSound = uintptr_t(dispatchSound) - uintptr_t(memory->dispatchSound + 1);
		VirtualProtect(memory->dispatchSound, 4, oldProtection, nullptr);
	}

	if constexpr (std::is_same_v<HookType, MinHook>)
		MH_EnableHook(MH_ALL_HOOKS);
}

extern "C" BOOL WINAPI _CRT_INIT(HMODULE moduleHandle, DWORD reason, LPVOID reserved);

static DWORD WINAPI unload(HMODULE moduleHandle) noexcept
{
	Sleep(100);

	interfaces->inputSystem->enableInput(true);
	eventListener->remove();

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	_CRT_INIT(moduleHandle, DLL_PROCESS_DETACH, nullptr);

	FreeLibraryAndExitThread(moduleHandle, 0);
}

void Hooks::uninstall() noexcept
{
	if constexpr (std::is_same_v<HookType, MinHook>)
	{
		MH_DisableHook(MH_ALL_HOOKS);
		MH_Uninitialize();
	}

	bspQuery.restore();
	panel.restore();
	client.restore();
	clientMode.restore();
	engine.restore();
	modelRender.restore();
	sound.restore();
	surface.restore();
	svCheats.restore();
	viewRender.restore();
	if (netChInitialized) networkChannel.restore();

	netvars->restore();

	Glow::clearCustomObjects();

	SetWindowLongPtrW(window, GWLP_WNDPROC, LONG_PTR(originalWndProc));
	**reinterpret_cast<void ***>(memory->present) = originalPresent;
	**reinterpret_cast<void ***>(memory->reset) = originalReset;

	if (DWORD oldProtection; VirtualProtect(memory->dispatchSound, 4, PAGE_EXECUTE_READWRITE, &oldProtection))
	{
		*memory->dispatchSound = uintptr_t(originalDispatchSound) - uintptr_t(memory->dispatchSound + 1);
		VirtualProtect(memory->dispatchSound, 4, oldProtection, nullptr);
	}

	if (HANDLE thread = CreateThread(nullptr, 0, LPTHREAD_START_ROUTINE(unload), moduleHandle, 0, nullptr))
		CloseHandle(thread);
}
