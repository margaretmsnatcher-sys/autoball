#ifndef AUTOBALL_CONSTANTS_H
#define AUTOBALL_CONSTANTS_H

/* ── Version ─────────────────────────────────────────────────────────────── */
#define AUTOBALL_VERSION_MAJOR 0
#define AUTOBALL_VERSION_MINOR 1
#define AUTOBALL_VERSION_PATCH 0

/* ── Window / render constants ───────────────────────────────────────────── */
#define WINDOW_TITLE   "Autoball"
#define WINDOW_WIDTH   1280
#define WINDOW_HEIGHT  720
#define TARGET_FPS     60
#define FIXED_DT       (1.0f / TARGET_FPS)

/* ── Arena dimensions (world units) ─────────────────────────────────────── */
#define ARENA_HALF_LEN   52.5f
#define ARENA_HALF_WID   34.0f
#define ARENA_WALL_H      8.0f
#define ARENA_CEIL_H     10.0f

/* ── Ball ────────────────────────────────────────────────────────────────── */
#define BALL_RADIUS       1.0f
#define BALL_MASS         1.0f
#define BALL_RESTITUTION  0.6f

/* ── Car ─────────────────────────────────────────────────────────────────── */
#define CAR_HALF_LEN      2.0f
#define CAR_HALF_WID      1.0f
#define CAR_HALF_HGT      0.5f
#define CAR_MASS          5.0f
#define CAR_MAX_SPEED    23.0f
#define CAR_BOOST_SPEED  33.0f
#define CAR_TURN_SPEED    2.8f
#define CAR_JUMP_IMPULSE  8.0f
#define CAR_BOOST_ACCEL  30.0f
#define CAR_GRAVITY      20.0f

/* ── Teams ───────────────────────────────────────────────────────────────── */
#define TEAM_SIZE         3
#define TEAM_COUNT        2
#define MAX_PLAYERS      (TEAM_SIZE * TEAM_COUNT)

/* ── Match ───────────────────────────────────────────────────────────────── */
#define MATCH_DURATION   300.0f
#define OVERTIME_ENABLED  1

/* ── Physics ─────────────────────────────────────────────────────────────── */
#define GRAVITY          -9.81f
#define PHYSICS_SUBSTEPS  4

/* ── Bot AI ──────────────────────────────────────────────────────────────── */
#define BOT_REACTION_TIME 0.1f
#define BOT_SKILL_EASY    0.3f
#define BOT_SKILL_MEDIUM  0.6f
#define BOT_SKILL_HARD    0.9f

/* ── Future: Twisted Metal weapon slots ─────────────────────────────────── */
#define WEAPON_SLOTS      2

#endif /* AUTOBALL_CONSTANTS_H */
