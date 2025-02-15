#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#include <ShlObj.h>
#endif

#include "nlohmann/json.hpp"

#include "Config.h"
#include "Helpers.h"
#include "Gui.h"

#ifdef _WIN32
int CALLBACK fontCallback(const LOGFONTW* lpelfe, const TEXTMETRICW*, DWORD, LPARAM lParam)
{
    const wchar_t* const fontName = reinterpret_cast<const ENUMLOGFONTEXW*>(lpelfe)->elfFullName;

    if (fontName[0] == L'@')
        return TRUE;

    if (HFONT font = CreateFontW(0, 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH, fontName)) {

        DWORD fontData = GDI_ERROR;

        if (HDC hdc = CreateCompatibleDC(nullptr)) {
            SelectObject(hdc, font);
            // Do not use TTC fonts as we only support TTF fonts
            fontData = GetFontData(hdc, 'fctt', 0, NULL, 0);
            DeleteDC(hdc);
        }
        DeleteObject(font);

        if (fontData == GDI_ERROR) {
            if (char buff[1024]; WideCharToMultiByte(CP_UTF8, 0, fontName, -1, buff, sizeof(buff), nullptr, nullptr))
                reinterpret_cast<std::vector<std::string>*>(lParam)->emplace_back(buff);
        }
    }
    return TRUE;
}
#endif

Config::Config(const char *name) noexcept
{
	#ifdef _WIN32
	if (PWSTR pathToDocuments; SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &pathToDocuments)))
	{
		path = pathToDocuments;
		path /= name;
		CoTaskMemFree(pathToDocuments);
	}
	#endif

	listConfigs();

	#ifdef _WIN32
	LOGFONTW logfont;
	logfont.lfCharSet = ANSI_CHARSET;
	logfont.lfPitchAndFamily = DEFAULT_PITCH;
	logfont.lfFaceName[0] = L'\0';

	EnumFontFamiliesExW(GetDC(nullptr), &logfont, fontCallback, (LPARAM)&systemFonts, 0);
	#endif

	std::sort(std::next(systemFonts.begin()), systemFonts.end());
}

using json = nlohmann::basic_json<std::map, std::vector, std::string, bool, std::int64_t, std::uint64_t, float>;
using value_t = json::value_t;

template <value_t Type, typename T>
static typename std::enable_if_t<!std::is_same_v<T, bool>> read(const json& j, const char* key, T& o) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.type() == Type)
        val.get_to(o);
}

static void read(const json& j, const char* key, bool& o) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.type() == value_t::boolean)
        val.get_to(o);
}

static void read(const json& j, const char* key, float& o) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.type() == value_t::number_float)
        val.get_to(o);
}

static void read(const json& j, const char* key, int& o) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.is_number_integer())
        val.get_to(o);
}

template <typename T, size_t Size>
static void read_array_opt(const json& j, const char* key, std::array<T, Size>& o) noexcept
{
    if (j.contains(key) && j[key].type() == value_t::array) {
        std::size_t i = 0;
        for (const auto& e : j[key]) {
            if (i >= o.size())
                break;

            if (e.is_null())
                continue;

            e.get_to(o[i]);
            ++i;
        }
    }
}

template <typename T, size_t Size>
static void read(const json& j, const char* key, std::array<T, Size>& o) noexcept
{
    if (!j.contains(key))
        return;

    if (const auto& val = j[key]; val.type() == value_t::array && val.size() == o.size()) {
        for (std::size_t i = 0; i < val.size(); ++i) {
            if (!val[i].empty())
                val[i].get_to(o[i]);
        }
    }
}

template <typename T>
static void read(const json& j, const char* key, std::unordered_map<std::string, T>& o) noexcept
{
    if (j.contains(key) && j[key].is_object()) {
        for (auto& element : j[key].items())
            element.value().get_to(o[element.key()]);
    }
}

static void from_json(const json &j, KeyBind &c)
{
	read(j, "Key", c.key);
	read(j, "Key mode", c.keyMode);
}

static void from_json(const json &j, Color4 &c)
{
	read(j, "Color", c.color);
	read(j, "Rainbow", c.rainbow);
	read(j, "Rainbow Speed", c.rainbowSpeed);
}

static void from_json(const json &j, Color4Border &cb)
{
    from_json(j, static_cast<Color4&>(cb));
    read(j, "Border", cb.border);
}

static void from_json(const json &j, Color4BorderToggle &cbt)
{
	from_json(j, static_cast<Color4Border &>(cbt));
	read(j, "Enabled", cbt.enabled);
}

static void from_json(const json &j, Color4BorderToggleThickness &cbt)
{
	from_json(j, static_cast<Color4BorderToggle &>(cbt));
	read(j, "Thickness", cbt.thickness);
}

static void from_json(const json &j, Color4Toggle &ct)
{
	from_json(j, static_cast<Color4 &>(ct));
	read(j, "Enabled", ct.enabled);
}

static void from_json(const json &j, Color3 &c)
{
	read(j, "Color", c.color);
	read(j, "Rainbow", c.rainbow);
	read(j, "Rainbow Speed", c.rainbowSpeed);
}

static void from_json(const json &j, Color3Toggle &ct)
{
	from_json(j, static_cast<Color3 &>(ct));
	read(j, "Enabled", ct.enabled);
}

static void from_json(const json &j, Color4ToggleRounding &ctr)
{
	from_json(j, static_cast<Color4Toggle &>(ctr));
	read(j, "Rounding", ctr.rounding);
}

static void from_json(const json &j, Color4ToggleThickness &ctt)
{
	from_json(j, static_cast<Color4Toggle &>(ctt));
	read(j, "Thickness", ctt.thickness);
}

static void from_json(const json &j, Color4ToggleThicknessRounding &cttr)
{
	from_json(j, static_cast<Color4ToggleRounding &>(cttr));
	read(j, "Thickness", cttr.thickness);
}

static void from_json(const json &j, Font &f)
{
	read<value_t::string>(j, "Name", f.name);

	if (!f.name.empty())
		config->scheduleFontLoad(f.name);

	if (const auto it = std::find_if(config->getSystemFonts().begin(), config->getSystemFonts().end(), [&f](const auto &e) { return e == f.name; }); it != config->getSystemFonts().end())
		f.index = std::distance(config->getSystemFonts().begin(), it);
	else
		f.index = 0;
}

static void from_json(const json& j, Snapline& s)
{
    from_json(j, static_cast<Color4ToggleThickness&>(s));

    read(j, "Type", s.type);
}

static void from_json(const json& j, Box& b)
{
    from_json(j, static_cast<Color4ToggleRounding&>(b));

    read(j, "Type", b.type);
    read(j, "Scale", b.scale);
    read<value_t::object>(j, "Fill", b.fill);
}

static void from_json(const json& j, Shared& s)
{
    read(j, "Enabled", s.enabled);
    read<value_t::object>(j, "Font", s.font);
    read<value_t::object>(j, "Snapline", s.snapline);
    read<value_t::object>(j, "Box", s.box);
    read<value_t::object>(j, "Name", s.name);
    read(j, "Text Cull Distance", s.textCullDistance);
}

static void from_json(const json& j, Weapon& w)
{
    from_json(j, static_cast<Shared&>(w));

    read<value_t::object>(j, "Ammo", w.ammo);
}

static void from_json(const json& j, Trail& t)
{
    from_json(j, static_cast<Color4BorderToggleThickness&>(t));

    read(j, "Type", t.type);
    read(j, "Time", t.time);
}

static void from_json(const json& j, Trails& t)
{
    read(j, "Enabled", t.enabled);
    read<value_t::object>(j, "Local Player", t.localPlayer);
    read<value_t::object>(j, "Allies", t.allies);
    read<value_t::object>(j, "Enemies", t.enemies);
}

static void from_json(const json& j, Projectile& p)
{
    from_json(j, static_cast<Shared&>(p));

    read<value_t::object>(j, "Trails", p.trails);
}

static void from_json(const json& j, Player& p)
{
    from_json(j, static_cast<Shared&>(p));

    read<value_t::object>(j, "Weapon", p.weapon);
    read<value_t::object>(j, "Flash Duration", p.flashDuration);
    read(j, "Audible Only", p.audibleOnly);
    read(j, "Spotted Only", p.spottedOnly);
    read<value_t::object>(j, "Health Bar", p.healthBar);
    read<value_t::object>(j, "Skeleton", p.skeleton);
    read<value_t::object>(j, "Head Box", p.headBox);
    read<value_t::object>(j, "Flags", p.flags);
}

static void from_json(const json& j, ImVec2& v)
{
    read(j, "X", v.x);
    read(j, "Y", v.y);
}

static void from_json(const json& j, Config::Aimbot& a)
{
    read<value_t::object>(j, "Bind", a.bind);
    read(j, "Aimlock", a.aimlock);
    read(j, "Multipoint", a.multipoint);
    read(j, "Multipoint scale", a.multipointScale);
    read(j, "Silent", a.silent);
    read(j, "Friendly fire", a.friendlyFire);
    read(j, "Visible only", a.visibleOnly);
    read(j, "Scoped only", a.scopedOnly);
    read(j, "Ignore flash", a.ignoreFlash);
    read(j, "Ignore smoke", a.ignoreSmoke);
    read(j, "Auto shot", a.autoShot);
    read(j, "Auto scope", a.autoScope);
    read(j, "Distance", a.distance);
    read(j, "Fov", a.fov);
    read(j, "Smooth start", a.smooth);
	read(j, "Linear speed", a.linearSpeed);
	read(j, "Interpolation", a.interpolation);
    read(j, "Hitgroup", a.hitgroup);
    read(j, "Targeting", a.targeting);
    read(j, "Max aim inaccuracy", a.maxAimInaccuracy);
    read(j, "Hitchance", a.shotHitchance);
    read(j, "Min damage", a.minDamage);
    read(j, "Min damage auto-wall", a.minDamageAutoWall);
    read(j, "Killshot", a.killshot);
    read(j, "Killshot auto-wall", a.killshotAutoWall);
    read(j, "Between shots", a.betweenShots);
    read<value_t::object>(j, "Safe only", a.safeOnly);
    read(j, "Safe mode", a.safeHitgroup);
    read(j, "On shot", a.onShot);
    read(j, "On move", a.onMove);
    read(j, "Target stop", a.targetStop);
}

static void from_json(const json& j, Config::Triggerbot& t)
{
    read<value_t::object>(j, "Bind", t.bind);
    read(j, "Friendly fire", t.friendlyFire);
    read(j, "Visible only", t.visibleOnly);
    read(j, "Scoped only", t.scopedOnly);
    read(j, "Ignore flash", t.ignoreFlash);
    read(j, "Ignore smoke", t.ignoreSmoke);
    read(j, "Hitgroup", t.hitgroup);
    read(j, "Hitchance", t.hitchance);
    read(j, "Distance", t.distance);
    read(j, "Shot delay", t.shotDelay);
    read(j, "Max inaccuracy", t.maxShotInaccuracy);
    read(j, "Min damage", t.minDamage);
    read(j, "Min damage auto-wall", t.minDamageAutoWall);
    read(j, "Killshot", t.killshot);
    read(j, "Killshot auto-wall", t.killshotAutoWall);
    read(j, "Burst Time", t.burstTime);
}

static void from_json(const json& j, Config::Backtrack& b)
{
    read(j, "Enabled", b.enabled);
    read(j, "Ignore smoke", b.ignoreSmoke);
    read(j, "Recoil based fov", b.recoilBasedFov);
    read(j, "Time limit", b.timeLimit);
}

static void from_json(const json& j, Config::AntiAim& a)
{
    read(j, "Pitch", a.pitch);
    read(j, "Pitch angle", a.pitchAngle);
    read(j, "Yaw", a.yaw);
    read(j, "Yaw angle", a.yawAngle);
    read(j, "Desync", a.desync);
    read(j, "Desync cor", a.corrected);
    read(j, "Desync clamp", a.clamped);
    read(j, "Desync ext", a.extended);
    read(j, "Desync avoid overlap", a.avoidOverlap);
    read(j, "Flip key", a.flipKey);
	read<value_t::object>(j, "Fake duck", a.fakeDuck);
	read(j, "Choked packets", a.chokedPackets);
	read<value_t::object>(j, "Choke", a.choke);
}

static void from_json(const json& j, Config::Glow& g)
{
    from_json(j, static_cast<Color4&>(g));

    read(j, "Enabled", g.enabled);
    read(j, "Health based", g.healthBased);
    read(j, "Style", g.style);
    read(j, "Full bloom", g.full);
}

static void from_json(const json& j, Config::Chams::Material& m)
{
    from_json(j, static_cast<Color4&>(m));

    read(j, "Enabled", m.enabled);
    read(j, "Health based", m.healthBased);
    read(j, "Blinking", m.blinking);
    read(j, "Wireframe", m.wireframe);
    read(j, "Cover", m.cover);
    read(j, "Ignore-Z", m.ignorez);
    read(j, "Material", m.material);
}

static void from_json(const json& j, Config::Chams& c)
{
    read_array_opt(j, "Materials", c.materials);
	read(j, "Trailing backtrack", c.trailBacktrack);
}

static void from_json(const json& j, Config::ESP& e)
{
    read(j, "Allies", e.allies);
    read(j, "Enemies", e.enemies);
    read(j, "Weapons", e.weapons);
    read(j, "Projectiles", e.projectiles);
    read(j, "Loot Crates", e.lootCrates);
    read(j, "Other Entities", e.otherEntities);
}

static void from_json(const json& j, Config::Visuals::ColorCorrection& c)
{
    read(j, "Enabled", c.enabled);
    read(j, "Blue", c.blue);
    read(j, "Red", c.red);
    read(j, "Mono", c.mono);
    read(j, "Saturation", c.saturation);
    read(j, "Ghost", c.ghost);
    read(j, "Green", c.green);
    read(j, "Yellow", c.yellow);
}

static void from_json(const json &j, Config::Visuals::Viewmodel &vxyz)
{
	read(j, "Enabled", vxyz.enabled);
	read(j, "Fov", vxyz.fov);
	read(j, "X", vxyz.x);
	read(j, "Y", vxyz.y);
	read(j, "Z", vxyz.z);
	read(j, "Roll", vxyz.roll);
}

static void from_json(const json &j, Config::Visuals::Beams &b)
{
	read(j, "Enabled", b.enabled);
	read(j, "Sprite", b.sprite);
	read(j, "Color", b.col);
	read(j, "Width", b.width);
	read(j, "Life", b.life);
	read(j, "Noise", b.noise);
	read(j, "Noise once", b.noiseOnce);
	read(j, "Railgun", b.railgun);
}

static void from_json(const json& j, Config::Visuals& v)
{
    read(j, "Disable post-processing", v.disablePostProcessing);
    read(j, "Inverse ragdoll gravity", v.inverseRagdollGravity);
    read(j, "No fog", v.noFog);
    read(j, "No 3d sky", v.no3dSky);
    read(j, "No land bob", v.noLandBob);
    read(j, "No aim punch", v.noAimPunch);
    read(j, "No view punch", v.noViewPunch);
    read(j, "No hands", v.noHands);
    read(j, "No sleeves", v.noSleeves);
    read(j, "No weapons", v.noWeapons);
    read(j, "No smoke", v.noSmoke);
    read(j, "No blur", v.noBlur);
    read(j, "No scope overlay", v.noScopeOverlay);
    read(j, "No grass", v.noGrass);
    read(j, "No shadows", v.noShadows);
    read(j, "Wireframe smoke", v.wireframeSmoke);
	read<value_t::object>(j, "Viewmodel", v.viewmodel);
    read<value_t::object>(j, "Zoom", v.zoom);
	read(j, "Zoom factor", v.zoomFac);
    read<value_t::object>(j, "Thirdperson", v.thirdPerson);
    read(j, "Thirdperson distance", v.thirdpersonDistance);
    read(j, "FOV", v.fov);
    read(j, "Force keep FOV", v.forceFov);
    read(j, "Far Z", v.farZ);
    read(j, "Flash reduction", v.flashReduction);
    read(j, "Brightness", v.brightness);
    read(j, "Skybox", v.skybox);
    read<value_t::object>(j, "World", v.world);
    read<value_t::object>(j, "Props", v.props);
    read<value_t::object>(j, "Sky", v.sky);
    read(j, "Deagle spinner", v.deagleSpinner);
    read(j, "Screen effect", v.screenEffect);
    read(j, "Hit effect", v.hitEffect);
    read(j, "Hit effect time", v.hitEffectTime);
    read(j, "Kill effect", v.killEffect);
    read(j, "Kill effect time", v.killEffectTime);
    read(j, "Hit marker", v.hitMarker);
    read(j, "Hit marker time", v.hitMarkerTime);
    read(j, "Playermodel T", v.playerModelT);
    read(j, "Playermodel CT", v.playerModelCT);
    read<value_t::object>(j, "Color correction", v.colorCorrection);
	read(j, "Aspect ratio", v.aspectratio);
	read(j, "Opposite hand knife", v.oppositeHandKnife);
	read(j, "Bullet impacts", v.bulletImpacts);
	read(j, "Accuracy tracers", v.accuracyTracers);
	read(j, "Model names", v.modelNames);
	read<value_t::object>(j, "Beams self", v.self);
	read<value_t::object>(j, "Beams ally", v.ally);
	read<value_t::object>(j, "Beams enemy", v.enemy);
	read<value_t::object>(j, "Inferno hull", v.molotovHull);
	read<value_t::object>(j, "Player bounds", v.playerBounds);
	read<value_t::object>(j, "Player velocity", v.playerVel);
	read<value_t::object>(j, "Console color", v.consoleCol);
}

static void from_json(const json& j, sticker_setting& s)
{
    read(j, "Kit", s.kit);
    read(j, "Wear", s.wear);
    read(j, "Scale", s.scale);
    read(j, "Rotation", s.rotation);

	s.onLoad();
}

static void from_json(const json& j, item_setting& i)
{
    read(j, "Enabled", i.enabled);
    read(j, "Definition index", i.itemId);
    read(j, "Quality", i.quality);

    read(j, "Paint Kit", i.paintKit);

    read(j, "Definition override", i.definition_override_index);

    read(j, "Seed", i.seed);
    read(j, "StatTrak", i.stat_trak);
    read(j, "Wear", i.wear);

    if (j.contains("Custom name"))
        strncpy_s(i.custom_name, j["Custom name"].get<std::string>().c_str(), _TRUNCATE);

    read(j, "Stickers", i.stickers);

	i.onLoad();
}

static void from_json(const json& j, Config::Sound::Player& p)
{
    read(j, "Master volume", p.masterVolume);
    read(j, "Headshot volume", p.headshotVolume);
    read(j, "Weapon volume", p.weaponVolume);
    read(j, "Footstep volume", p.footstepVolume);
}

static void from_json(const json& j, Config::Sound& s)
{
    read(j, "Chicken volume", s.chickenVolume);
    read(j, "Players", s.players);
	read(j, "Hit sound", s.hitSound);
	read(j, "Kill sound", s.killSound);
	read(j, "Hit sound volume", s.hitSoundVol);
	read(j, "Kill sound volume", s.killSoundVol);
	read<value_t::string>(j, "Custom hit sound", s.customHitSound);
	read<value_t::string>(j, "Custom kill sound", s.customKillSound);
}

static void from_json(const json& j, Config::Style& s)
{
    read(j, "Menu style", s.menuStyle);
    read(j, "Menu colors", s.menuColors);

    if (j.contains("Colors") && j["Colors"].is_object()) {
        const auto& colors = j["Colors"];

        ImGuiStyle& style = ImGui::GetStyle();

        for (int i = 0; i < ImGuiCol_COUNT; i++) {
            if (const char* name = ImGui::GetStyleColorName(i); colors.contains(name)) {
				std::array<float, 4> temp;
				read(colors, name, temp);
				style.Colors[i].x = temp[0];
				style.Colors[i].y = temp[1];
				style.Colors[i].z = temp[2];
				style.Colors[i].w = temp[3];
            }
        }
    }
}

static void from_json(const json& j, Config::Misc::PurchaseList& pl)
{
    read(j, "Enabled", pl.enabled);
    read(j, "Only During Freeze Time", pl.onlyDuringFreezeTime);
    read(j, "Show Prices", pl.showPrices);
    read(j, "No Title Bar", pl.noTitleBar);
    read(j, "Mode", pl.mode);
}

static void from_json(const json& j, Config::Misc::PreserveKillfeed& o)
{
    read(j, "Enabled", o.enabled);
    read(j, "Only Headshots", o.onlyHeadshots);
}

static void from_json(const json& j, Config::Misc& m)
{
    read(j, "Menu key", m.menuKey);
    read(j, "Auto pistol", m.autoPistol);
    read(j, "Auto reload", m.autoReload);
    read(j, "Auto accept", m.autoAccept);
    read<value_t::object>(j, "Spectator list", m.spectatorList);
	read<value_t::object>(j, "Spectator list background", m.specBg);
    read<value_t::object>(j, "Watermark", m.watermark);
	read<value_t::object>(j, "Watermark background", m.bg);
    read(j, "Fix animation LOD", m.fixAnimationLOD);
    read(j, "Fix bone matrix", m.fixBoneMatrix);
    read(j, "Fix movement", m.fixMovement);
    read(j, "Fix animations", m.fixAnimation);
    read(j, "Disable model occlusion", m.disableModelOcclusion);
    read(j, "Disable HUD blur", m.disablePanoramablur);
    read<value_t::object>(j, "Prepare revolver", m.prepareRevolver);
    read<value_t::object>(j, "Purchase list", m.purchaseList);
	read<value_t::object>(j, "Preserve killfeed", m.preserveKillfeed);
	read(j, "Quick healthshot key", m.quickHealthshotKey);
	read(j, "Radar hack", m.radarHack);
	read(j, "Reveal ranks", m.revealRanks);
	read(j, "Reveal money", m.revealMoney);
	read(j, "Reveal suspect", m.revealSuspect);
	read(j, "Fast plant", m.fastPlant);
	read(j, "Quick reload", m.quickReload);
	read(j, "Fix tablet signal", m.fixTabletSignal);
	read(j, "Aimstep", m.maxAngleDelta);
	read<value_t::object>(j, "Noscope crosshair", m.noscopeCrosshair);
	read<value_t::object>(j, "Recoil crosshair", m.recoilCrosshair);
	read<value_t::object>(j, "Offscreen Enemies", m.offscreenEnemies);
	read(j, "Bomb timer", m.bombTimer);
	read(j, "Grenade predict", m.nadePredict);
	read(j, "Spam use", m.spamUse);
	read(j, "Indicators", m.indicators);
}

static void from_json(const json &j, Config::Exploits &e)
{
	read(j, "Anti AFK kick", e.antiAfkKick);
	read(j, "Fast duck", e.fastDuck);
	read(j, "Moonwalk", e.moonwalk);
	read<value_t::object>(j, "Slowwalk", e.slowwalk);
	read<value_t::object>(j, "Doubletap", e.doubletap);
	read(j, "Bypass sv_pure", e.bypassPure);
}

static void from_json(const json &j, Config::Griefing &s)
{
	read(j, "Custom clantag", s.customClanTag);
	read(j, "Clock tag", s.clocktag);
	if (j.contains("Clantag"))
		strncpy_s(s.clanTag, j["Clantag"].get<std::string>().c_str(), _TRUNCATE);
	read(j, "Animated clantag", s.animatedClanTag);
	read(j, "Kill message", s.killMessage);
	read<value_t::string>(j, "Kill message string", s.killMessageString);
	read(j, "Name stealer", s.nameStealer);
	read(j, "Ban color", s.banColor);
	read<value_t::string>(j, "Ban text", s.banText);
	read<value_t::object>(j, "Reportbot", s.reportbot);
	read(j, "Fake prime", s.fakePrime);
	read(j, "Vote reveal", s.revealVotes);
	read<value_t::object>(j, "Blockbot", s.bb);
	read<value_t::object>(j, "Blockbot target", s.bbTar);
	read(j, "Blockbot factor", s.bbTrajFac);
	read(j, "Blockbot dist factor", s.bbDistFac);
	read<value_t::object>(j, "Blockbot color", s.bbCol);
}

static void from_json(const json& j, Config::Griefing::Reportbot& r)
{
    read(j, "Enabled", r.enabled);
    read(j, "Target", r.target);
    read(j, "Delay", r.delay);
    read(j, "Rounds", r.rounds);
    read(j, "Abusive Communications", r.textAbuse);
    read(j, "Griefing", r.griefing);
    read(j, "Wall Hacking", r.wallhack);
    read(j, "Aim Hacking", r.aimbot);
    read(j, "Other Hacking", r.other);
}

static void from_json(const json &j, Config::Movement &m)
{
	read(j, "Bunny hop", m.bunnyHop);
	read(j, "Auto strafe", m.autoStrafe);
	read(j, "Steer", m.steerSpeed);
	read<value_t::object>(j, "Edge jump", m.edgeJump);
	read(j, "Fast stop", m.fastStop);
}

bool Config::load(const char8_t *name, bool incremental) noexcept
{
	json j;

	if (std::ifstream in{path / name}; in.good())
	{
		j = json::parse(in, nullptr, false);
		if (j.is_discarded())
			return false;
	}
	else return false;

	if (!incremental)
		reset();

	read(j, "Aimbot", aimbot);
	read(j, "Triggerbot", triggerbot);
	read<value_t::object>(j, "Backtrack", backtrack);
	read<value_t::object>(j, "Anti aim", antiAim);
	read(j, "Glow", glow);
	read(j, "Chams", chams);
	read<value_t::object>(j, "ESP", esp);
	read<value_t::object>(j, "Visuals", visuals);
	read(j, "Skin changer", skinChanger);
	read<value_t::object>(j, "Sound", sound);
	read<value_t::object>(j, "Style", style);
	read<value_t::object>(j, "Misc", misc);
	read<value_t::object>(j, "Exploits", exploits);
	read<value_t::object>(j, "Movement", movement);
	read<value_t::object>(j, "Griefing", griefing);

	return true;
}

bool Config::load(size_t id, bool incremental) noexcept
{
	return load((const char8_t *)configs[id].c_str(), incremental);
}

// WRITE macro requires:
// - json object named 'j'
// - object holding default values named 'dummy'
// - object to write to json named 'o'
#define WRITE(name, valueName) to_json(j[name], o.valueName, dummy.valueName)

template <typename T>
static void to_json(json& j, const T& o, const T& dummy)
{
    if (o != dummy)
        j = o;
}

static void to_json(json &j, const KeyBind &o, const KeyBind &dummy = {})
{
	WRITE("Key", key);
	WRITE("Key mode", keyMode);
}

static void to_json(json &j, const Color4 &o, const Color4 &dummy = {})
{
	WRITE("Color", color);
	WRITE("Rainbow", rainbow);
	WRITE("Rainbow Speed", rainbowSpeed);
}

static void to_json(json &j, const Color4Border &o, const Color4Border &dummy = {})
{
	to_json(j, static_cast<const Color4 &>(o), dummy);
	WRITE("Border", border);
}

static void to_json(json &j, const Color4BorderToggle &o, const Color4BorderToggle &dummy = {})
{
	to_json(j, static_cast<const Color4Border &>(o), dummy);
	WRITE("Enabled", enabled);
}

static void to_json(json &j, const Color4BorderToggleThickness &o, const Color4BorderToggleThickness &dummy = {})
{
	to_json(j, static_cast<const Color4BorderToggle &>(o), dummy);
	WRITE("Thickness", thickness);
}

static void to_json(json &j, const Color4Toggle &o, const Color4Toggle &dummy = {})
{
	to_json(j, static_cast<const Color4 &>(o), dummy);
	WRITE("Enabled", enabled);
}

static void to_json(json &j, const Color3 &o, const Color3 &dummy = {})
{
	WRITE("Color", color);
	WRITE("Rainbow", rainbow);
	WRITE("Rainbow Speed", rainbowSpeed);
}

static void to_json(json &j, const Color3Toggle &o, const Color3Toggle &dummy = {})
{
	to_json(j, static_cast<const Color3 &>(o), dummy);
	WRITE("Enabled", enabled);
}

static void to_json(json &j, const Color4ToggleRounding &o, const Color4ToggleRounding &dummy = {})
{
	to_json(j, static_cast<const Color4Toggle &>(o), dummy);
	WRITE("Rounding", rounding);
}

static void to_json(json &j, const Color4ToggleThickness &o, const Color4ToggleThickness &dummy = {})
{
	to_json(j, static_cast<const Color4Toggle &>(o), dummy);
	WRITE("Thickness", thickness);
}

static void to_json(json &j, const Color4ToggleThicknessRounding &o, const Color4ToggleThicknessRounding &dummy = {})
{
	to_json(j, static_cast<const Color4ToggleRounding &>(o), dummy);
	WRITE("Thickness", thickness);
}

static void to_json(json& j, const Font& o, const Font& dummy = {})
{
    WRITE("Name", name);
}

static void to_json(json& j, const Snapline& o, const Snapline& dummy = {})
{
    to_json(j, static_cast<const Color4ToggleThickness&>(o), dummy);
    WRITE("Type", type);
}

static void to_json(json& j, const Box& o, const Box& dummy = {})
{
    to_json(j, static_cast<const Color4ToggleRounding&>(o), dummy);
    WRITE("Type", type);
    WRITE("Scale", scale);
    WRITE("Fill", fill);
}

static void to_json(json& j, const Shared& o, const Shared& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Font", font);
    WRITE("Snapline", snapline);
    WRITE("Box", box);
    WRITE("Name", name);
    WRITE("Text Cull Distance", textCullDistance);
}

static void to_json(json& j, const Player& o, const Player& dummy = {})
{
    to_json(j, static_cast<const Shared&>(o), dummy);
    WRITE("Weapon", weapon);
    WRITE("Flash Duration", flashDuration);
    WRITE("Audible Only", audibleOnly);
    WRITE("Spotted Only", spottedOnly);
    WRITE("Health Bar", healthBar);
    WRITE("Skeleton", skeleton);
    WRITE("Head Box", headBox);
    WRITE("Flags", flags);
}

static void to_json(json& j, const Weapon& o, const Weapon& dummy = {})
{
    to_json(j, static_cast<const Shared&>(o), dummy);
    WRITE("Ammo", ammo);
}

static void to_json(json& j, const Trail& o, const Trail& dummy = {})
{
    to_json(j, static_cast<const Color4BorderToggleThickness&>(o), dummy);
    WRITE("Type", type);
    WRITE("Time", time);
}

static void to_json(json& j, const Trails& o, const Trails& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Local Player", localPlayer);
    WRITE("Allies", allies);
    WRITE("Enemies", enemies);
}

static void to_json(json& j, const Projectile& o, const Projectile& dummy = {})
{
    j = static_cast<const Shared&>(o);

    WRITE("Trails", trails);
}

static void to_json(json& j, const ImVec2& o, const ImVec2& dummy = {})
{
    WRITE("X", x);
    WRITE("Y", y);
}

static void to_json(json& j, const Config::Aimbot& o, const Config::Aimbot& dummy = {})
{
    WRITE("Bind", bind);
    WRITE("Aimlock", aimlock);
    WRITE("Multipoint", multipoint);
    WRITE("Multipoint scale", multipointScale);
    WRITE("Silent", silent);
    WRITE("Friendly fire", friendlyFire);
    WRITE("Visible only", visibleOnly);
    WRITE("Scoped only", scopedOnly);
    WRITE("Ignore flash", ignoreFlash);
    WRITE("Ignore smoke", ignoreSmoke);
    WRITE("Auto shot", autoShot);
    WRITE("Auto scope", autoScope);
    WRITE("Distance", distance);
    WRITE("Fov", fov);
    WRITE("Smooth start", smooth);
	WRITE("Linear speed", linearSpeed);
	WRITE("Interpolation", interpolation);
    WRITE("Hitgroup", hitgroup);
    WRITE("Targeting", targeting);
    WRITE("Max aim inaccuracy", maxAimInaccuracy);
    WRITE("Hitchance", shotHitchance);
    WRITE("Min damage", minDamage);
    WRITE("Min damage auto-wall", minDamageAutoWall);
    WRITE("Killshot", killshot);
    WRITE("Killshot auto-wall", killshotAutoWall);
    WRITE("Between shots", betweenShots);
	WRITE("Safe only", safeOnly);
	WRITE("Safe mode", safeHitgroup);
	WRITE("On shot", onShot);
	WRITE("On move", onMove);
	WRITE("Target stop", targetStop);
}

static void to_json(json& j, const Config::Triggerbot& o, const Config::Triggerbot& dummy = {})
{
    WRITE("Bind", bind);
    WRITE("Friendly fire", friendlyFire);
    WRITE("Visible only", visibleOnly);
    WRITE("Scoped only", scopedOnly);
    WRITE("Ignore flash", ignoreFlash);
    WRITE("Ignore smoke", ignoreSmoke);
    WRITE("Hitgroup", hitgroup);
    WRITE("Hitchance", hitchance);
    WRITE("Distance", distance);
    WRITE("Shot delay", shotDelay);
    WRITE("Max inaccuracy", maxShotInaccuracy);
    WRITE("Min damage", minDamage);
    WRITE("Min damage auto-wall", minDamageAutoWall);
    WRITE("Killshot", killshot);
    WRITE("Killshot auto-wall", killshotAutoWall);
    WRITE("Burst Time", burstTime);
}

static void to_json(json& j, const Config::Backtrack& o, const Config::Backtrack& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Ignore smoke", ignoreSmoke);
    WRITE("Recoil based fov", recoilBasedFov);
    WRITE("Time limit", timeLimit);
}

static void to_json(json& j, const Config::AntiAim& o, const Config::AntiAim& dummy = {})
{
    WRITE("Pitch", pitch);
    WRITE("Pitch angle", pitchAngle);
    WRITE("Yaw", yaw);
    WRITE("Yaw angle", yawAngle);
    WRITE("Desync", desync);
    WRITE("Desync cor", corrected);
    WRITE("Desync clamp", clamped);
    WRITE("Desync ext", extended);
    WRITE("Desync avoid overlap", avoidOverlap);
    WRITE("Flip key", flipKey);
	WRITE("Fake duck", fakeDuck);
	WRITE("Choked packets", chokedPackets);
	WRITE("Choke", choke);
}

static void to_json(json& j, const Config::Glow& o, const Config::Glow& dummy = {})
{
    to_json(j, static_cast<const Color4&>(o), dummy);
    WRITE("Enabled", enabled);
    WRITE("Health based", healthBased);
    WRITE("Style", style);
    WRITE("Full bloom", full);
}

static void to_json(json& j, const Config::Chams::Material& o)
{
    const Config::Chams::Material dummy;

    to_json(j, static_cast<const Color4&>(o), dummy);
    WRITE("Enabled", enabled);
    WRITE("Health based", healthBased);
    WRITE("Blinking", blinking);
    WRITE("Wireframe", wireframe);
    WRITE("Cover", cover);
    WRITE("Ignore-Z", ignorez);
    WRITE("Material", material);
}

static void to_json(json& j, const Config::Chams& o)
{
    j["Materials"] = o.materials;
	j["Trailing backtrack"] = o.trailBacktrack;
}

static void to_json(json& j, const Config::ESP& o)
{
    j["Allies"] = o.allies;
    j["Enemies"] = o.enemies;
    j["Weapons"] = o.weapons;
    j["Projectiles"] = o.projectiles;
    j["Loot Crates"] = o.lootCrates;
    j["Other Entities"] = o.otherEntities;
}

static void to_json(json& j, const Config::Sound::Player& o)
{
    const Config::Sound::Player dummy;

    WRITE("Master volume", masterVolume);
    WRITE("Headshot volume", headshotVolume);
    WRITE("Weapon volume", weaponVolume);
    WRITE("Footstep volume", footstepVolume);
}

static void to_json(json& j, const Config::Sound& o)
{
    const Config::Sound dummy;

    WRITE("Chicken volume", chickenVolume);
    j["Players"] = o.players;
	WRITE("Hit sound", hitSound);
	WRITE("Kill sound", killSound);
	WRITE("Hit sound volume", hitSoundVol);
	WRITE("Kill sound volume", killSoundVol);
	//j["Custom hit sound"] = o.customHitSound;
	//j["Custom kill sound"] = o.customKillSound;
	WRITE("Custom hit sound", customHitSound);
	WRITE("Custom kill sound", customKillSound);
}

static void to_json(json& j, const Config::Misc::PurchaseList& o, const Config::Misc::PurchaseList& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Only During Freeze Time", onlyDuringFreezeTime);
    WRITE("Show Prices", showPrices);
    WRITE("No Title Bar", noTitleBar);
    WRITE("Mode", mode);
}

static void to_json(json& j, const Config::Misc::PreserveKillfeed& o, const Config::Misc::PreserveKillfeed& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Only Headshots", onlyHeadshots);
}

static void to_json(json& j, const Config::Misc& o)
{
    const Config::Misc dummy;

	WRITE("Noscope crosshair", noscopeCrosshair);
	WRITE("Recoil crosshair", recoilCrosshair);
	WRITE("Offscreen Enemies", offscreenEnemies);
	WRITE("Bomb timer", bombTimer);
	WRITE("Grenade predict", nadePredict);
    WRITE("Menu key", menuKey);
    WRITE("Auto pistol", autoPistol);
    WRITE("Auto reload", autoReload);
    WRITE("Auto accept", autoAccept);
    WRITE("Spectator list", spectatorList);
    WRITE("Spectator list background", specBg);
    WRITE("Watermark", watermark);
    WRITE("Watermark background", bg);
    WRITE("Fix animation LOD", fixAnimationLOD);
    WRITE("Fix bone matrix", fixBoneMatrix);
    WRITE("Fix movement", fixMovement);
    WRITE("Fix animations", fixAnimation);
    WRITE("Disable model occlusion", disableModelOcclusion);
    WRITE("Disable HUD blur", disablePanoramablur);
    WRITE("Prepare revolver", prepareRevolver);
    WRITE("Quick healthshot key", quickHealthshotKey);
    WRITE("Purchase list", purchaseList);
    WRITE("Preserve killfeed", preserveKillfeed);
	WRITE("Reveal ranks", revealRanks);
	WRITE("Reveal money", revealMoney);
	WRITE("Reveal suspect", revealSuspect);
	WRITE("Radar hack", radarHack);
	WRITE("Fast plant", fastPlant);
	WRITE("Quick reload", quickReload);
	WRITE("Fix tablet signal", fixTabletSignal);
	WRITE("Aimstep", maxAngleDelta);
	WRITE("Spam use", spamUse);
	WRITE("Indicators", indicators);
}

static void to_json(json &j, const Config::Exploits &o)
{
	const Config::Exploits dummy;

	WRITE("Anti AFK kick", antiAfkKick);
	WRITE("Fast duck", fastDuck);
	WRITE("Moonwalk", moonwalk);
	WRITE("Slowwalk", slowwalk);
	WRITE("Doubletap", doubletap);
	WRITE("Bypass sv_pure", bypassPure);
}

static void to_json(json &j, const Config::Griefing::Reportbot &o, const Config::Griefing::Reportbot &dummy = {})
{
	WRITE("Enabled", enabled);
	WRITE("Target", target);
	WRITE("Delay", delay);
	WRITE("Rounds", rounds);
	WRITE("Abusive Communications", textAbuse);
	WRITE("Griefing", griefing);
	WRITE("Wall Hacking", wallhack);
	WRITE("Aim Hacking", aimbot);
	WRITE("Other Hacking", other);
}

static void to_json(json &j, const Config::Griefing &o)
{
	const Config::Griefing dummy;

	WRITE("Custom clantag", customClanTag);
	WRITE("Clocktag", clocktag);

	if (o.clanTag[0])
		j["Clantag"] = o.clanTag;

	WRITE("Animated clantag", animatedClanTag);
	WRITE("Name stealer", nameStealer);
	WRITE("Kill message", killMessage);
	WRITE("Kill message string", killMessageString);
	WRITE("Fake prime", fakePrime);
	WRITE("Vote reveal", revealVotes);
	WRITE("Ban color", banColor);
	WRITE("Ban text", banText);
	WRITE("Reportbot", reportbot);
	WRITE("Blockbot", bb);
	WRITE("Blockbot target", bbTar);
	WRITE("Blockbot factor", bbTrajFac);
	WRITE("Blockbot dist factor", bbDistFac);
	WRITE("Blockbot color", bbCol);
}

static void to_json(json &j, const Config::Movement &o)
{
	const Config::Movement dummy;

	WRITE("Bunny hop", bunnyHop);
	WRITE("Auto strafe", autoStrafe);
	WRITE("Steer", steerSpeed);
	WRITE("Edge jump", edgeJump);
	WRITE("Fast stop", fastStop);
}

static void to_json(json &j, const Config::Visuals::ColorCorrection &o, const Config::Visuals::ColorCorrection &dummy)
{
	WRITE("Enabled", enabled);
	WRITE("Blue", blue);
	WRITE("Red", red);
	WRITE("Mono", mono);
	WRITE("Saturation", saturation);
	WRITE("Ghost", ghost);
	WRITE("Green", green);
	WRITE("Yellow", yellow);
}

static void to_json(json &j, const Config::Visuals::Viewmodel &o, const Config::Visuals::Viewmodel &dummy)
{
	WRITE("Enabled", enabled);
	WRITE("Fov", fov);
	WRITE("X", x);
	WRITE("Y", y);
	WRITE("Z", z);
	WRITE("Roll", roll);
}

static void to_json(json &j, const Config::Visuals::Beams &o, const Config::Visuals::Beams &dummy)
{
	WRITE("Enabled", enabled);
	WRITE("Sprite", sprite);
	WRITE("Color", col);
	WRITE("Width", width);
	WRITE("Life", life);
	WRITE("Noise", noise);
	WRITE("Noise once", noiseOnce);
	WRITE("Railgun", railgun);
}

static void to_json(json& j, const Config::Visuals& o)
{
    const Config::Visuals dummy;

	WRITE("Aspect ratio", aspectratio);
	WRITE("Opposite hand knife", oppositeHandKnife);
    WRITE("Disable post-processing", disablePostProcessing);
    WRITE("Inverse ragdoll gravity", inverseRagdollGravity);
    WRITE("No fog", noFog);
    WRITE("No 3d sky", no3dSky);
    WRITE("No land bob", noLandBob);
    WRITE("No aim punch", noAimPunch);
    WRITE("No view punch", noViewPunch);
    WRITE("No hands", noHands);
    WRITE("No sleeves", noSleeves);
    WRITE("No weapons", noWeapons);
    WRITE("No smoke", noSmoke);
    WRITE("No blur", noBlur);
    WRITE("No scope overlay", noScopeOverlay);
    WRITE("No grass", noGrass);
    WRITE("No shadows", noShadows);
    WRITE("Wireframe smoke", wireframeSmoke);
	WRITE("Viewmodel", viewmodel);
    WRITE("Zoom", zoom);
    WRITE("Zoom factor", zoomFac);
    WRITE("Thirdperson", thirdPerson);
    WRITE("Thirdperson distance", thirdpersonDistance);
    WRITE("FOV", fov);
    WRITE("Force keep FOV", forceFov);
    WRITE("Far Z", farZ);
    WRITE("Flash reduction", flashReduction);
    WRITE("Brightness", brightness);
    WRITE("Skybox", skybox);
    WRITE("World", world);
    WRITE("Props", props);
    WRITE("Sky", sky);
    WRITE("Deagle spinner", deagleSpinner);
    WRITE("Screen effect", screenEffect);
    WRITE("Hit effect", hitEffect);
    WRITE("Hit effect time", hitEffectTime);
    WRITE("Kill effect", killEffect);
    WRITE("Kill effect time", killEffectTime);
    WRITE("Hit marker", hitMarker);
    WRITE("Hit marker time", hitMarkerTime);
    WRITE("Playermodel T", playerModelT);
    WRITE("Playermodel CT", playerModelCT);
    WRITE("Color correction", colorCorrection);
	WRITE("Bullet impacts", bulletImpacts);
	WRITE("Accuracy tracers", accuracyTracers);
	WRITE("Model names", modelNames);
	WRITE("Beams self", self);
	WRITE("Beams ally", ally);
	WRITE("Beams enemy", enemy);
	WRITE("Inferno hull", molotovHull);
	WRITE("Player bounds", playerBounds);
	WRITE("Player velocity", playerVel);
	WRITE("Console color", consoleCol);
}

static void to_json(json& j, const ImVec4& o)
{
    j[0] = o.x;
    j[1] = o.y;
    j[2] = o.z;
    j[3] = o.w;
}

static void to_json(json& j, const Config::Style& o)
{
    const Config::Style dummy;

    WRITE("Menu style", menuStyle);
    WRITE("Menu colors", menuColors);

    auto& colors = j["Colors"];
    ImGuiStyle& style = ImGui::GetStyle();

    for (int i = 0; i < ImGuiCol_COUNT; i++)
        colors[ImGui::GetStyleColorName(i)] = style.Colors[i];
}

static void to_json(json& j, const sticker_setting& o)
{
    const sticker_setting dummy;

    WRITE("Kit", kit);
    WRITE("Wear", wear);
    WRITE("Scale", scale);
    WRITE("Rotation", rotation);
}

static void to_json(json& j, const item_setting& o)
{
    const item_setting dummy;

    WRITE("Enabled", enabled);
    WRITE("Definition index", itemId);
    WRITE("Quality", quality);
    WRITE("Paint Kit", paintKit);
    WRITE("Definition override", definition_override_index);
    WRITE("Seed", seed);
    WRITE("StatTrak", stat_trak);
    WRITE("Wear", wear);
    if (o.custom_name[0])
        j["Custom name"] = o.custom_name;
    WRITE("Stickers", stickers);
}

void removeEmptyObjects(json& j) noexcept
{
    for (auto it = j.begin(); it != j.end();) {
        auto& val = it.value();
        if (val.is_object() || val.is_array())
            removeEmptyObjects(val);
        if (val.empty() && !j.is_array())
            it = j.erase(it);
        else
            ++it;
    }
}

void Config::save(size_t id) const noexcept
{
    std::error_code ec;
    std::filesystem::create_directory(path, ec);

    if (std::ofstream out{ path / (const char8_t*)configs[id].c_str() }; out.good()) {
        json j;

        j["Aimbot"] = aimbot;
        j["Triggerbot"] = triggerbot;
        j["Backtrack"] = backtrack;
        j["Anti aim"] = antiAim;
        j["Glow"] = glow;
        j["Chams"] = chams;
        j["ESP"] = esp;
        j["Sound"] = sound;
        j["Visuals"] = visuals;
        j["Misc"] = misc;
        j["Style"] = style;
        j["Skin changer"] = skinChanger;
		j["Exploits"] = exploits;
		j["Movement"] = movement;
		j["Griefing"] = griefing;

        removeEmptyObjects(j);
        out << std::setw(2) << j;
    }
}

void Config::add(const char* name) noexcept
{
    if (*name && std::find(configs.cbegin(), configs.cend(), name) == configs.cend()) {
        configs.emplace_back(name);
        save(configs.size() - 1);
    }
}

void Config::remove(size_t id) noexcept
{
    std::error_code ec;
    std::filesystem::remove(path / (const char8_t*)configs[id].c_str(), ec);
    configs.erase(configs.cbegin() + id);
}

void Config::rename(size_t item, const char* newName) noexcept
{
    std::error_code ec;
    std::filesystem::rename(path / (const char8_t*)configs[item].c_str(), path / (const char8_t*)newName, ec);
    configs[item] = newName;
}

void Config::reset() noexcept
{
	aimbot = {};
	antiAim = {};
	triggerbot = {};
	backtrack = {};
	glow = {};
	chams = {};
	esp = {};
	visuals = {};
	skinChanger = {};
	sound = {};
	style = {};
	exploits = {};
	griefing = {};
	movement = {};
	misc = {};
}

void Config::listConfigs() noexcept
{
    configs.clear();

    std::error_code ec;
    std::transform(std::filesystem::directory_iterator{ path, ec },
        std::filesystem::directory_iterator{ },
        std::back_inserter(configs),
        [](const auto& entry) { return std::string{ (const char*)entry.path().filename().u8string().c_str() }; });
}

void Config::openConfigDir() const noexcept
{
	std::error_code ec;
	std::filesystem::create_directory(path, ec);
	ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void Config::scheduleFontLoad(const std::string &name) noexcept
{
	scheduledFonts.push_back(name);
}

#ifdef _WIN32
static auto getFontData(const std::string& fontName) noexcept
{
    HFONT font = CreateFontA(0, 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH, fontName.c_str());

    std::unique_ptr<std::byte[]> data;
    DWORD dataSize = GDI_ERROR;

    if (font) {
        HDC hdc = CreateCompatibleDC(nullptr);

        if (hdc) {
            SelectObject(hdc, font);
            dataSize = GetFontData(hdc, 0, 0, nullptr, 0);

            if (dataSize != GDI_ERROR) {
                data = std::make_unique<std::byte[]>(dataSize);
                dataSize = GetFontData(hdc, 0, 0, data.get(), dataSize);

                if (dataSize == GDI_ERROR)
                    data.reset();
            }
            DeleteDC(hdc);
        }
        DeleteObject(font);
    }
    return std::make_pair(std::move(data), dataSize);
}
#endif

bool Config::loadScheduledFonts() noexcept
{
	bool result = false;

	for (const auto &fontName : scheduledFonts)
	{
		#define FONT_BIG 13.0f
		#define FONT_MEDIUM 11.0f
		#define FONT_TINY 9.0f

		if (fontName == "Default")
		{
			if (fonts.find("Default") == fonts.cend())
			{
				Font newFont;
				newFont.big = gui->getFont();
				newFont.tiny = newFont.medium = newFont.big;

				fonts.emplace(fontName, newFont);
				result = true;
			}
			continue;
		}

		#ifdef _WIN32
		const auto [fontData, fontDataSize] = getFontData(fontName);
		if (fontDataSize == GDI_ERROR)
			continue;

		if (fonts.find(fontName) == fonts.cend())
		{
			const auto ranges = Helpers::getFontGlyphRanges();
			ImFontConfig cfg;
			cfg.FontDataOwnedByAtlas = false;
			cfg.OversampleH = cfg.OversampleV = 8;
			cfg.PixelSnapH = false;
			cfg.SizePixels = FONT_BIG;

			Font newFont;
			newFont.big = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(fontData.get(), fontDataSize, FONT_BIG, &cfg, ranges);
			newFont.tiny = newFont.medium = newFont.big;
			fonts.emplace(fontName, newFont);
			result = true;
		}
		#endif
	}
	scheduledFonts.clear();
	return result;
}
