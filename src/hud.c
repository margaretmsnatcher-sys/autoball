/*
 * hud.c  -  Heads-up display: score, timer, boost bar.
 */

#include "hud.h"
#include "autoball_constants.h"
#include <stdio.h>
#include <string.h>

int hud_init(HUD *hud, Renderer *r)
{
    (void)r;
    memset(hud, 0, sizeof(*hud));
    hud->show_debug = 0;
    return 1;
}

void hud_shutdown(HUD *hud)
{
    (void)hud;
}

void hud_draw(HUD *hud, Renderer *r, const MatchState *match)
{
    (void)hud; (void)r;

    static float print_timer = 0.0f;
    print_timer += FIXED_DT;
    if (print_timer >= 1.0f) {
        print_timer = 0.0f;
        int mins = (int)(match->time_remaining / 60.0f);
        int secs = (int)(match->time_remaining) % 60;
        float boost = (match->human_car_id >= 0)
                      ? match->cars[match->human_car_id].boost_amount
                      : 0.0f;
        printf("  [HUD] Blue %d - %d Orange  |  %02d:%02d  |  Boost: %.0f%%\n",
               match->score[TEAM_BLUE],
               match->score[TEAM_ORANGE],
               mins, secs, boost);
    }
}
