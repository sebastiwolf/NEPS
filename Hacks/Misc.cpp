﻿#include <mutex>
#include <numeric>
#include <sstream>

#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../Netvars.h"

#include "EnginePrediction.h"
#include "Misc.h"
#include "Animations.h"

#include "../SDK/AnimState.h"
#include "../SDK/Client.h"
#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/FrameStage.h"
#include "../SDK/GameEvent.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/ItemSchema.h"
#include "../SDK/Input.h"
#include "../SDK/Localize.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/Panorama.h"
#include "../SDK/Sound.h"
#include "../SDK/Surface.h"
#include "../SDK/UserCmd.h"
#include "../SDK/WeaponData.h"
#include "../SDK/WeaponSystem.h"

#include "../GUI.h"
#include "../Helpers.h"
#include "../GameData.h"

#include "../imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../imgui/imgui_internal.h"
#include "../imguiCustom.h"

void Misc::edgejump(UserCmd* cmd) noexcept
{
    if (static Helpers::KeyBindState flag; !flag[config->movement.edgeJump])
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER)
        return;

    if ((EnginePrediction::getFlags() & Entity::FL_ONGROUND) && !(localPlayer->flags() & Entity::FL_ONGROUND))
        cmd->buttons |= UserCmd::IN_JUMP;
}

void Misc::slowwalk(UserCmd* cmd) noexcept
{
	if (static Helpers::KeyBindState flag; !flag[config->exploits.slowwalk])
		return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon)
        return;

    const auto weaponData = activeWeapon->getWeaponData();
    if (!weaponData)
        return;

    const float maxSpeed = (localPlayer->isScoped() ? weaponData->maxSpeedAlt : weaponData->maxSpeed) / 3;

    if (cmd->forwardmove && cmd->sidemove) {
        const float maxSpeedRoot = maxSpeed * static_cast<float>(M_SQRT1_2);
        cmd->forwardmove = cmd->forwardmove < 0.0f ? -maxSpeedRoot : maxSpeedRoot;
        cmd->sidemove = cmd->sidemove < 0.0f ? -maxSpeedRoot : maxSpeedRoot;
    } else if (cmd->forwardmove) {
        cmd->forwardmove = cmd->forwardmove < 0.0f ? -maxSpeed : maxSpeed;
    } else if (cmd->sidemove) {
        cmd->sidemove = cmd->sidemove < 0.0f ? -maxSpeed : maxSpeed;
    }
}

void Misc::updateClanTag() noexcept
{
	static std::string clanTag;
	static std::string clanTagBuffer;

	if (clanTag != config->griefing.clanTag)
	{
		clanTagBuffer = clanTag = config->griefing.clanTag;
		if (!clanTagBuffer.empty() && clanTagBuffer.front() != ' ' && clanTagBuffer.back() != ' ')
			clanTagBuffer.push_back(' ');
		return;
	}

	static auto lastTime = 0.0f;

	if (config->griefing.clocktag)
	{
		if (memory->globalVars->realtime - lastTime < 1.0f)
			return;

		const auto time = std::time(nullptr);
		const auto localTime = std::localtime(&time);
		char s[11];
		s[0] = '\0';
		sprintf_s(s, "[%02d:%02d:%02d]", localTime->tm_hour, localTime->tm_min, localTime->tm_sec);
		lastTime = memory->globalVars->realtime;
		memory->setClanTag(s, s);
	} else if (config->griefing.customClanTag)
	{
		if (memory->globalVars->realtime - lastTime < 0.6f)
			return;
		if (!clanTag.empty())
		{
			static int lastMode = 0;

			if (lastMode != config->griefing.animatedClanTag)
			{
				clanTagBuffer = clanTag;
				if (clanTagBuffer.front() != ' ' && clanTagBuffer.back() != ' ')
					clanTagBuffer.push_back(' ');
			}

			switch (config->griefing.animatedClanTag)
			{
			case 1:
			{
				const auto offset = Helpers::utf8SeqLen(clanTagBuffer[0]);
				if (offset != -1)
					std::rotate(clanTagBuffer.begin(), clanTagBuffer.begin() + offset, clanTagBuffer.end());
				break;
			}
			default:
				break;
			}

			lastMode = config->griefing.animatedClanTag;
		}

		lastTime = memory->globalVars->realtime;
		memory->setClanTag(clanTagBuffer.c_str(), clanTagBuffer.c_str());
	}
}

void Misc::spectatorList() noexcept
{
    if (!config->misc.spectatorList.enabled)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    interfaces->surface->setTextFont(Surface::font);

    const auto [width, height] = interfaces->surface->getScreenSize();

    auto textPositionY = static_cast<int>(0.5f * height);

    for (int i = 1; i <= interfaces->engine->getMaxClients(); ++i) {
        const auto entity = interfaces->entityList->getEntity(i);
        if (!entity || entity->isDormant() || entity->isAlive() || entity->getObserverTarget() != localPlayer.get())
            continue;

        PlayerInfo playerInfo;

        if (!interfaces->engine->getPlayerInfo(i, playerInfo))
            continue;

        if (wchar_t name[128]; MultiByteToWideChar(CP_UTF8, 0, playerInfo.name, -1, name, 128))
		{
            const auto [textWidth, textHeight] = interfaces->surface->getTextSize(Surface::font, name);

			interfaces->surface->setTextColor(0, 0, 0, 100);

            interfaces->surface->setTextPosition(width - textWidth - 5, textPositionY + 2);
            interfaces->surface->printText(name);
            interfaces->surface->setTextPosition(width - textWidth - 6, textPositionY + 1);
            interfaces->surface->printText(name);

			if (config->misc.spectatorList.rainbow)
				interfaces->surface->setTextColor(Helpers::rainbowColor(config->misc.spectatorList.rainbowSpeed));
			else
				interfaces->surface->setTextColor(config->misc.spectatorList.color);

            interfaces->surface->setTextPosition(width - textWidth - 7, textPositionY);
            interfaces->surface->printText(name);

            textPositionY += textHeight;
        }
    }

	const auto title = L"Spectators";

	const auto [titleWidth, titleHeight] = interfaces->surface->getTextSize(Surface::font, title);
	textPositionY = static_cast<int>(0.5f * height);
	
	if (config->misc.specBg.rainbow)
		interfaces->surface->setDrawColor(Helpers::rainbowColor(config->misc.specBg.rainbowSpeed), static_cast<int>(config->misc.specBg.color[3] * 255));
	else
		interfaces->surface->setDrawColor(static_cast<int>(config->misc.specBg.color[0] * 255), static_cast<int>(config->misc.specBg.color[1] * 255), static_cast<int>(config->misc.specBg.color[2] * 255), static_cast<int>(config->misc.specBg.color[3] * 255));

	interfaces->surface->drawFilledRect(width - titleWidth - 15, textPositionY - titleHeight - 12, width, textPositionY);

	if (config->misc.spectatorList.rainbow)
		interfaces->surface->setDrawColor(Helpers::rainbowColor(config->misc.spectatorList.rainbowSpeed), 255);
	else
		interfaces->surface->setDrawColor(static_cast<int>(config->misc.spectatorList.color[0] * 255), static_cast<int>(config->misc.spectatorList.color[1] * 255), static_cast<int>(config->misc.spectatorList.color[2] * 255), 255);

	interfaces->surface->drawOutlinedRect(width - titleWidth - 15, textPositionY - titleHeight - 12, width, textPositionY);

	interfaces->surface->setTextColor(0, 0, 0, 100);

	interfaces->surface->setTextPosition(width - titleWidth - 5, textPositionY - titleHeight - 5);
	interfaces->surface->printText(title);

	interfaces->surface->setTextPosition(width - titleWidth - 6, textPositionY - titleHeight - 6);
	interfaces->surface->printText(title);

	if (config->misc.spectatorList.rainbow)
		interfaces->surface->setTextColor(Helpers::rainbowColor(config->misc.spectatorList.rainbowSpeed));
	else
		interfaces->surface->setTextColor(config->misc.spectatorList.color);

	interfaces->surface->setTextPosition(width - titleWidth - 7, textPositionY - titleHeight - 7);
	interfaces->surface->printText(title);
}

void Misc::spectatorList(ImDrawList *drawList) noexcept
{
	if (!config->misc.spectatorList.enabled)
		return;

	if (!localPlayer)
		return;
	
	std::vector<const char *> observers;

	GameData::Lock lock;
	for (auto &observer : GameData::observers())
	{
		if (observer.targetIsObservedByLocalPlayer || observer.targetIsLocalPlayer)
			observers.emplace_back(observer.name);
	}

	if (!observers.empty() || gui->open)
	{
		ImGui::SetNextWindowPos(ImVec2{ImGui::GetIO().DisplaySize.x - 200.0f, ImGui::GetIO().DisplaySize.y / 2 - 20.0f}, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2{200.0f, 0.0f}, ImVec2{FLT_MAX, FLT_MAX});
		ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, {0.5f, 0.5f});
		if (ImGui::Begin("Spectators", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
		{

			for (auto &observer : observers)
			{
				ImGui::TextUnformatted(observer);
			}
			ImGui::End();
		}
		ImGui::PopStyleVar();
	}
}

void Misc::watermark() noexcept
{
    if (config->misc.watermark.enabled) {
        interfaces->surface->setTextFont(Surface::font);

		const auto watermark = L"Welcome to NEPS.PP";

        static auto frameRate = 1.0f;
        frameRate = 0.9f * frameRate + 0.1f * memory->globalVars->absoluteFrameTime;
        const auto [screenWidth, screenHeight] = interfaces->surface->getScreenSize();
        std::wstring fps{ std::to_wstring(static_cast<int>(1 / frameRate)) + L" fps" };
        const auto [fpsWidth, fpsHeight] = interfaces->surface->getTextSize(Surface::font, fps.c_str());

        float latency = 0.0f;
        if (auto networkChannel = interfaces->engine->getNetworkChannel(); networkChannel && networkChannel->getLatency(0) > 0.0f)
            latency = networkChannel->getLatency(0);

        std::wstring ping{ L"Ping: " + std::to_wstring(static_cast<int>(latency * 1000)) + L" ms" };
        const auto [pingWidth, pingHeight] = interfaces->surface->getTextSize(Surface::font, ping.c_str());

		const auto [waterWidth, waterHeight] = interfaces->surface->getTextSize(Surface::font, watermark);

		interfaces->surface->setTextColor(0, 0, 0, 100);

		if (config->misc.bg.rainbow)
			interfaces->surface->setDrawColor(Helpers::rainbowColor(config->misc.bg.rainbowSpeed), static_cast<int>(config->misc.bg.color[3] * 255));
		else
			interfaces->surface->setDrawColor(static_cast<int>(config->misc.bg.color[0] * 255), static_cast<int>(config->misc.bg.color[1] * 255), static_cast<int>(config->misc.bg.color[2] * 255), static_cast<int>(config->misc.bg.color[3] * 255));

		interfaces->surface->drawFilledRect(screenWidth - max(pingWidth, fpsWidth) - 14, 0, screenWidth, fpsHeight + pingHeight + 12);

		interfaces->surface->setTextPosition(screenWidth - pingWidth - 5, fpsHeight + 6);
		interfaces->surface->printText(ping.c_str());
		interfaces->surface->setTextPosition(screenWidth - pingWidth - 6, fpsHeight + 7);
		interfaces->surface->printText(ping.c_str());

		interfaces->surface->setTextPosition(screenWidth - fpsWidth - 5, 6);
		interfaces->surface->printText(fps.c_str());
		interfaces->surface->setTextPosition(screenWidth - fpsWidth - 6, 7);
		interfaces->surface->printText(fps.c_str());

		interfaces->surface->drawFilledRect(0, 0, waterWidth + 14, waterHeight + 11);

		interfaces->surface->setTextPosition(5, 6);
		interfaces->surface->printText(watermark);
		interfaces->surface->setTextPosition(6, 7);
		interfaces->surface->printText(watermark);

		if (config->misc.watermark.rainbow)
			interfaces->surface->setTextColor(Helpers::rainbowColor(config->misc.watermark.rainbowSpeed));
		else
			interfaces->surface->setTextColor(config->misc.watermark.color);

        interfaces->surface->setTextPosition(screenWidth - pingWidth - 7, fpsHeight + 5);
        interfaces->surface->printText(ping.c_str());

		interfaces->surface->setTextPosition(screenWidth - fpsWidth - 7, 5);
		interfaces->surface->printText(fps.c_str());

		interfaces->surface->setTextPosition(7, 5);
		interfaces->surface->printText(watermark);

		if (config->misc.watermark.rainbow)
			interfaces->surface->setDrawColor(Helpers::rainbowColor(config->misc.watermark.rainbowSpeed));
		else
			interfaces->surface->setDrawColor(static_cast<int>(config->misc.watermark.color[0] * 255), static_cast<int>(config->misc.watermark.color[1] * 255), static_cast<int>(config->misc.watermark.color[2] * 255), 255);

		interfaces->surface->drawOutlinedRect(screenWidth - max(pingWidth, fpsWidth) - 14, 0, screenWidth, fpsHeight + pingHeight + 12);
		interfaces->surface->drawOutlinedRect(0, 0, waterWidth + 14, waterHeight + 11);
    }
}

void Misc::watermark(ImDrawList *drawList) noexcept
{
	if (!config->misc.watermark.enabled)
		return;

	ImGui::SetNextWindowPos(ImVec2{0.0f, 0.0f}, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Welcome to NEPS", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
	{
		const auto watermark = "Welcome to NEPS";
		static float frameRate = 1.0f;
		frameRate = 0.9f * frameRate + 0.1f * memory->globalVars->absoluteFrameTime;

		ImGui::TextUnformatted(watermark);
		ImGui::TextUnformatted((std::to_string(static_cast<int>(1 / frameRate)) + "fps").c_str());
		if (interfaces->engine->isInGame())
		{
			float latency = 0.0f;
			float tickrate = 1.0f;
			const auto networkChannel = interfaces->engine->getNetworkChannel();
			if (networkChannel)
			{
				latency = networkChannel->getLatency(0) * 1000;
				tickrate = 1 / memory->globalVars->intervalPerTick;
			}

			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine();
			ImGui::TextUnformatted((std::to_string(static_cast<int>(latency)) + "ms").c_str());
			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine();
			ImGui::TextUnformatted((std::to_string(static_cast<int>(tickrate)) + "tick").c_str());
		}
		ImGui::End();
	}
}

static void drawCrosshair(ImDrawList *drawList, ImVec2 pos, ImU32 color)
{
	bool aa = false;
	if (drawList->Flags & ImDrawListFlags_AntiAliasedFill)
	{
		drawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;
		aa = true;
	}

	float th = interfaces->cvar->findVar("cl_crosshairthickness")->getFloat();
	float gp = interfaces->cvar->findVar("cl_crosshairgap")->getFloat();
	float sz = interfaces->cvar->findVar("cl_crosshairsize")->getFloat();
	pos = pos + ImVec2(0.5f, 0.5f);
	th += 1.0f;
	gp *= 2.0f;
	gp += th;
	sz *= 2.0f;
	sz -= th;
	th *= 0.5f;
	drawList->AddRectFilled(pos + ImVec2(-th, -gp - sz), pos + ImVec2(th, -gp), color);
	drawList->AddRectFilled(pos + ImVec2(-th, gp), pos + ImVec2(th, gp + sz), color);
	drawList->AddRectFilled(pos + ImVec2(-gp - sz, -th), pos + ImVec2(-gp, th), color);
	drawList->AddRectFilled(pos + ImVec2(gp, -th), pos + ImVec2(gp + sz, th), color);

	if (aa)
		drawList->Flags |= ImDrawListFlags_AntiAliasedFill;
}

void Misc::noscopeCrosshair(ImDrawList* drawList) noexcept
{
    if (!config->misc.noscopeCrosshair.enabled)
        return;

    GameData::Lock lock;
    const auto& local = GameData::local();

    if (!local.exists || !local.alive /*|| !local.noScope*/) // Changed to overlay
        return;

	drawCrosshair(drawList, ImGui::GetIO().DisplaySize / 2, Helpers::calculateColor(config->misc.noscopeCrosshair));
}

void Misc::recoilCrosshair(ImDrawList* drawList) noexcept
{
    if (!config->misc.recoilCrosshair.enabled)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

	GameData::Lock lock;
	const auto aimPunch = GameData::local().aimPunch;

	auto col = config->misc.recoilCrosshair;
	col.color[3] *= std::clamp(GameData::local().aimPunchAngle.length2D() * 10.0f, 0.0f, 1.0f);
	
	if (ImVec2 pos; Helpers::worldToScreen(aimPunch, pos))
	{
		drawCrosshair(drawList, pos, Helpers::calculateColor(col));
	}
}

void Misc::prepareRevolver(UserCmd *cmd) noexcept
{
	constexpr float revolverPrepareTime{0.234375f};

	static float readyTime;
	if (static Helpers::KeyBindState flag; flag[config->misc.prepareRevolver])
	{
		if (auto activeWeapon = localPlayer->getActiveWeapon(); activeWeapon && activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver)
		{
			if (!readyTime) readyTime = memory->globalVars->serverTime() + revolverPrepareTime;
			auto ticksToReady = Helpers::timeToTicks(readyTime - memory->globalVars->serverTime() - interfaces->engine->getNetworkChannel()->getLatency(0));
			if (ticksToReady > 0 && ticksToReady <= Helpers::timeToTicks(revolverPrepareTime))
				cmd->buttons |= UserCmd::IN_ATTACK;
			else
				readyTime = 0.0f;
		}
	}
}

void Misc::fastPlant(UserCmd* cmd) noexcept
{
    if (!config->misc.fastPlant)
        return;

    static auto plantAnywhere = interfaces->cvar->findVar("mp_plant_c4_anywhere");

    if (plantAnywhere->getInt())
        return;

    if (!localPlayer || !localPlayer->isAlive() || (localPlayer->inBombZone() && localPlayer->flags() & Entity::FL_ONGROUND))
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->isC4())
        return;

    cmd->buttons &= ~UserCmd::IN_ATTACK;

    constexpr auto doorRange = 200.0f;

    Trace trace;
    const auto startPos = localPlayer->getEyePosition();
    const auto endPos = startPos + Vector::fromAngle(cmd->viewangles) * doorRange;
    interfaces->engineTrace->traceRay({ startPos, endPos }, 0x46004009, localPlayer.get(), trace);

    if (!trace.entity || trace.entity->getClientClass()->classId != ClassId::PropDoorRotating)
        cmd->buttons &= ~UserCmd::IN_USE;
}

void Misc::fastStop(UserCmd* cmd) noexcept
{
    if (!config->movement.fastStop)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER || !(localPlayer->flags() & 1) || cmd->buttons & UserCmd::IN_JUMP)
        return;

    if (cmd->buttons & (UserCmd::IN_MOVELEFT | UserCmd::IN_MOVERIGHT | UserCmd::IN_FORWARD | UserCmd::IN_BACK))
        return;
    
    const auto velocity = localPlayer->velocity();
    const auto speed = velocity.length2D();
    if (speed < 15.0f)
        return;
    
    float direction = velocity.toAngle2D();
    direction = cmd->viewangles.y - direction;

    const auto negatedDirection = Vector::fromAngle2D(direction) * -speed;
    cmd->forwardmove = negatedDirection.x;
    cmd->sidemove = negatedDirection.y;
}

void Misc::drawBombTimer() noexcept
{
	if (!config->misc.bombTimer)
		return;

	GameData::Lock lock;
	const auto &plantedC4 = GameData::plantedC4();
	if (plantedC4.blowTime == 0.0f && !gui->open)
		return;

	static float windowWidth = 500.0f;
	ImGui::SetNextWindowPos({(ImGui::GetIO().DisplaySize.x - 500.0f) / 2.0f, 160.0f}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({windowWidth, 0}, ImGuiCond_FirstUseEver);

	if (!gui->open)
		ImGui::SetNextWindowSize({windowWidth, 0});

	ImGui::SetNextWindowSizeConstraints({200, -1}, {FLT_MAX, -1});
	if (ImGui::Begin("Bomb timer", nullptr, ImGuiWindowFlags_NoTitleBar | (gui->open ? 0 : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize)))
	{
		std::ostringstream ss; ss << "Bomb on " << (!plantedC4.bombsite ? 'A' : 'B') << " " << std::fixed << std::showpoint << std::setprecision(3) << (std::max)(plantedC4.blowTime - memory->globalVars->currenttime, 0.0f) << " s";

		ImGuiCustom::textUnformattedCentered(ss.str().c_str());

		ImGuiCustom::progressBarFullWidth((plantedC4.blowTime - memory->globalVars->currenttime) / plantedC4.timerLength);

		if (plantedC4.defuserHandle != -1)
		{
			const bool canDefuse = plantedC4.blowTime >= plantedC4.defuseCountDown;

			if (plantedC4.defuserHandle == GameData::local().handle)
			{
				std::ostringstream ss; ss << "Defusing... " << std::fixed << std::showpoint << std::setprecision(3) << (std::max)(plantedC4.defuseCountDown - memory->globalVars->currenttime, 0.0f) << " s";

				ImGuiCustom::textUnformattedCentered(ss.str().c_str());
			} else if (auto playerData = GameData::playerByHandle(plantedC4.defuserHandle))
			{
				std::ostringstream ss; ss << playerData->name << " is defusing... " << std::fixed << std::showpoint << std::setprecision(3) << (std::max)(plantedC4.defuseCountDown - memory->globalVars->currenttime, 0.0f) << " s";

				ImGuiCustom::textUnformattedCentered(ss.str().c_str());
			}

			ImGuiCustom::progressBarFullWidth((plantedC4.defuseCountDown - memory->globalVars->currenttime) / plantedC4.defuseLength);

			if (canDefuse)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
				ImGuiCustom::textUnformattedCentered("CAN DEFUSE");
				ImGui::PopStyleColor();
			} else
			{
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
				ImGuiCustom::textUnformattedCentered("CANNOT DEFUSE");
				ImGui::PopStyleColor();
			}
		}

		windowWidth = ImGui::GetCurrentWindow()->SizeFull.x;
		ImGui::End();
	}
}

void Misc::stealNames() noexcept
{
    if (!config->griefing.nameStealer)
        return;

    if (!localPlayer)
        return;

    static std::vector<int> stolenIds;

    for (int i = 1; i <= memory->globalVars->maxClients; ++i) {
        const auto entity = interfaces->entityList->getEntity(i);

        if (!entity || entity == localPlayer.get())
            continue;

        PlayerInfo playerInfo;
        if (!interfaces->engine->getPlayerInfo(entity->index(), playerInfo))
            continue;

        if (playerInfo.fakeplayer || std::find(stolenIds.cbegin(), stolenIds.cend(), playerInfo.userId) != stolenIds.cend())
            continue;

        if (changeName(false, (std::string{ playerInfo.name } + '\x1').c_str(), 1.0f))
            stolenIds.emplace_back(playerInfo.userId);

        return;
    }
    stolenIds.clear();
}

void Misc::quickReload(UserCmd* cmd) noexcept
{
    if (config->misc.quickReload) {
        static Entity* reloadedWeapon{ nullptr };

        if (reloadedWeapon) {
            for (auto weaponHandle : localPlayer->weapons()) {
                if (weaponHandle == -1)
                    break;

                if (interfaces->entityList->getEntityFromHandle(weaponHandle) == reloadedWeapon) {
                    cmd->weaponselect = reloadedWeapon->index();
                    cmd->weaponsubtype = reloadedWeapon->getWeaponSubType();
                    break;
                }
            }
            reloadedWeapon = nullptr;
        }

        if (auto activeWeapon = localPlayer->getActiveWeapon(); activeWeapon && activeWeapon->isInReload() && activeWeapon->clip() == activeWeapon->getWeaponData()->maxClip) {
            reloadedWeapon = activeWeapon;

            for (auto weaponHandle : localPlayer->weapons()) {
                if (weaponHandle == -1)
                    break;

                if (auto weapon = interfaces->entityList->getEntityFromHandle(weaponHandle); weapon && weapon != reloadedWeapon) {
                    cmd->weaponselect = weapon->index();
                    cmd->weaponsubtype = weapon->getWeaponSubType();
                    break;
                }
            }
        }
    }
}

bool Misc::changeName(bool reconnect, const char* newName, float delay) noexcept
{
    static auto exploitInitialized = false;

    static auto name = interfaces->cvar->findVar("name");

    if (reconnect) {
        exploitInitialized = false;
        return false;
    }

    if (!exploitInitialized && interfaces->engine->isInGame()) {
        if (PlayerInfo playerInfo; localPlayer && interfaces->engine->getPlayerInfo(localPlayer->index(), playerInfo) && (!strcmp(playerInfo.name, "?empty") || !strcmp(playerInfo.name, "\n\xAD\xAD\xAD"))) {
            exploitInitialized = true;
        } else {
            name->onChangeCallbacks.size = 0;
            name->setValue("\n\xAD\xAD\xAD");
            return false;
        }
    }

    static auto nextChangeTime{ 0.0f };
    if (nextChangeTime <= memory->globalVars->realtime) {
        name->setValue(newName);
        nextChangeTime = memory->globalVars->realtime + delay;
        return true;
    }
    return false;
}

void Misc::bunnyHop(UserCmd* cmd) noexcept
{
    if (!localPlayer)
        return;

    static auto wasLastTimeOnGround = localPlayer->flags() & Entity::FL_ONGROUND;

    if (config->movement.bunnyHop && !(localPlayer->flags() & Entity::FL_ONGROUND) && localPlayer->moveType() != MoveType::LADDER && !wasLastTimeOnGround)
        cmd->buttons &= ~UserCmd::IN_JUMP;

    wasLastTimeOnGround = localPlayer->flags() & Entity::FL_ONGROUND;
}

void Misc::fakeBan(bool set) noexcept
{
    static bool shouldSet = false;

    if (set)
        shouldSet = set;

    if (shouldSet && interfaces->engine->isInGame() && changeName(false, std::string{ "\x1\xB" }.append(std::string{ static_cast<char>(config->griefing.banColor + 1) }).append(config->griefing.banText).append("\x1").c_str(), 5.0f))
        shouldSet = false;
}

void Misc::changeConVarsTick() noexcept
{
	static auto tracerVar = interfaces->cvar->findVar("cl_weapon_debug_show_accuracy");
	tracerVar->onChangeCallbacks.size = 0;
	tracerVar->setValue(config->visuals.accuracyTracers);
	static auto impactsVar = interfaces->cvar->findVar("sv_showimpacts");
	impactsVar->onChangeCallbacks.size = 0;
	impactsVar->setValue(config->visuals.bulletImpacts);
    static auto nadeVar = interfaces->cvar->findVar("cl_grenadepreview");
    nadeVar->onChangeCallbacks.size = 0;
	nadeVar->setValue(config->misc.nadePredict);
    static auto modelVar = interfaces->cvar->findVar("r_drawmodelnames");
    modelVar->onChangeCallbacks.size = 0;
	modelVar->setValue(config->visuals.modelNames);
	static auto shadowVar = interfaces->cvar->findVar("cl_csm_enabled");
	shadowVar->setValue(!config->visuals.noShadows);
	static auto lerpVar = interfaces->cvar->findVar("cl_interpolate");
	lerpVar->setValue(true);
	static auto jigglebonesVar = interfaces->cvar->findVar("r_jiggle_bones");
	jigglebonesVar->setValue(false);
	static auto ragdollGravity = interfaces->cvar->findVar("cl_ragdoll_gravity");
	ragdollGravity->setValue(config->visuals.inverseRagdollGravity ? -600 : 600);
}

static void oppositeHandKnife(FrameStage stage) noexcept
{
    static const auto rightHandVar = interfaces->cvar->findVar("cl_righthand");
	static bool original = rightHandVar->getInt();

    if (!localPlayer)
        return;

    if (stage != FrameStage::RENDER_START && stage != FrameStage::RENDER_END)
        return;

    if (!config->visuals.oppositeHandKnife)
    {
        rightHandVar->setValue(original);
        return;
    }

    if (stage == FrameStage::RENDER_START) {
        if (const auto activeWeapon = localPlayer->getActiveWeapon()) {
            if (const auto classId = activeWeapon->getClientClass()->classId; classId == ClassId::Knife || classId == ClassId::KnifeGG)
				rightHandVar->setValue(!original);
        }
    } else {
		rightHandVar->setValue(original);
    }
}

static void camDist(FrameStage stage)
{
	if (stage == FrameStage::RENDER_START)
	{
		static auto distVar = interfaces->cvar->findVar("cam_idealdist");
		static auto curDist = 0.0f;
		if (memory->input->isCameraInThirdPerson)
			curDist = Helpers::approachValSmooth(static_cast<float>(config->visuals.thirdpersonDistance), curDist, memory->globalVars->frametime * 7.0f);
		else
			curDist = 0.0f;

		distVar->setValue(curDist);
	}
}

void Misc::changeConVarsFrame(FrameStage stage)
{
	switch (stage)
	{
	case FrameStage::UNDEFINED:
		break;
	case FrameStage::START:
		break;
	case FrameStage::NET_UPDATE_START:
		break;
	case FrameStage::NET_UPDATE_POSTDATAUPDATE_START:
		break;
	case FrameStage::NET_UPDATE_POSTDATAUPDATE_END:
		break;
	case FrameStage::NET_UPDATE_END:
		break;
	case FrameStage::RENDER_START:
		static auto mdlVar = interfaces->cvar->findVar("r_drawothermodels");
		mdlVar->onChangeCallbacks.size = 0;
		if (mdlVar->getInt() == 1) mdlVar->setValue(3);
		static auto blurVar = interfaces->cvar->findVar("@panorama_disable_blur");
		blurVar->setValue(config->misc.disablePanoramablur);
		static auto lagVar = interfaces->cvar->findVar("cam_ideallag");
		lagVar->setValue(0);
		static auto minVar = interfaces->cvar->findVar("c_mindistance");
		minVar->setValue(-FLT_MAX);
		static auto maxVar = interfaces->cvar->findVar("c_maxdistance");
		maxVar->setValue(FLT_MAX);
		static auto propsVar = interfaces->cvar->findVar("r_DrawSpecificStaticProp");
		propsVar->setValue(config->visuals.props.enabled ? 0 : -1);
		break;
	case FrameStage::RENDER_END:
		static auto sky = interfaces->cvar->findVar("r_3dsky");
		sky->setValue(!config->visuals.no3dSky);
		break;
	}

	camDist(stage);
	oppositeHandKnife(stage);
}

void Misc::quickHealthshot(UserCmd* cmd) noexcept
{
    if (!localPlayer)
        return;

    static bool inProgress{ false };

    if (GetAsyncKeyState(config->misc.quickHealthshotKey) & 1)
        inProgress = true;

    if (auto activeWeapon{ localPlayer->getActiveWeapon() }; activeWeapon && inProgress) {
        if (activeWeapon->getClientClass()->classId == ClassId::Healthshot && localPlayer->nextAttack() <= memory->globalVars->serverTime() && activeWeapon->nextPrimaryAttack() <= memory->globalVars->serverTime())
            cmd->buttons |= UserCmd::IN_ATTACK;
        else {
            for (auto weaponHandle : localPlayer->weapons()) {
                if (weaponHandle == -1)
                    break;

                if (const auto weapon{ interfaces->entityList->getEntityFromHandle(weaponHandle) }; weapon && weapon->getClientClass()->classId == ClassId::Healthshot) {
                    cmd->weaponselect = weapon->index();
                    cmd->weaponsubtype = weapon->getWeaponSubType();
                    return;
                }
            }
        }
        inProgress = false;
    }
}

void Misc::fixTabletSignal() noexcept
{
    if (config->misc.fixTabletSignal && localPlayer) {
        if (auto activeWeapon{ localPlayer->getActiveWeapon() }; activeWeapon && activeWeapon->getClientClass()->classId == ClassId::Tablet)
            activeWeapon->tabletReceptionIsBlocked() = false;
    }
}

void Misc::fakePrime() noexcept
{
    static bool lastState = false;

    if (config->griefing.fakePrime != lastState) {
        lastState = config->griefing.fakePrime;

        if (DWORD oldProtect; VirtualProtect(memory->fakePrime, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            constexpr uint8_t patch[]{ 0x74, 0xEB };
            *memory->fakePrime = patch[config->griefing.fakePrime];
            VirtualProtect(memory->fakePrime, 1, oldProtect, nullptr);
        }
    }
}

void Misc::killMessage(GameEvent& event) noexcept
{
    if (!config->griefing.killMessage)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
        return;

    std::string cmd = "say \"";
    cmd += config->griefing.killMessageString;
    cmd += '"';
    interfaces->engine->clientCmdUnrestricted(cmd.c_str());
}

void Misc::fixMovement(UserCmd* cmd, float yaw) noexcept
{
    if (config->misc.fixMovement) {
        float oldYaw = yaw + (yaw < 0.0f ? 360.0f : 0.0f);
        float newYaw = cmd->viewangles.y + (cmd->viewangles.y < 0.0f ? 360.0f : 0.0f);
        float yawDelta = newYaw < oldYaw ? fabsf(newYaw - oldYaw) : 360.0f - fabsf(newYaw - oldYaw);
        yawDelta = 360.0f - yawDelta;

        const float forwardmove = cmd->forwardmove;
        const float sidemove = cmd->sidemove;
        cmd->forwardmove = std::cos(Helpers::degreesToRadians(yawDelta)) * forwardmove + std::cos(Helpers::degreesToRadians(yawDelta + 90.0f)) * sidemove;
        cmd->sidemove = std::sin(Helpers::degreesToRadians(yawDelta)) * forwardmove + std::sin(Helpers::degreesToRadians(yawDelta + 90.0f)) * sidemove;
    }
}

void Misc::antiAfkKick(UserCmd* cmd) noexcept
{
    if (config->exploits.antiAfkKick && cmd->commandNumber % 2)
        cmd->buttons |= 1 << 26;
}

void Misc::fixAnimationLOD(FrameStage stage) noexcept
{
    if (config->misc.fixAnimationLOD && stage == FrameStage::RENDER_START) {
        if (!localPlayer)
            return;

        for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
            Entity* entity = interfaces->entityList->getEntity(i);
            if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()) continue;
            *reinterpret_cast<int*>(entity + 0xA28) = 0;
            *reinterpret_cast<int*>(entity + 0xA30) = memory->globalVars->framecount;
        }
    }
}

void Misc::autoPistol(UserCmd *cmd) noexcept
{
	if (config->misc.autoPistol && localPlayer)
	{
		const auto activeWeapon = localPlayer->getActiveWeapon();
		if (activeWeapon && !activeWeapon->isC4() && activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime() && !activeWeapon->isGrenade())
		{
			if (activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver)
				cmd->buttons &= ~UserCmd::IN_ATTACK2;
			else
				cmd->buttons &= ~UserCmd::IN_ATTACK;
		}
	}
}

void Misc::autoReload(UserCmd* cmd) noexcept
{
    if (config->misc.autoReload && localPlayer) {
        const auto activeWeapon = localPlayer->getActiveWeapon();
        if (activeWeapon && getWeaponIndex(activeWeapon->itemDefinitionIndex2()) && !activeWeapon->clip())
            cmd->buttons &= ~(UserCmd::IN_ATTACK | UserCmd::IN_ATTACK2);
    }
}

void Misc::revealRanks(UserCmd* cmd) noexcept
{
    if (config->misc.revealRanks && cmd->buttons & UserCmd::IN_SCORE)
        interfaces->client->dispatchUserMessage(50, 0, 0, nullptr);
}

static float idealAngleDelta2(float speed, bool crouch)
{
	// TODO: account server constants into calculation
	//static const auto airaccelVar = interfaces->cvar->findVar("sv_airaccelerate");
	//static const auto speedVar = interfaces->cvar->findVar("sv_maxspeed");

	//const float accel = crouch ? airaccelVar->getFloat() * 0.33333333f : airaccelVar->getFloat();
	//const float max = std::fminf(speedVar->getFloat(), 30.0f);

	//return std::fminf(std::asinf(std::fminf(accel, max) / speed), RAD90) * 0.8f;
	float steer = crouch ? config->movement.steerSpeed * 0.33333333f : config->movement.steerSpeed;
	return std::fminf(std::asinf(steer / speed), RAD90);
}

static float idealAngleDelta3(float speed)
{
	return std::fminf(std::atanf(30.0f / speed), RAD90);
}

void Misc::autoStrafe(UserCmd* cmd) noexcept
{
	if (!config->movement.autoStrafe || !config->movement.bunnyHop)
		return;

	if (!localPlayer || localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER)
		return;

	static auto wasLastTimeOnGround = localPlayer->flags() & Entity::FL_ONGROUND;

	if (~cmd->buttons & UserCmd::IN_JUMP || localPlayer->flags() & Entity::FL_ONGROUND && wasLastTimeOnGround)
		return;

	const float curSpeed = localPlayer->velocity().length2D();
	const float idealDelta = idealAngleDelta2(curSpeed, localPlayer->flags() & Entity::FL_DUCKING);
	const float deltaAt90 = idealAngleDelta3(curSpeed);

	if (idealDelta == 0.0f)
		return;

	const float yawRad = Helpers::degreesToRadians(cmd->viewangles.y);

	const float curVel = curSpeed != 0.0f ? std::atan2f(localPlayer->velocity().y, localPlayer->velocity().x) - yawRad : 0.0f;
	static float prevVel = curVel;
	const float wishAng = std::atan2f(-cmd->sidemove, cmd->forwardmove);
	const float wishSpeed = Vector{-cmd->sidemove, cmd->forwardmove, cmd->upmove}.length();

	const float angleDelta = Helpers::angleDiffRad(curVel, wishAng);
	float move = angleDelta < 0.0f ? curVel + RAD90 - deltaAt90 + idealDelta : curVel - RAD90 + deltaAt90 - idealDelta;

	cmd->forwardmove = std::cosf(move) * 450.0f;
	cmd->sidemove = -std::sinf(move) * 450.0f;

	prevVel = curVel;
	wasLastTimeOnGround = localPlayer->flags() & Entity::FL_ONGROUND;
}

void Misc::removeCrouchCooldown(UserCmd* cmd) noexcept
{
    if (config->exploits.fastDuck)
        cmd->buttons |= UserCmd::IN_BULLRUSH;
}

void Misc::moonwalk(UserCmd* cmd, bool &sendPacket) noexcept
{
	if (!localPlayer || localPlayer->moveType() == MoveType::LADDER)
		return;

	cmd->buttons &= ~(UserCmd::IN_FORWARD | UserCmd::IN_BACK | UserCmd::IN_MOVELEFT | UserCmd::IN_MOVERIGHT);
	if (cmd->forwardmove > 0.0f)
		cmd->buttons |= UserCmd::IN_FORWARD;
	else if (cmd->forwardmove < 0.0f)
		cmd->buttons |= UserCmd::IN_BACK;
	if (cmd->sidemove > 0.0f)
		cmd->buttons |= UserCmd::IN_MOVERIGHT;
	else if (cmd->sidemove < 0.0f)
		cmd->buttons |= UserCmd::IN_MOVELEFT;

	if (config->exploits.moonwalk)
	{
		cmd->buttons ^= UserCmd::IN_FORWARD | UserCmd::IN_BACK | UserCmd::IN_MOVELEFT | UserCmd::IN_MOVERIGHT;
	}
}

void Misc::playHitSound(GameEvent &event) noexcept
{
	if (!config->sound.hitSound)
		return;

	if (!localPlayer)
		return;

	if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
		return;

	constexpr std::array hitSounds = {
		"play physics/metal/metal_solid_impact_bullet2",
		"play buttons/arena_switch_press_02",
		"play training/timer_bell",
		"play physics/glass/glass_impact_bullet1"
	};

	if (static_cast<std::size_t>(config->sound.hitSound - 1) < hitSounds.size())
		//interfaces->engine->clientCmdUnrestricted((std::string("playvol ") + hitSounds[config->sound.hitSound - 1] + ' ' + std::to_string(config->sound.hitSoundVol)).c_str(), true);
		interfaces->engine->clientCmdUnrestricted(hitSounds[config->sound.hitSound - 1]);
	else if (config->sound.hitSound == 5)
		//interfaces->engine->clientCmdUnrestricted((std::string("playvol ") + config->sound.customHitSound + ' ' + std::to_string(config->sound.hitSoundVol)).c_str(), true);
		interfaces->engine->clientCmdUnrestricted(("play " + config->sound.customHitSound).c_str());
}

void Misc::killSound(GameEvent& event) noexcept
{
    if (!config->sound.killSound)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
        return;

    constexpr std::array killSounds = {
        "play physics/metal/metal_solid_impact_bullet2",
        "play buttons/arena_switch_press_02",
        "play training/timer_bell",
        "play physics/glass/glass_impact_bullet1"
    };

    if (static_cast<std::size_t>(config->sound.killSound - 1) < killSounds.size())
        //interfaces->engine->clientCmdUnrestricted((std::string("playvol ") + killSounds[config->sound.killSound - 1] + ' ' + std::to_string(config->sound.killSoundVol)).c_str(), true);
        interfaces->engine->clientCmdUnrestricted(killSounds[config->sound.killSound - 1]);
	else if (config->sound.killSound == 5)
        //interfaces->engine->clientCmdUnrestricted((std::string("playvol ") + config->sound.customKillSound + ' ' + std::to_string(config->sound.killSoundVol)).c_str(), true);
        interfaces->engine->clientCmdUnrestricted(("play " + config->sound.customKillSound).c_str());
}

void Misc::purchaseList(GameEvent* event) noexcept
{
    static std::mutex mtx;
    std::scoped_lock _{ mtx };

	struct PlayerPurchases
	{
		int totalCost;
		std::unordered_map<std::string, int> items;
	};

	static std::unordered_map<int, PlayerPurchases> playerPurchases;
    static std::unordered_map<std::string, int> purchaseTotal;
    static int totalCost;

    static auto freezeEnd = 0.0f;

    if (event) {
        switch (fnv::hashRuntime(event->getName())) {
        case fnv::hash("item_purchase"): {
            const auto player = interfaces->entityList->getEntity(interfaces->engine->getPlayerForUserID(event->getInt("userid")));

            if (player && localPlayer && memory->isOtherEnemy(player, localPlayer.get())) {
                if (const auto definition = memory->itemSystem()->getItemSchema()->getItemDefinitionByName(event->getString("weapon"))) {
					auto &purchase = playerPurchases[player->handle()];
                    if (const auto weaponInfo = memory->weaponSystem->getWeaponInfo(definition->getWeaponId())) {
						purchase.totalCost += weaponInfo->price;
                        totalCost += weaponInfo->price;
                    }
                    const std::string weapon = interfaces->localize->findAsUTF8(definition->getItemBaseName());
                    ++purchaseTotal[weapon];
					++purchase.items[weapon];
                }
            }
            break;
        }
        case fnv::hash("round_start"):
            freezeEnd = 0.0f;
			playerPurchases.clear();
            purchaseTotal.clear();
            totalCost = 0;
            break;
        case fnv::hash("round_freeze_end"):
            freezeEnd = memory->globalVars->realtime;
            break;
        }
	} else
	{
		if (!config->misc.purchaseList.enabled)
			return;

		static const auto mp_buytime = interfaces->cvar->findVar("mp_buytime");

		if ((!interfaces->engine->isInGame() || freezeEnd != 0.0f && memory->globalVars->realtime > freezeEnd + (!config->misc.purchaseList.onlyDuringFreezeTime ? mp_buytime->getFloat() : 0.0f) || playerPurchases.empty() || purchaseTotal.empty()) && !gui->open)
			return;

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;
		if (!gui->open)
			windowFlags |= ImGuiWindowFlags_NoInputs;
		if (config->misc.purchaseList.noTitleBar)
			windowFlags |= ImGuiWindowFlags_NoTitleBar;

		ImGui::SetNextWindowSize({200.0f, 200.0f}, ImGuiCond_FirstUseEver);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, {0.5f, 0.5f});
		if (ImGui::Begin("Purchases", nullptr, windowFlags))
		{
			if (config->misc.purchaseList.mode == Config::Misc::PurchaseList::Details)
			{
				GameData::Lock lock;

				for (const auto &[handle, purchases] : playerPurchases)
				{
					std::string s;
					s.reserve(std::accumulate(purchases.items.begin(), purchases.items.end(), 0, [](int length, const auto &p) { return length + p.first.length() + 2; }));
					for (const auto &purchasedItem : purchases.items)
					{
						if (purchasedItem.second > 1)
							s += std::to_string(purchasedItem.second) + "x ";
						s += purchasedItem.first + ", ";
					}

					if (s.length() >= 2)
						s.erase(s.length() - 2);

					if (const auto it = std::find_if(GameData::players().cbegin(), GameData::players().cend(), [handle = handle](const auto &playerData) { return playerData.handle == handle; }); it != GameData::players().cend())
					{
						if (config->misc.purchaseList.showPrices)
							ImGui::TextWrapped("%s $%d: %s", it->name, purchases.totalCost, s.c_str());
						else
							ImGui::TextWrapped("%s: %s", it->name, s.c_str());
					}
				}
			} else if (config->misc.purchaseList.mode == Config::Misc::PurchaseList::Summary)
			{
				for (const auto &purchase : purchaseTotal)
					ImGui::TextWrapped("%d x %s", purchase.second, purchase.first.c_str());

				if (config->misc.purchaseList.showPrices && totalCost > 0)
				{
					ImGui::Separator();
					ImGui::TextWrapped("Total: $%d", totalCost);
				}
			}
			ImGui::End();
		}
		ImGui::PopStyleVar();
	}
}

static std::vector<std::uint64_t> reportedPlayers;
static int reportbotRound;

void Misc::runReportbot() noexcept
{
    if (!config->griefing.reportbot.enabled)
        return;

    if (!localPlayer)
        return;

    static auto lastReportTime = 0.0f;

    if (lastReportTime + config->griefing.reportbot.delay > memory->globalVars->realtime)
        return;

    if (reportbotRound >= config->griefing.reportbot.rounds)
        return;

    for (int i = 1; i <= interfaces->engine->getMaxClients(); ++i) {
        const auto entity = interfaces->entityList->getEntity(i);

        if (!entity || entity == localPlayer.get())
            continue;

        if (config->griefing.reportbot.target != 2 && (entity->isOtherEnemy(localPlayer.get()) ? config->griefing.reportbot.target != 0 : config->griefing.reportbot.target != 1))
            continue;

        PlayerInfo playerInfo;
        if (!interfaces->engine->getPlayerInfo(i, playerInfo))
            continue;

        if (playerInfo.fakeplayer || std::find(reportedPlayers.cbegin(), reportedPlayers.cend(), playerInfo.xuid) != reportedPlayers.cend())
            continue;

        std::string report;

        if (config->griefing.reportbot.textAbuse)
            report += "textabuse,";
        if (config->griefing.reportbot.griefing)
            report += "grief,";
        if (config->griefing.reportbot.wallhack)
            report += "wallhack,";
        if (config->griefing.reportbot.aimbot)
            report += "aimbot,";
        if (config->griefing.reportbot.other)
            report += "speedhack,";

        if (!report.empty()) {
            memory->submitReport(std::to_string(playerInfo.xuid).c_str(), report.c_str());
            lastReportTime = memory->globalVars->realtime;
            reportedPlayers.emplace_back(playerInfo.xuid);
        }
        return;
    }

    reportedPlayers.clear();
    ++reportbotRound;
}

void Misc::resetReportbot() noexcept
{
    reportbotRound = 0;
    reportedPlayers.clear();
}

void Misc::preserveKillfeed(bool roundStart) noexcept
{
    if (!config->misc.preserveKillfeed.enabled)
        return;

    static auto nextUpdate = 0.0f;

    if (roundStart) {
        nextUpdate = memory->globalVars->realtime + 10.0f;
        return;
    }

    if (nextUpdate > memory->globalVars->realtime)
        return;

    nextUpdate = memory->globalVars->realtime + 2.0f;

    const auto deathNotice = memory->findHudElement(memory->hud, "CCSGO_HudDeathNotice");
    if (!deathNotice)
        return;

    const auto deathNoticePanel = (*(UIPanel**)(*(deathNotice - 5 + 22) + 4));
    const auto childPanelCount = deathNoticePanel->getChildCount();

    for (int i = 0; i < childPanelCount; ++i) {
        const auto child = deathNoticePanel->getChild(i);
        if (!child)
            continue;

        if (child->hasClass("DeathNotice_Killer") && (!config->misc.preserveKillfeed.onlyHeadshots || child->hasClass("DeathNoticeHeadShot")))
            child->setAttributeFloat("SpawnTime", memory->globalVars->currenttime);
    }
}

void Misc::fixAnimation() noexcept
{
	GameData::Lock lock;
	auto &global = GameData::global();

	Animations::animSync(&global.lastCmd, global.sentPacket, &global.indicators.serverHead);
}

void Misc::drawOffscreenEnemies(ImDrawList *drawList) noexcept
{
	if (!config->misc.offscreenEnemies.enabled)
		return;

	GameData::Lock lock;

	const auto yaw = Helpers::degreesToRadians(interfaces->engine->getViewAngles().y);
	
	for (auto &player : GameData::players())
    {
		if (player.dormant || !player.alive || !player.enemy || player.inViewFrustum)
			continue;
		
		const auto positionDiff = GameData::local().origin - player.origin;

		auto x = std::cos(yaw) * positionDiff.y - std::sin(yaw) * positionDiff.x;
		auto y = std::cos(yaw) * positionDiff.x + std::sin(yaw) * positionDiff.y;
		auto cx = std::sin(yaw) * positionDiff.y + std::cos(yaw) * positionDiff.x;
		auto cy = std::sin(yaw) * positionDiff.x - std::cos(yaw) * positionDiff.y;
		const auto len = std::sqrt(x * x + y * y);
		const auto clen = std::sqrt(cx * cx + cy * cy);
		x /= len;
		y /= len;
		cx /= clen;
		cy /= clen;

		const auto center = ImGui::GetIO().DisplaySize / 2;
		const auto pos = center + ImVec2{ x, y } * 170;
		const auto pfar = center + ImVec2{ x, y } * 200;
		const auto color = Helpers::calculateColor(config->misc.offscreenEnemies);
        //drawList->AddCircleFilled(pos, 11.0f, color & IM_COL32_A_MASK, 40);
        //drawList->AddCircleFilled(pos, 10.0f, color, 40);
		drawList->AddTriangleFilled(pfar, pos + ImVec2{cx, cy} * 15, pos + ImVec2{cx, cy} * -15, color);
		drawList->AddTriangle(pfar, pos + ImVec2{cx, cy} * 15, pos + ImVec2{cx, cy} * -15, color | IM_COL32_A_MASK);
    }
}

void Misc::blockBot(UserCmd *cmd) noexcept
{
	if (!config->griefing.bb.keyMode) return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	GameData::Lock lock;
	auto &global = GameData::global();

	float best = 60.0f;
	if (static Helpers::KeyBindState flag; flag[config->griefing.bbTar])
	{
		for (auto &player : GameData::players())
		{
			Entity *entity = interfaces->entityList->getEntityFromHandle(player.handle);

			if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive() || localPlayer->isOtherEnemy(entity))
				continue;

			const auto angle = Helpers::calculateRelativeAngle(localPlayer->getEyePosition(), entity->getEyePosition(), cmd->viewangles);
			const auto fov = std::hypot(angle.x, angle.y);

			if (fov < best)
			{
				best = fov;
				global.indicators.blockTarget = entity->handle();
			}
		}
	}

	if (static Helpers::KeyBindState flag; !flag[config->griefing.bb]) return;

	const auto target = interfaces->entityList->getEntityFromHandle(global.indicators.blockTarget);
	if (target && target != localPlayer.get() && !target->isDormant() && target->isAlive() && !localPlayer->isOtherEnemy(target))
	{
		const auto targetVec = (target->getAbsOrigin() + target->velocity() * memory->globalVars->intervalPerTick * config->griefing.bbTrajFac - localPlayer->getAbsOrigin()) * config->griefing.bbDistFac;
		const auto z1 = target->getAbsOrigin().z - localPlayer->getEyePosition().z;
		const auto z2 = target->getEyePosition().z - localPlayer->getAbsOrigin().z;
		if (z1 >= 0.0f || z2 <= 0.0f)
		{
			Vector fwd = Vector::fromAngle2D(cmd->viewangles.y);
			Vector side = fwd.crossProduct(Vector::up());
			Vector move = Vector{fwd.dotProduct2D(targetVec), side.dotProduct2D(targetVec), 0.0f};
			move *= 45.0f;
			const float l = move.length2D();
			if (l > 450.0f)
				move *= 450.0f / l;
			cmd->forwardmove = move.x;
			cmd->sidemove = move.y;
		}
		else
		{
			Vector fwd = Vector::fromAngle2D(cmd->viewangles.y);
			Vector side = fwd.crossProduct(Vector::up());
			Vector tar = (targetVec / targetVec.length2D()).crossProduct(Vector::up());
			tar = tar.snapTo4();
			tar *= tar.dotProduct2D(targetVec);
			Vector move = Vector{fwd.dotProduct2D(tar), side.dotProduct2D(tar), 0.0f};
			move *= 45.0f;
			const float l = move.length2D();
			if (l > 450.0f)
				move *= 450.0f / l;
			cmd->forwardmove = move.x;
			cmd->sidemove = move.y;
		}
	}
}

void Misc::useSpam(UserCmd *cmd)
{
	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (!config->misc.spamUse)
		return;

	static auto plantAnywhere = interfaces->cvar->findVar("mp_plant_c4_anywhere");

	if (plantAnywhere->getInt())
		return;

	if (localPlayer->inBombZone() && localPlayer->flags() & Entity::FL_ONGROUND)
		return;

	if (cmd->buttons & UserCmd::IN_USE)
	{
		static bool flag = false;
		if (flag)
			cmd->buttons |= UserCmd::IN_USE;
		else
			cmd->buttons &= ~UserCmd::IN_USE;
		flag = !flag;
	}
}

void Misc::indicators(ImDrawList *drawList) noexcept
{
	GameData::Lock lock;
	auto &local = GameData::local();
	const auto &global = GameData::global();

	if (!local.exists || !local.alive)
		return;

	if (config->griefing.bb.keyMode)
	{
		auto target = GameData::playerByHandle(global.indicators.blockTarget);
		if (target && !target->dormant && target->alive && !target->enemy)
		{
			Vector curDir = target->velocity * 0.12f;
			curDir.z = 0.0f;
			Vector max = target->colMaxs + target->origin;
			Vector min = target->colMins + target->origin;
			const auto z = target->origin.z;

			ImVec2 pos, dir;
			ImVec2 points[4];

			const auto color = Helpers::calculateColor(config->griefing.bbCol);

			bool draw = Helpers::worldToScreen(target->origin, pos);
			draw = draw && Helpers::worldToScreen(curDir + target->origin, dir);

			if (draw)
			{
				drawList->AddLine(pos, dir, color);
			}

			draw = Helpers::worldToScreen(Vector{max.x, max.y, z}, points[0]);
			draw = draw && Helpers::worldToScreen(Vector{max.x, min.y, z}, points[1]);
			draw = draw && Helpers::worldToScreen(Vector{min.x, min.y, z}, points[2]);
			draw = draw && Helpers::worldToScreen(Vector{min.x, max.y, z}, points[3]);

			if (draw)
			{
				drawList->AddLine(points[0], points[1], color, 1.0f);
				drawList->AddLine(points[1], points[2], color, 1.0f);
				drawList->AddLine(points[2], points[3], color, 1.0f);
				drawList->AddLine(points[3], points[0], color, 1.0f);
			}
		}
	}

	if (config->visuals.playerVel.enabled)
	{
		Vector curDir = local.velocity * 0.12f;
		curDir.z = 0.0f;

		ImVec2 pos, dir;

		bool draw = Helpers::worldToScreen(local.origin, pos);
		draw = draw && Helpers::worldToScreen(curDir + local.origin, dir);

		if (draw)
		{
			const auto color = Helpers::calculateColor(config->visuals.playerVel);
			drawList->AddLine(pos, dir, color, config->visuals.playerVel.thickness);
		}
	}

	if (config->visuals.playerBounds.enabled)
	{
		Vector max = local.colMaxs + local.origin;
		Vector min = local.colMins + local.origin;
		const auto z = local.origin.z;

		ImVec2 points[4];

		bool draw = Helpers::worldToScreen(Vector{max.x, max.y, z}, points[0]);
		draw = draw && Helpers::worldToScreen(Vector{max.x, min.y, z}, points[1]);
		draw = draw && Helpers::worldToScreen(Vector{min.x, min.y, z}, points[2]);
		draw = draw && Helpers::worldToScreen(Vector{min.x, max.y, z}, points[3]);

		if (draw)
		{
			const auto color = Helpers::calculateColor(config->visuals.playerBounds);
			drawList->AddLine(points[0], points[1], color, config->visuals.playerBounds.thickness);
			drawList->AddLine(points[1], points[2], color, config->visuals.playerBounds.thickness);
			drawList->AddLine(points[2], points[3], color, config->visuals.playerBounds.thickness);
			drawList->AddLine(points[3], points[0], color, config->visuals.playerBounds.thickness);
		}
	}

	#ifdef _DEBUG_NEPS
	for (auto &p : global.indicators.multipoints)
	{
		ImVec2 pos;
		Helpers::worldToScreen(p, pos);

		drawList->AddCircle(pos, 2.0f, 0xFFFFFFFF);
	}
	#endif // _DEBUG_NEPS

	if (!config->misc.indicators) return;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, {0.5f, 0.5f});
	ImGui::SetNextWindowPos(ImVec2{0.0f, 50.0f}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2{200.0f, 0.0f}, ImVec2{200.0f, FLT_MAX});
	if (ImGui::Begin("Indicators", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
	{
		const auto netChannel = interfaces->engine->getNetworkChannel();
		if (netChannel)
		{
			ImGui::TextUnformatted("Choke");
			ImGuiCustom::progressBarFullWidth(static_cast<float>(netChannel->chokedPackets) / static_cast<float>(config->antiAim.chokedPackets));
		}

		ImGui::TextUnformatted(("Speed " + std::to_string(std::lroundf(local.velocity.length2D())) + "u").c_str());

		if (memory->input->isCameraInThirdPerson)
		{
			ImGui::TextUnformatted("In thirdperson");
			if (config->antiAim.desync)
			{
				const auto deltaU = global.indicators.serverHead.distTo(global.indicators.desyncHead);
				const auto deltaA = std::fabsf(global.indicators.deltaLby);

				#ifdef _DEBUG_NEPS
				ImGui::TextUnformatted(("Delta LBY " + std::to_string(global.indicators.deltaLby) + "deg").c_str());
				ImGui::TextUnformatted(("Desync " + std::to_string(deltaU) + "u").c_str());
				ImGui::TextUnformatted(("Desync " + std::to_string(deltaA) + "deg").c_str());
				#endif // _DEBUG_NEPS

				if (deltaU < 3.5f)
					ImGui::TextColored(ImVec4{1.0f, 0.0f, 0.0f, 1.0f}, "Desync overlap!");
				else if (deltaU < 6.0f)
					ImGui::TextUnformatted("Desync unsafe");
				else if (deltaA > 90.0f)
					ImGui::TextUnformatted("Desync extended");
				else
					ImGui::TextUnformatted("Desync ok");
			}
		}

		ImGui::End();
	}
	ImGui::PopStyleVar();
}

void Misc::voteRevealer(GameEvent &event) noexcept
{
	if (!config->griefing.revealVotes)
		return;

	const auto entity = interfaces->entityList->getEntity(event.getInt("entityid"));
	if (!entity || !entity->isPlayer())
		return;

	memory->conColorMsg({120, 0, 255, 255}, "[NEPS]: ");
	memory->debugMsg("%s -> ", entity->getPlayerName().c_str());

	if (event.getInt("vote_option"))
		memory->conColorMsg({255, 0, 0, 255}, "NO\n");
	else
		memory->conColorMsg({0, 255, 0, 255}, "YES\n");
}
