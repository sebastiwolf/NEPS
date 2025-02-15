#include <functional>

#include "Chams.h"
#include "Animations.h"
#include "../Config.h"
#include "../GameData.h"
#include "../Hooks.h"
#include "../Interfaces.h"
#include "Backtrack.h"
#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/EntityList.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/Material.h"
#include "../SDK/MaterialSystem.h"
#include "../SDK/Vector.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/StudioRender.h"
#include "../SDK/KeyValues.h"

Chams::Chams() noexcept
{
    normal = interfaces->materialSystem->createMaterial("normal", KeyValues::fromString("VertexLitGeneric", nullptr));
    flat = interfaces->materialSystem->createMaterial("flat", KeyValues::fromString("UnlitGeneric", nullptr));
	flatadditive = interfaces->materialSystem->createMaterial("flatadditive", KeyValues::fromString("UnlitGeneric", "$additive 1"));
    chrome = interfaces->materialSystem->createMaterial("chrome", KeyValues::fromString("VertexLitGeneric", "$envmap env_cubemap"));
    glow = interfaces->materialSystem->createMaterial("glow", KeyValues::fromString("VertexLitGeneric", "$additive 1 $envmap models/effects/cube_white $envmapfresnel 1"));
    pearlescent = interfaces->materialSystem->createMaterial("pearlescent", KeyValues::fromString("VertexLitGeneric", "$ambientonly 1 $phong 1 $basemapalphaphongmask 1 $phongboost 0"));

    {
		const auto kv = KeyValues::fromString("VertexLitGeneric", "$additive 1 $basetexture dev/zone_warning proxies { texturescroll { texturescrollvar $basetexturetransform texturescrollrate 0.6 texturescrollangle 90 } }");
        animated = interfaces->materialSystem->createMaterial("animated", kv);
    }

    {
        const auto kv = KeyValues::fromString("VertexLitGeneric", "$baseTexture detail/dt_metal1 $additive 1 $envmap editor/cube_vertigo");
        kv->setString("$color", "[.05 .05 .05]");
        glass = interfaces->materialSystem->createMaterial("glass", kv);
    }

    {
        const auto kv = KeyValues::fromString("VertexLitGeneric", "$baseTexture black $bumpmap effects/flat_normal $translucent 1 $envmap models/effects/crystal_cube_vertigo_hdr $envmapfresnel 0");
        crystal = interfaces->materialSystem->createMaterial("crystal", kv);
    }

	{
		const auto kv = KeyValues::fromString("VertexLitGeneric", "$baseTexture black $bumpmap models/inventory_items/trophy_majors/matte_metal_normal $additive 1 $normalmapalphaenvmapmask 1 $phong 1 $phongboost 20 $phongdisablehalflambert 1");
		phong = interfaces->materialSystem->createMaterial("phong", kv);
	}

	{
		const auto kv = KeyValues::fromString("VertexLitGeneric", "$baseTexture black $additive 1 $envmap env_cubemap $envmapfresnel 1");
		fresnel = interfaces->materialSystem->createMaterial("fresnel", kv);
	}
}

bool Chams::render(void* ctx, void* state, const ModelRenderInfo& info, matrix3x4* customBoneToWorld) noexcept
{
    appliedChams = false;
    this->ctx = ctx;
    this->state = state;
    this->info = &info;
	this->customBoneToWorld = customBoneToWorld;

    if (std::string_view{ info.model->name }.starts_with("models/weapons/v_")) {
        // info.model->name + 17 -> small optimization, skip "models/weapons/v_"
        if (std::strstr(info.model->name + 17, "sleeve"))
            renderSleeves();
        else if (std::strstr(info.model->name + 17, "arms"))
            renderHands();
        else if (!std::strstr(info.model->name + 17, "tablet")
			&& !std::strstr(info.model->name + 17, "parachute")
			&& !std::strstr(info.model->name + 17, "fists"))
            renderWeapons();
	} else if (std::string_view{info.model->name}.starts_with("models/weapons/w_")) {
		if (std::strstr(info.model->name + 17, "ied_dropped"))
			renderC4();
		else if (std::strstr(info.model->name + 17, "defuser"))
			renderDefuser();
		else if (!std::strstr(info.model->name + 17, "glove"))
			renderWorldWeapons();
	} else if (std::string_view{info.model->name}.starts_with("models/player/custom_player/legacy/") && !interfaces->entityList->getEntity(info.entityIndex)->isPlayer()) {
		renderRagdolls();
	} else if (std::string_view{info.model->name}.starts_with("models/gibs") || std::string_view{info.model->name}.starts_with("models/props")) {
		renderProps();
	} else {
        const auto entity = interfaces->entityList->getEntity(info.entityIndex);
        if (entity && !entity->isDormant() && entity->isPlayer())
            renderPlayer(entity);
    }

    return appliedChams;
}

void Chams::renderPlayer(Entity* player) noexcept
{
    if (!localPlayer)
        return;

    const auto health = player->health();

	if (const auto activeWeapon = player->getActiveWeapon(); activeWeapon && activeWeapon->getClientClass()->classId == ClassId::C4 && activeWeapon->c4StartedArming() && std::any_of(config->chams["Planting"].materials.cbegin(), config->chams["Planting"].materials.cend(), [](const Config::Chams::Material& mat) { return mat.enabled; }))
	{
        applyChams(config->chams["Planting"].materials, health);
    } else if (player->isDefusing() && std::any_of(config->chams["Defusing"].materials.cbegin(), config->chams["Defusing"].materials.cend(), [](const Config::Chams::Material& mat) { return mat.enabled; }))
	{
        applyChams(config->chams["Defusing"].materials, health);
	} else if (player == localPlayer.get())
	{
		applyChams(config->chams["Local player"].materials, health);

		GameData::Lock lock;
		const auto &global = GameData::global();

		if (config->antiAim.desync) {
			matrix3x4 fakeBones[MAXSTUDIOBONES];
			std::copy(std::begin(global.lerpedBones), std::end(global.lerpedBones), fakeBones);

			const auto &origin = localPlayer->getRenderOrigin();

			for (int i = 0; i < MAXSTUDIOBONES; i++)
				fakeBones[i].setOrigin(fakeBones[i].origin() + origin);

			if (!appliedChams) hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customBoneToWorld);
			applyChams(config->chams["Desync"].materials, health, fakeBones);
			interfaces->studioRender->forcedMaterialOverride(nullptr);
		}
    } else if (localPlayer->isOtherEnemy(player))
	{
        applyChams(config->chams["Enemies"].materials, health);

        if (config->backtrack.enabled) {
            const auto& record = Backtrack::getRecords(player->index());
			if (record.size())
				if (config->chams["Backtrack"].trailBacktrack)
				{
					for (auto &snapshot : record)
						if (Backtrack::valid(snapshot.simulationTime) && snapshot.origin != player->origin())
						{
							if (!appliedChams) hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customBoneToWorld);
							applyChams(config->chams["Backtrack"].materials, health, snapshot.matrix);
							interfaces->studioRender->forcedMaterialOverride(nullptr);
						}
				}
				else
				{
					if (Backtrack::valid(record.front().simulationTime) && record.back().origin != player->origin())
					{
						if (!appliedChams) hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customBoneToWorld);
						applyChams(config->chams["Backtrack"].materials, health, record.back().matrix);
						interfaces->studioRender->forcedMaterialOverride(nullptr);
					}
				}
        }
    } else {
        applyChams(config->chams["Allies"].materials, health);
    }
}

void Chams::renderWeapons() noexcept
{
    if (!localPlayer || !localPlayer->isAlive() || localPlayer->isScoped())
        return;

    applyChams(config->chams["Weapons"].materials, localPlayer->health());
}

void Chams::renderProps() noexcept
{
	if (!localPlayer)
		return;

	applyChams(config->chams["Props"].materials, 0);
}

void Chams::renderRagdolls() noexcept
{
	if (!localPlayer)
		return;

	applyChams(config->chams["Ragdolls"].materials, 0);
}

void Chams::renderWorldWeapons() noexcept
{
	if (!localPlayer)
		return;

	applyChams(config->chams["World weapons"].materials, 0);
}

void Chams::renderDefuser() noexcept
{
	if (!localPlayer)
		return;

	applyChams(config->chams["Defusers"].materials, 0);
}

void Chams::renderC4() noexcept
{
	if (!localPlayer)
		return;

	applyChams(config->chams["C4"].materials, 0);
}

void Chams::renderHands() noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;

    applyChams(config->chams["Hands"].materials, localPlayer->health());
}

void Chams::renderSleeves() noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;

    applyChams(config->chams["Sleeves"].materials, localPlayer->health());
}

void Chams::applyChams(const std::array<Config::Chams::Material, 7>& chams, int health, const matrix3x4* customMatrix) noexcept
{
    for (const auto& cham : chams) {
        if (!cham.enabled || !cham.ignorez)
            continue;

        const auto material = dispatchMaterial(cham.material);
        if (!material)
            continue;
        
        float r, g, b;
        if (cham.healthBased && health) {
            r = 1.0f - health / 100.0f;
            g = health / 100.0f;
            b = 0.0f;
        } else if (cham.rainbow) {
            std::tie(r, g, b) = Helpers::rainbowColor(cham.rainbowSpeed);
        } else {
            r = cham.color[0];
            g = cham.color[1];
            b = cham.color[2];
        }

		if (material == glow || material == chrome || material == glass || material == crystal || material == fresnel)
			material->findVar("$envmaptint")->setVectorValue(r, g, b);
		else if (material == phong)
			material->findVar("$phongtint")->setVectorValue(r, g, b);
		else
			material->colorModulate(r, g, b);

		const auto pulse = cham.color[3] * (cham.blinking ? std::sin(memory->globalVars->currenttime * 5) * 0.5f + 0.5f : 1.0f);
		const auto invpulse = 1.0f - pulse;

		if (material == glow)
			material->findVar("$envmapfresnelminmaxexp")->setVecComponentValue(10.0f * invpulse * invpulse + 2.0f, 2);
		else if (material == fresnel)
			material->findVar("$envmapfresnelminmaxexp")->setVecComponentValue(8.0f * invpulse * invpulse * invpulse * invpulse, 2);
		else if (material == phong)
			material->findVar("$phongexponent")->setValue(3000.0f * pulse * pulse * pulse * pulse + 10.0f);
		else if (material == pearlescent)
			material->findVar("$pearlescent")->setValue(15.0f * (0.5f - pulse));
		else
			material->alphaModulate(pulse);

        material->setMaterialVarFlag(MaterialVarFlag::IGNOREZ, true);
        material->setMaterialVarFlag(MaterialVarFlag::WIREFRAME, cham.wireframe);
        interfaces->studioRender->forcedMaterialOverride(material);
        hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customMatrix ? customMatrix : customBoneToWorld);
        interfaces->studioRender->forcedMaterialOverride(nullptr);
    }

    for (const auto& cham : chams) {
        if (!cham.enabled || cham.ignorez)
            continue;

        const auto material = dispatchMaterial(cham.material);
        if (!material)
            continue;

        float r, g, b;
        if (cham.healthBased && health) {
            r = 1.0f - health / 100.0f;
            g = health / 100.0f;
            b = 0.0f;
        } else if (cham.rainbow) {
            std::tie(r, g, b) = Helpers::rainbowColor(cham.rainbowSpeed);
        } else {
            r = cham.color[0];
            g = cham.color[1];
            b = cham.color[2];
        }

		if (material == glow || material == chrome || material == glass || material == crystal || material == fresnel)
			material->findVar("$envmaptint")->setVectorValue(r, g, b);
		else if (material == phong)
			material->findVar("$phongtint")->setVectorValue(r, g, b);
		else
			material->colorModulate(r, g, b);

		const auto pulse = cham.color[3] * (cham.blinking ? std::sin(memory->globalVars->currenttime * 5) * 0.5f + 0.5f : 1.0f);
		const auto invpulse = 1.0f - pulse;

		if (material == glow)
			material->findVar("$envmapfresnelminmaxexp")->setVecComponentValue(10.0f * invpulse * invpulse + 2.0f, 2);
		else if (material == fresnel)
			material->findVar("$envmapfresnelminmaxexp")->setVecComponentValue(8.0f * invpulse * invpulse * invpulse * invpulse, 2);
		else if (material == phong)
			material->findVar("$phongexponent")->setValue(3000.0f * pulse * pulse * pulse * pulse + 10.0f);
		else if (material == pearlescent)
			material->findVar("$pearlescent")->setValue(15.0f * (0.5f - pulse));
		else
			material->alphaModulate(pulse);

        if (cham.cover && !appliedChams)
            hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customMatrix ? customMatrix : customBoneToWorld);

        material->setMaterialVarFlag(MaterialVarFlag::IGNOREZ, false);
        material->setMaterialVarFlag(MaterialVarFlag::WIREFRAME, cham.wireframe);
        interfaces->studioRender->forcedMaterialOverride(material);
        hooks->modelRender.callOriginal<void, 21>(ctx, state, info, customMatrix ? customMatrix : customBoneToWorld);
        appliedChams = true;
    }
}
