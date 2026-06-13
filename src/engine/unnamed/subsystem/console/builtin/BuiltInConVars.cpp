#include "BuiltInConVars.h"

#include <engine/unnamed/subsystem/console/concommand/ConVar.h>

namespace Unnamed {
	void RegisterBuiltInConVars() {
		EngineConVar();
		ClientConVar();
		ServerConVar();

#ifdef UNNAMED_WITH_EDITOR
		EditorConVar();
#endif
	}

	void EngineConVar() {
		static ConVar<std::string> im_configpath(
			"im_configpath",
			"./content/core/settings/imconfig.json",
			FCVAR::NONE, "Path to ImGui config file."
		);

		static ConVar<std::string> im_guizmoconfigpath(
			"im_guizmoconfigpath",
			"./content/core/settings/imguizmo.json",
			FCVAR::NONE, "Path to ImGuizmo config file."
		);

		//---------------------------------------------------------------------
		// Asset
		//---------------------------------------------------------------------
		static ConVar asset_hotreloadpollinterval(
			"asset_hotreloadpollinterval",
			0.25f, // まあリアルタイムといえるでしょう
			FCVAR::ARCHIVE,
			"アセットのホットリロード確認の間隔（秒）",
			true,
			0.125f, // 0.125s = 8回/s
			true,
			5.0f // 適当に上限を設ける
		);

		//---------------------------------------------------------------------
		// Renderer
		//---------------------------------------------------------------------
		static ConVar r_vsync(
			"r_vsync", false, FCVAR::ARCHIVE,
			"Vertical sync"
		);

		static ConVar r_clear(
			"r_clear", true, FCVAR::NONE,
			"Clear the backbuffer each frame."
		);

		static ConVar post_bloommipcount(
			"post_bloommipcount", 2, FCVAR::ARCHIVE,
			"Number of mip levels to use for bloom effect."
		);

		static ConVar r_ui_text_sampler_mode(
			"r_ui_text_sampler_mode", 0, FCVAR::ARCHIVE,
			"UI text sampler mode: 0=default(aniso+wrap), 1=linear+clamp, 2=point+clamp.",
			true,
			0,
			true,
			2
		);

		static ConVar r_shadowmap_size(
			"r_shadowmap_size", 1024, FCVAR::ARCHIVE,
			"Directional shadow map resolution.",
			true,
			256,
			true,
			8192
		);

		static ConVar r_shadowmap_debug(
			"r_shadowmap_debug", false, FCVAR::ARCHIVE,
			"Draw the directional shadow map depth texture as a backbuffer overlay."
		);

		static ConVar r_shadowmap_debug_size(
			"r_shadowmap_debug_size", 256, FCVAR::ARCHIVE,
			"Shadow map debug overlay size in pixels.",
			true,
			64,
			true,
			1024
		);

		static ConVar r_shadowmap_enabled(
			"r_shadowmap_enabled", false, FCVAR::ARCHIVE,
			"Apply the directional shadow map in geometry lighting."
		);

		static ConVar r_shadowmap_bias(
			"r_shadowmap_bias", 0.0005f, FCVAR::ARCHIVE,
			"Directional shadow map depth bias.",
			true,
			0.0f,
			true,
			0.05f
		);

		static ConVar r_shadowmap_normal_bias(
			"r_shadowmap_normal_bias", 0.0f, FCVAR::ARCHIVE,
			"Directional shadow map normal offset bias in world units.",
			true,
			0.0f,
			true,
			0.5f
		);

		static ConVar r_shadowmap_strength(
			"r_shadowmap_strength", 0.65f, FCVAR::ARCHIVE,
			"Directional shadow map strength.",
			true,
			0.0f,
			true,
			1.0f
		);

		static ConVar r_shadowmap_pcf_enabled(
			"r_shadowmap_pcf_enabled", true, FCVAR::ARCHIVE,
			"Enable 3x3 PCF filtering for the directional shadow map."
		);

		static ConVar r_shadowmap_pcf_radius(
			"r_shadowmap_pcf_radius", 1.0f, FCVAR::ARCHIVE,
			"Directional shadow map 3x3 PCF radius in texels.",
			true,
			0.0f,
			true,
			4.0f
		);

		static ConVar r_shadowmap_force_cull_none(
			"r_shadowmap_force_cull_none", false, FCVAR::ARCHIVE,
			"Force CullNone for directional shadow map casters."
		);

		static ConVar ui_new_font_size_preset(
			"ui_new_font_size_preset", 0, FCVAR::ARCHIVE,
			"New UI font size preset index: 0=16, 1=18, 2=20, 3=24.",
			true,
			0,
			true,
			3
		);

		static ConVar ui_new_font_oversample_preset(
			"ui_new_font_oversample_preset", 1, FCVAR::ARCHIVE,
			"New UI oversample preset index: 0=1x1, 1=2x2, 2=3x1, 3=3x2.",
			true,
			0,
			true,
			3
		);

		static ConVar ui_new_text_force_fallback_texture(
			"ui_new_text_force_fallback_texture", false, FCVAR::ARCHIVE,
			"Force NewUI text glyph sprites to use fallback white texture (debug)."
		);

		static ConVar ui_new_text_debug_mode(
			"ui_new_text_debug_mode", 0, FCVAR::ARCHIVE,
			"NewUI text debug mode. 0=normal, 4=force first glyph to red 64x64 rect."
		);
	}

	void EditorConVar() {
		static ConVar ed_gridcolor(
			"ed_gridcolor", Vec3(0.28f, 0.28f, 0.28f), FCVAR::ARCHIVE,
			"Editor grid color."
		);

		static ConVar ed_gridmajorcolor_r(
			"ed_gridmajorcolor_r", 0.39f, FCVAR::ARCHIVE,
			"Editor grid major line r color."
		);
		static ConVar ed_gridmajorcolor_g(
			"ed_gridmajorcolor_g", 0.2f, FCVAR::ARCHIVE,
			"Editor grid major line g color."
		);
		static ConVar ed_gridmajorcolor_b(
			"ed_gridmajorcolor_b", 0.02f, FCVAR::ARCHIVE,
			"Editor grid major line b color."
		);

		static ConVar ed_gridaxiscolor_r(
			"ed_gridaxiscolor_r", 0.0f, FCVAR::ARCHIVE,
			"Editor grid axis line r color."
		);
		static ConVar ed_gridaxiscolor_g(
			"ed_gridaxiscolor_g", 0.39f, FCVAR::ARCHIVE,
			"Editor grid axis line g color."
		);
		static ConVar ed_gridaxiscolor_b(
			"ed_gridaxiscolor_b", 0.39f, FCVAR::ARCHIVE,
			"Editor grid axis line b color."
		);

		static ConVar ed_gridminorcolor_r(
			"ed_gridminorcolor_r", 0.39f, FCVAR::ARCHIVE,
			"Editor grid minor line r color."
		);
		static ConVar ed_gridminorcolor_g(
			"ed_gridminorcolor_g", 0.39f, FCVAR::ARCHIVE,
			"Editor grid minor line g color."
		);
		static ConVar ed_gridminorcolor_b(
			"ed_gridminorcolor_b", 0.39f, FCVAR::ARCHIVE,
			"Editor grid minor line b color."
		);
	}

	void ClientConVar() {
		static ConVar<std::string> name(
			"name", "unnamed", FCVAR::NOTIFY,
			"Current user name."
		);

		static ConVar cl_showpos(
			"cl_showpos",
#ifdef UNNAMED_WITH_EDITOR
			1,
#else
			0,
#endif
			FCVAR::CHEAT,
			"Draw current position at top of screen (1 = meter, 2 = hammer)"
		);

		static ConVar cl_showfps(
			"cl_showfps",
#ifdef UNNAMED_WITH_EDITOR
			2,
#else
			0,
#endif
			FCVAR::NONE,
			"Draw fps meter (1 = fps, 2 = smooth)"
		);

		//---------------------------------------------------------------------
		// Time
		//---------------------------------------------------------------------
		static ConVar host_timescale(
			"host_timescale", 1.0f, FCVAR::NONE,
			"Prescale the clock by this amount."
		);

		//---------------------------------------------------------------------
		// Mouse
		//---------------------------------------------------------------------
		static ConVar sensitivity(
			"sensitivity", 1.0f, FCVAR::ARCHIVE,
			"Mouse sensitivity."
		);

		static ConVar m_pitch(
			"m_pitch", 0.022f, FCVAR::ARCHIVE,
			"Mouse pitch factor."
		);
		static ConVar m_yaw(
			"m_yaw", 0.022f, FCVAR::ARCHIVE,
			"Mouse yaw factor."
		);

		static ConVar joy_sensitivity(
			"joy_sensitivity", 360.0f, FCVAR::ARCHIVE,
			"Gamepad right-stick look sensitivity (deg/sec).",
			true,
			0.0f,
			true,
			1080.0f
		);

		//---------------------------------------------------------------------
		// Camera 
		//---------------------------------------------------------------------

		static ConVar cl_pitchdown(
			"cl_pitchdown", 89.0f, FCVAR::CHEAT,
			"Maximum downward pitch angle."
		);
		static ConVar cl_pitchup(
			"cl_pitchup", 89.0f, FCVAR::CHEAT,
			"Maximum upward pitch angle."
		);

		static ConVar cl_fov(
			"cl_fov", 90.0f, FCVAR::ARCHIVE,
			"Player Camera field of view."
		);
		static ConVar viewmodel_fov(
			"viewmodel_fov", 54.0f, FCVAR::CHEAT,
			"Viewmodel field of view."
		);

		static ConVar cl_maxfov(
			"cl_maxfov", 179.9f, FCVAR::CHEAT,
			"Maximum allowed field of view."
		);
		static ConVar cl_minfov(
			"cl_minfov", 0.1f, FCVAR::CHEAT,
			"Minimum allowed field of view."
		);

		//---------------------------------------------------------------------
		// Map 
		//---------------------------------------------------------------------
		static ConVar r_mapextents(
			"r_mapextents", 16384.0f, FCVAR::CHEAT,
			"Set the max dimension for the map."
		);

		//---------------------------------------------------------------------
		// Graphics
		//---------------------------------------------------------------------
		static ConVar fps_max(
			"fps_max", 360.0, FCVAR::ARCHIVE,
			"Frame rate limiter. 0 = unlimited."
		);

		static ConVar cl_interpolate(
			"cl_interpolate", true, FCVAR::ARCHIVE,
			"Enable Source-style render interpolation for fixed-tick simulation."
		);

		static ConVar<std::string> demo_mismatch_policy(
			"demo_mismatch_policy", "continue", FCVAR::ARCHIVE,
			"Demo mismatch handling policy: continue | stop | ignore."
		);
		static ConVar<int> demo_mismatch_log_interval(
			"demo_mismatch_log_interval", 120, FCVAR::ARCHIVE,
			"Demo mismatch logging interval in ticks after first 16 logs.",
			true,
			1,
			true,
			10000
		);

		//---------------------------------------------------------------------
		// Debug
		//---------------------------------------------------------------------
		static ConVar ent_axis(
			"ent_axis", false, FCVAR::CHEAT,
			"Draw axis for entities."
		);

		static ConVar ent_bbox(
			"ent_bbox", false, FCVAR::CHEAT,
			"Draw bounding box for entities."
		);
	}

	/// @brief サーバー側のConVarを登録します。
	/// @details このエンジンはネットワーク・マルチプレイ非対応です。ロマン。
	void ServerConVar() {
		static ConVar noclip(
			"noclip", false, FCVAR::CHEAT | FCVAR::NOTIFY,
			"Enable noclip mode."
		);

		// World
		static ConVar sv_gravity(
			"sv_gravity", 800.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"World gravity."
		);

		static ConVar sv_maxvelocity(
			"sv_maxvelocity", 3500.0f, FCVAR::REPLICATED,
			"Maximum speed any ballistically moving object is allowed to attain per axis."
		);

		static ConVar sv_tickrate(
			"sv_tickrate", 66, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"World fixed simulation tickrate.",
			true,
			1,
			true,
			1000
		);

		// Cheat
		static ConVar sv_cheats(
			"sv_cheats", false, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Allow cheats on server"
		);

		// Player movement
		static ConVar sv_accelerate(
			"sv_accelerate", 10.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			""
		);

		static ConVar sv_airaccelerate(
			"sv_airaccelerate", 10.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			""
		);

		static ConVar sv_maxspeed(
			"sv_maxspeed", 320.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			""
		);

		static ConVar sv_stopspeed(
			"sv_stopspeed", 100.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Minimum stopping speed when on ground."
		);

		static ConVar sv_friction(
			"sv_friction", 5.2f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"World friction."
		);

		static ConVar sv_airspeedcap(
			"sv_airspeedcap", 30.0f, FCVAR::CHEAT | FCVAR::REPLICATED,
			""
		);

		static ConVar sv_duckspeed(
			"sv_duckspeed", 63.3f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Duck speed in HU/s."
		);
		static ConVar sv_walkspeed(
			"sv_walkspeed", 150.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Walk speed in HU/s."
		);
		static ConVar sv_sprintspeed(
			"sv_sprintspeed", 320.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Sprint speed in HU/s."
		);

		static ConVar sv_jumpvelocity(
			"sv_jumpvelocity", 400.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Jump velocity in HU/s."
		);
		static ConVar sv_jumpsnapdisabletime(
			"sv_jumpsnapdisabletime", 0.10f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Disable ground snap duration immediately after jump (seconds)."
		);

		static ConVar sv_stepheight(
			"sv_stepheight", 18.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Step height in HU."
		);

		static ConVar sv_groundprobe_distance_hu(
			"sv_groundprobe_distance_hu", 1.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Ground probe distance in HU."
		);

		static ConVar sv_move_collision_debugdraw(
			"sv_move_collision_debugdraw", false,
			FCVAR::CHEAT | FCVAR::REPLICATED,
			"Draw GameMovement collision hit/recover debug overlays."
		);
		static ConVar sv_move_collision_debuglog(
			"sv_move_collision_debuglog", false,
			FCVAR::CHEAT | FCVAR::REPLICATED,
			"Print GameMovement collision target/debug details to console."
		);
		static ConVar sv_move_collision_debuglog_interval(
			"sv_move_collision_debuglog_interval", 0.15f,
			FCVAR::CHEAT | FCVAR::REPLICATED,
			"Minimum interval in seconds between collision debug logs.",
			true,
			0.01f,
			true,
			5.0f
		);

		// Noclip
		static ConVar sv_noclipaccelerate(
			"sv_noclipaccelerate", 5.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			""
		);

		static ConVar sv_noclipspeed(
			"sv_noclipspeed", 5.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			""
		);

		// Parkour
		static ConVar park_duckaccelerate(
			"park_duckaccelerate", 4.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Parkour duck accelerate."
		);

		static ConVar park_duckfriction(
			"park_duckfriction", 1.0f, FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Parkour duck friction."
		);

		static ConVar park_duck_heightscale(
			"park_duck_heightscale", 0.5f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Parkour duck collision half-height scale."
		);
		static ConVar park_duck_debugdraw(
			"park_duck_debugdraw", true,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Draw parkour duck/unduck hull and cast debug overlays."
		);

		static ConVar park_doublejump_velocity(
			"park_doublejump_velocity", 300.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Parkour double jump velocity in HU/s."
		);

		static ConVar park_footstep_minspeed(
			"park_footstep_minspeed", 80.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Minimum speed in HU/s required to emit movement.footstep cue."
		);

		static ConVar park_footstep_stride(
			"park_footstep_stride", 100.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Stride distance in HU used for movement.footstep cue cadence."
		);

		static ConVar park_footstep_maxspeed(
			"park_footstep_maxspeed", 600.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Maximum speed in HU/s considered for movement.footstep cue cadence."
		);

		static ConVar park_slide_minspeed(
			"park_slide_minspeed", 200.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Minimum speed in HU/s required to start slide."
		);
		static ConVar park_slide_maxtime(
			"park_slide_maxtime", 20.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Maximum slide duration in seconds."
		);
		static ConVar park_slide_boostspeed(
			"park_slide_boostspeed", 50.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Additional speed in HU/s when slide starts."
		);
		static ConVar park_slide_stopspeed(
			"park_slide_stopspeed", 50.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Slide ends when horizontal speed falls below this HU/s."
		);
		static ConVar park_slide_hopspeedcap(
			"park_slide_hopspeedcap", 25000.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Maximum slide speed cap in HU/s."
		);
		static ConVar park_slide_gravityscale(
			"park_slide_gravityscale", 1.75f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Slope gravity scale while sliding."
		);

		static ConVar park_wallrun_minspeed(
			"park_wallrun_minspeed", 200.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Minimum horizontal speed in HU/s required to wallrun."
		);
		static ConVar park_wallrun_maxtime(
			"park_wallrun_maxtime", 2.5f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Maximum wallrun duration in seconds."
		);
		static ConVar park_wallrun_gravityscale(
			"park_wallrun_gravityscale", 0.1f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Wallrun gravity multiplier."
		);
		static ConVar park_wallrun_jumpforce(
			"park_wallrun_jumpforce", 350.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Wallrun jump force in HU/s."
		);
		static ConVar park_wallrun_cooldown(
			"park_wallrun_cooldown", 0.1f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Wallrun cooldown in seconds."
		);
		static ConVar park_wallrun_samewallcooldown(
			"park_wallrun_samewallcooldown", 1.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Cooldown before the same wall can be used again."
		);
		static ConVar park_wallrun_detachonsideinput(
			"park_wallrun_detachonsideinput", true,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Detach wallrun when side input pushes away from wall."
		);
		static ConVar park_wallrun_verticaldamping(
			"park_wallrun_verticaldamping", 0.3f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Vertical velocity damping on wallrun start."
		);
		static ConVar park_wallrun_maxnormaldot(
			"park_wallrun_maxnormaldot", 0.5f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Maximum allowed abs(dot(velocityDir, wallNormal))."
		);
		static ConVar park_wallrun_startboost(
			"park_wallrun_startboost", 50.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Speed boost in HU/s applied when wallrun starts."
		);
		static ConVar park_wallrun_pullforce(
			"park_wallrun_pullforce", 80.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Force toward the wall while wallrunning in HU/s^2."
		);
		static ConVar park_wallrun_checkdistance(
			"park_wallrun_checkdistance", 10.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Extra side cast distance in HU when searching wallrun walls."
		);
		static ConVar park_wallrun_maintaindistance(
			"park_wallrun_maintaindistance", 20.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Extra cast distance in HU for wallrun wall maintenance."
		);

		static ConVar park_vault_maxheight(
			"park_vault_maxheight", 72.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Maximum vaultable wall height in HU."
		);
		static ConVar park_vault_minspeed(
			"park_vault_minspeed", 32.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Minimum speed in HU/s to start speed vault."
		);
		static ConVar park_vault_forwardcheck(
			"park_vault_forwardcheck", 32.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Forward wall check distance in HU for speed vault."
		);
		static ConVar park_vault_landingcheck(
			"park_vault_landingcheck", 72.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Additional landing check distance in HU for speed vault."
		);
		static ConVar park_vault_debuglog(
			"park_vault_debuglog", false,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Enable detailed reject logs for speed vault trajectory checks."
		);
		static ConVar park_vault_routeclearance(
			"park_vault_routeclearance", 2.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Extra clearance in HU added above ledge top when validating vault route."
		);
		static ConVar park_vault_routecaststep(
			"park_vault_routecaststep", 12.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Max per-step cast length in HU when validating segmented vault route."
		);
		static ConVar park_vault_duration(
			"park_vault_duration", 0.25f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Speed vault duration in seconds."
		);
		static ConVar park_vault_cooldown(
			"park_vault_cooldown", 0.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Speed vault cooldown in seconds."
		);

		static ConVar park_blink_distance(
			"park_blink_distance", 512.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Blink distance in HU."
		);
		static ConVar park_blink_cooldown(
			"park_blink_cooldown", 1.0f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Blink cooldown in seconds."
		);
		static ConVar park_blink_moveduration(
			"park_blink_moveduration", 0.1f,
			FCVAR::NOTIFY | FCVAR::REPLICATED,
			"Blink interpolation duration in seconds."
		);
	}
}
