#ifndef MATCH_H
#define MATCH_H

#include "autoball_constants.h"
#include "car.h"
#include "ball.h"
#include "arena.h"
#include "bot.h"
#include <stdbool.h>

typedef enum {
    MATCH_PHASE_PREGAME   = 0,
    MATCH_PHASE_KICKOFF   = 1,
    MATCH_PHASE_PLAY      = 2,
    MATCH_PHASE_GOAL      = 3,
    MATCH_PHASE_OVERTIME  = 4,
    MATCH_PHASE_POSTGAME  = 5
} MatchPhase;

typedef struct {
    MatchPhase phase;
    float      time_remaining;
    float      phase_timer;
    int        score[TEAM_COUNT];
    int        last_scorer;
    int        last_assister;
    Car        cars[MAX_PLAYERS];
    BotState   bots[MAX_PLAYERS];
    Ball       ball;
    Arena      arena;
    int        human_car_id;
    int        overtime;
    int        kickoff_team;
    float      role_timer;        /* bot role rebalance timer */
} MatchState;

void match_init(MatchState *match, int human_car_id, float bot_skill);
void match_update(MatchState *match, float dt);
void match_do_kickoff(MatchState *match);
void match_on_goal(MatchState *match, GoalResult goal);
int  match_is_over(const MatchState *match);
Car *match_get_car(MatchState *match, int id);

#endif /* MATCH_H */
