#include "Aimbot.h"
#include "../Config.h"
#include "../Interfaces.h"
#ifdef _DEBUG_NEPS
#include "../GameData.h"
#endif // _DEBUG_NEPS
#include "../Memory.h"
#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Vector.h"
#include "../SDK/WeaponId.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/ModelInfo.h"
#include "../SDK/PhysicsSurfaceProps.h"
#include "../SDK/WeaponData.h"
#include "../Helpers.h"

void Aimbot::run(UserCmd *cmd) noexcept
{
	if (!localPlayer) return;

	if (localPlayer->nextAttack() > memory->globalVars->serverTime() || localPlayer->isDefusing() || localPlayer->waitForNoAttack())
		return;

	const auto activeWeapon = localPlayer->getActiveWeapon();
	if (!activeWeapon || !activeWeapon->clip())
		return;

	if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
		return;

	auto weaponIndex = getWeaponIndex(activeWeapon->itemDefinitionIndex2());
	if (!weaponIndex)
		return;

	auto weaponClass = getWeaponClass(activeWeapon->itemDefinitionIndex2());
	if (!config->aimbot[weaponIndex].bind.keyMode)
		weaponIndex = weaponClass;

	if (!config->aimbot[weaponIndex].bind.keyMode)
		weaponIndex = 0;

	if (static Helpers::KeyBindState flag; !flag[config->aimbot[weaponIndex].bind]) return;

	if (!config->aimbot[weaponIndex].hitgroup)
		return;

    if (!config->aimbot[weaponIndex].betweenShots && activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
        return;

    if (!config->aimbot[weaponIndex].ignoreFlash && localPlayer->isFlashed())
        return;

    if ((cmd->buttons & UserCmd::IN_ATTACK || config->aimbot[weaponIndex].autoShot || config->aimbot[weaponIndex].aimlock) && activeWeapon->getInaccuracy() <= config->aimbot[weaponIndex].maxAimInaccuracy) {

        if (config->aimbot[weaponIndex].scopedOnly && activeWeapon->isSniperRifle() && !localPlayer->isScoped() && !config->aimbot[weaponIndex].autoScope)
            return;

		const auto aimPunch = activeWeapon->requiresRecoilControl() ? localPlayer->getAimPunch() : Vector{ };

        auto bestFov = config->aimbot[weaponIndex].fov;
        auto bestDistance = config->aimbot[weaponIndex].distance ? config->aimbot[weaponIndex].distance : INFINITY;
        auto bestDamage = min(config->aimbot[weaponIndex].minDamage, config->aimbot[weaponIndex].minDamageAutoWall);
        auto bestHitchance = config->aimbot[weaponIndex].shotHitchance;
        Vector bestTarget = Vector{};
        Entity *targetEntity = nullptr;

        const auto localPlayerEyePosition = localPlayer->getEyePosition();

		std::array<matrix3x4, MAXSTUDIOBONES> bufferBones;
		#ifdef _DEBUG_NEPS
		std::vector<Vector> multipoints;
		#endif // _DEBUG_NEPS

		for (int i = 1; i <= interfaces->engine->getMaxClients(); i++)
		{
			auto entity = interfaces->entityList->getEntity(i);

			if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive() || entity->gunGameImmunity() || !config->aimbot[weaponIndex].friendlyFire && !entity->isOtherEnemy(localPlayer.get()))
				continue;

			const auto hitboxSet = entity->getHitboxSet();

			if (!hitboxSet)
				continue;

			auto allowedHitgroup = config->aimbot[weaponIndex].hitgroup;

			if (static Helpers::KeyBindState flag; flag[config->aimbot[weaponIndex].safeOnly] && !entity->isBot())
			{
				const auto remoteActiveWep = entity->getActiveWeapon();
				if (remoteActiveWep && config->aimbot[weaponIndex].onShot && remoteActiveWep->lastShotTime() == entity->simulationTime());
				else if (config->aimbot[weaponIndex].onMove && (entity->velocity().length2D() > 90.0f || ~entity->flags() & Entity::FL_ONGROUND));
				else
				{
					allowedHitgroup = config->aimbot[weaponIndex].safeHitgroup;
				}
			}

			if (!allowedHitgroup)
				continue;

			if (!entity->setupBones(bufferBones.data(), MAXSTUDIOBONES, BONE_USED_BY_HITBOX, 0.0f))
				continue;

			if (config->aimbot[weaponIndex].autoScope && activeWeapon->isSniperRifle() && !localPlayer->isScoped())
			{
				bool goesThroughWall = false;
				Trace trace;
				auto origin = bufferBones[0].origin();
				bool canHit = Helpers::canHit(origin, trace, config->aimbot[weaponIndex].friendlyFire, &goesThroughWall);
				if (trace.entity == entity && canHit && (!config->aimbot[weaponIndex].visibleOnly || !goesThroughWall))
					cmd->buttons |= UserCmd::IN_ATTACK2;
			}

			for (int hitboxIdx = 0; hitboxIdx < hitboxSet->numHitboxes; hitboxIdx++)
			{
				if (hitboxIdx == Hitbox::LeftFoot ||
					hitboxIdx == Hitbox::RightFoot ||
					hitboxIdx == Hitbox::LeftHand ||
					hitboxIdx == Hitbox::RightHand ||
					hitboxIdx == Hitbox::Neck ||
					hitboxIdx == Hitbox::LowerChest ||
					hitboxIdx == Hitbox::Belly)
					continue;

				const auto hitbox = *hitboxSet->getHitbox(hitboxIdx);

				std::vector<Vector> points;

				if (config->aimbot[weaponIndex].multipoint)
				{
					switch (hitboxIdx)
					{
					case Hitbox::Head:
					{
						const float r = hitbox.capsuleRadius * config->aimbot[weaponIndex].multipointScale;
						const Vector min = hitbox.bbMin.transform(bufferBones[hitbox.bone]);
						const Vector max = hitbox.bbMax.transform(bufferBones[hitbox.bone]);
						Vector mid = (min + max) * 0.5f;
						Vector axis = max - min;
						axis /= axis.length();

						Vector v1 = min.crossProduct(max);
						v1 /= v1.length();
						v1 *= r;
						Vector v2 = v1.rotate(axis, 120.0f);
						Vector v3 = v2.rotate(axis, 120.0f);

						points.emplace_back(mid);
						points.emplace_back(max + v1);
						points.emplace_back(max + v2);
						points.emplace_back(max + v3);
						points.emplace_back(max + axis * r);
						break;
					}
					case Hitbox::UpperChest:
					{
						const float r = hitbox.capsuleRadius * config->aimbot[weaponIndex].multipointScale;
						const Vector min = hitbox.bbMin.transform(bufferBones[hitbox.bone]);
						const Vector max = hitbox.bbMax.transform(bufferBones[hitbox.bone]);
						Vector axis = max - min;
						axis /= axis.length();
						Vector axisRel = hitbox.bbMax - hitbox.bbMin;
						axisRel /= axisRel.length();
						Vector midRel = (hitbox.bbMin + hitbox.bbMax) * 0.5f;

						Vector v1 = hitbox.bbMin.crossProduct(hitbox.bbMax);
						v1 /= v1.length();
						v1 *= r;

						axis *= r;

						points.emplace_back((midRel + v1).transform(bufferBones[hitbox.bone]));
						points.emplace_back(max + axis);
						points.emplace_back(min - axis);
						break;
					}
					case Hitbox::Thorax:
					{
						const float r = hitbox.capsuleRadius * config->aimbot[weaponIndex].multipointScale;
						const Vector min = hitbox.bbMin.transform(bufferBones[hitbox.bone]);
						const Vector max = hitbox.bbMax.transform(bufferBones[hitbox.bone]);
						Vector mid = (min + max) * 0.5f;
						Vector axis = max - min;
						axis /= axis.length();
						axis *= r;

						points.emplace_back(mid);
						points.emplace_back(max + axis);
						points.emplace_back(min - axis);
						break;
					}
					case Hitbox::Pelvis:
					{
						const float r = hitbox.capsuleRadius * config->aimbot[weaponIndex].multipointScale;
						const Vector min = hitbox.bbMin.transform(bufferBones[hitbox.bone]);
						const Vector max = hitbox.bbMax.transform(bufferBones[hitbox.bone]);
						Vector axis = max - min;
						axis /= axis.length();
						Vector axisRel = hitbox.bbMax - hitbox.bbMin;
						axisRel /= axisRel.length();
						Vector midRel = (hitbox.bbMin + hitbox.bbMax) * 0.5f;

						Vector v1 = hitbox.bbMin.crossProduct(hitbox.bbMax);
						v1 /= v1.length();
						v1 *= r;

						axis *= r;

						points.emplace_back((midRel - v1).transform(bufferBones[hitbox.bone]));
						points.emplace_back(max + axis);
						points.emplace_back(min - axis);
						break;
					}
					default:
						points.emplace_back(hitbox.bbMax.transform(bufferBones[hitbox.bone]));
						break;
					}
				} else
				{
				switch (hitboxIdx)
				{
				case Hitbox::Head:
				case Hitbox::UpperChest:
				case Hitbox::Thorax:
				case Hitbox::Pelvis:
					points.emplace_back(((hitbox.bbMin + hitbox.bbMax) * 0.5f).transform(bufferBones[hitbox.bone]));
					break;
				default:
					points.emplace_back(hitbox.bbMax.transform(bufferBones[hitbox.bone]));
					break;
				}
				}

				#ifdef _DEBUG_NEPS
				multipoints.insert(multipoints.end(), points.begin(), points.end());
				#endif // _DEBUG_NEPS

				const float radius = Helpers::approxRadius(hitbox, hitboxIdx);

				for (auto &point : points)
				{
					const auto angle = Helpers::calculateRelativeAngle(localPlayerEyePosition, point, cmd->viewangles + aimPunch);

					const auto fov = std::hypot(angle.x, angle.y);
					if (fov >= bestFov)
						continue;

					const auto distance = localPlayerEyePosition.distTo(point);
					if (distance >= bestDistance || distance > activeWeapon->getWeaponData()->range)
						continue;

					const auto hitchance = Helpers::findHitchance(activeWeapon->getInaccuracy(), activeWeapon->getSpread(), radius, distance);
					if (hitchance <= bestHitchance)
						continue;

					bool goesThroughWall = false;
					Trace trace;
					const auto damage = Helpers::findDamage(point, activeWeapon->getWeaponData(), trace, config->aimbot[weaponIndex].friendlyFire, allowedHitgroup, &goesThroughWall);

					if (config->aimbot[weaponIndex].visibleOnly && goesThroughWall) continue;

					if (trace.entity != entity) continue;

					if (!goesThroughWall)
					{
						if (damage <= min(config->aimbot[weaponIndex].minDamage, entity->health() + config->aimbot[weaponIndex].killshot))
							continue;
						if (damage <= min(bestDamage, entity->health() + config->aimbot[weaponIndex].killshot))
							continue;
					} else
					{
						if (damage <= min(config->aimbot[weaponIndex].minDamageAutoWall, entity->health() + config->aimbot[weaponIndex].killshotAutoWall))
							continue;
						if (damage <= min(bestDamage, entity->health() + config->aimbot[weaponIndex].killshotAutoWall))
							continue;
					}

					if (!config->aimbot[weaponIndex].ignoreSmoke && memory->lineGoesThroughSmoke(localPlayerEyePosition, point, 1))
						continue;

					switch (config->aimbot[weaponIndex].targeting)
					{
					case 0:
						if (fov < bestFov)
						{
							bestFov = fov;
							bestTarget = point;
							targetEntity = entity;
						}
						break;
					case 1:
						if (damage > bestDamage)
						{
							bestDamage = damage;
							bestTarget = point;
							targetEntity = entity;
						}
						break;
					case 2:
						if (hitchance > bestHitchance)
						{
							bestHitchance = hitchance;
							bestTarget = point;
							targetEntity = entity;
						}
						break;
					case 3:
						if (distance < bestDistance)
						{
							bestDistance = distance;
							bestTarget = point;
							targetEntity = entity;
						}
						break;
					}
				}
			}
		}

		#ifdef _DEBUG_NEPS
		{
			GameData::Lock lock;
			GameData::global().indicators.multipoints.clear();
			GameData::global().indicators.multipoints = multipoints;
		}
		#endif // _DEBUG_NEPS

		if (bestTarget.notNull() && targetEntity)
		{
			static Vector lastAngles = cmd->viewangles;
			static int lastCommand = 0;

			if (lastCommand == cmd->commandNumber - 1 && lastAngles.notNull() && config->aimbot[weaponIndex].silent)
				cmd->viewangles = lastAngles;

			auto angle = Helpers::calculateRelativeAngle(localPlayerEyePosition, bestTarget, cmd->viewangles + aimPunch);
			bool clamped = false;

			if (std::abs(angle.x) > config->misc.maxAngleDelta || std::abs(angle.y) > config->misc.maxAngleDelta)
			{
				angle.x = std::clamp(angle.x, -config->misc.maxAngleDelta, config->misc.maxAngleDelta);
				angle.y = std::clamp(angle.y, -config->misc.maxAngleDelta, config->misc.maxAngleDelta);
				clamped = true;
			}

			if (config->aimbot[weaponIndex].interpolation == 2 || config->aimbot[weaponIndex].interpolation == 3)
				angle = angle * (1.0f - config->aimbot[weaponIndex].smooth);

			const auto l = angle.length();
			if ((config->aimbot[weaponIndex].interpolation == 1 || config->aimbot[weaponIndex].interpolation == 3) && l > config->aimbot[weaponIndex].linearSpeed)
				angle *= config->aimbot[weaponIndex].linearSpeed / l;

			if (config->aimbot[weaponIndex].targetStop && !config->aimbot[weaponIndex].silent)
			{
				Vector looking = localPlayerEyePosition + Vector::fromAngle(cmd->viewangles) * activeWeapon->getWeaponData()->range;
				bool goesThroughWall = false;
				Trace trace;
				bool reached = Helpers::canHit(looking, trace, config->aimbot[weaponIndex].friendlyFire, &goesThroughWall);
				if (trace.entity == targetEntity && reached && (!config->aimbot[weaponIndex].visibleOnly || !goesThroughWall))
					angle = Vector{};
			}

			if (angle.notNull())
			{
				cmd->viewangles += angle;

				if (!config->aimbot[weaponIndex].silent)
					interfaces->engine->setViewAngles(cmd->viewangles);
			}

			if (config->aimbot[weaponIndex].autoShot && activeWeapon->nextPrimaryAttack() <= memory->globalVars->serverTime() && !clamped)
				cmd->buttons |= UserCmd::IN_ATTACK;

			if (clamped)
			{
				cmd->buttons &= ~UserCmd::IN_ATTACK;
				lastAngles = cmd->viewangles;
			} else lastAngles = Vector{};

			lastCommand = cmd->commandNumber;
		}
	}
}
