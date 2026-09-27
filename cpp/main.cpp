// Capitol Defense: raylib port of index.html.
// Build: see cpp/README.md. Run with --sim for a headless balance playtest.
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

constexpr int W = 640, H = 400, T = 40, COLS = 16, ROWS = 10;
constexpr int VW = 640, VH = 560, OY = 40;  // virtual screen; field starts at OY

// ---- Colors ----
static Color hexc(unsigned v, unsigned char a = 255) { return {(unsigned char)(v >> 16), (unsigned char)((v >> 8) & 255), (unsigned char)(v & 255), a}; }
static const Color INK = hexc(0xeef1f6), BG = hexc(0x0e1733), PANEL = hexc(0x16224a), LINE = hexc(0x2b3c75),
                   RED_ = hexc(0xd7263d), BLUE_ = hexc(0x4b7be5), GOLD_ = hexc(0xf2c14e), MUTED = hexc(0x9aa6c8),
                   GOOD = hexc(0x5bd16a), BAD = hexc(0xff5a6e);

// ---- Drawing helpers (gA = global alpha, used for the build ghost) ----
static float gA = 1.f;
static Color Ac(Color c) { c.a = (unsigned char)(c.a * gA); return c; }
static void tri(float ax, float ay, float bx, float by, float cx, float cy, Color col) {
    if ((bx - ax) * (cy - ay) - (by - ay) * (cx - ax) > 0) { std::swap(bx, cx); std::swap(by, cy); }
    DrawTriangle({ax, ay}, {bx, by}, {cx, cy}, Ac(col));
}
static void rect(float x, float y, float w, float h, Color c) { DrawRectangleRec({x, y, w, h}, Ac(c)); }
static void circ(float x, float y, float r, Color c) { DrawCircleV({x, y}, r, Ac(c)); }
static void line(float x1, float y1, float x2, float y2, float th, Color c) { DrawLineEx({x1, y1}, {x2, y2}, th, Ac(c)); }
static void textC(const char* s, float x, float y, int size, Color c) { DrawText(s, (int)(x - MeasureText(s, size) / 2.f), (int)y, size, Ac(c)); }
static float rnd() { return GetRandomValue(0, 10000) / 10000.f; }

// ---- Map: Pennsylvania Ave winding to the Capitol ----
struct V2 { float x, y; };
struct Seg { V2 a, b; float l, s; };
static const int WP[][2] = {{-1, 1}, {3, 1}, {3, 6}, {7, 6}, {7, 2}, {11, 2}, {11, 7}, {15, 7}};
static std::vector<V2> PTS;
static std::vector<Seg> SEG;
static float PLEN = 0;
static bool blocked[COLS][ROWS];

static void initMap() {
    for (auto& p : WP) PTS.push_back({p[0] * (float)T + T / 2.f, p[1] * (float)T + T / 2.f});
    for (size_t i = 1; i < PTS.size(); i++) {
        V2 a = PTS[i - 1], b = PTS[i];
        float l = std::hypot(b.x - a.x, b.y - a.y);
        SEG.push_back({a, b, l, PLEN});
        PLEN += l;
    }
    const int cap[][2] = {{14, 6}, {15, 6}, {14, 8}, {15, 8}, {15, 5}, {15, 9}};
    for (auto& c : cap) blocked[c[0]][c[1]] = true;
    for (size_t i = 1; i < std::size(WP); i++) {
        int c = WP[i - 1][0], r = WP[i - 1][1], c1 = WP[i][0], r1 = WP[i][1];
        int dc = (c1 > c) - (c1 < c), dr = (r1 > r) - (r1 < r);
        for (;;) {
            if (c >= 0 && c < COLS && r >= 0 && r < ROWS) blocked[c][r] = true;
            if (c == c1 && r == r1) break;
            c += dc; r += dr;
        }
    }
}
static V2 posAt(float d) {
    for (auto& s : SEG)
        if (d <= s.s + s.l) { float k = (d - s.s) / s.l; return {s.a.x + (s.b.x - s.a.x) * k, s.a.y + (s.b.y - s.a.y) * k}; }
    return PTS.back();
}

// ---- Defenses ----
enum TK { FACT, FILI, AD, COURT, NT };
struct TowerDef { const char* name; int cost; float range, rate, dmg, slow, splash; Color col; const char* tip; };
static const TowerDef TW[NT] = {
    {"Fact-Checker", 50, 95, .5f, 9, 0, 0, BLUE_, "Rapid shots of cold, hard truth."},
    {"Filibuster", 70, 78, 1.1f, 3, .5f, 0, hexc(0x2bb3a3), "Talks for hours. Slows everyone nearby."},
    {"Attack Ad", 100, 110, 1.5f, 26, 0, 50, RED_, "Splash damage. Nobody is safe."},
    {"Supreme Court", 160, 170, 2.4f, 120, 0, 0, GOLD_, "Slow, long-range rulings on the toughest target."}};
static const char* QUIP[NT] = {"DEBUNKED!", "TABLED!", "CANCELLED!", "OVERRULED!"};

// ---- Mobs ----
enum EK { ROBO, LOBBY, TROLL, PAC, SCANDAL, SHUT, NE };
struct EnemyDef { const char* name; const char* plural; float hp, spd; int gold, hit; float r, gap; };
static const EnemyDef EN[NE] = {
    {"Robocall", "Robocalls", 20, 85, 5, 4, 10, .7f},       {"Lobbyist", "Lobbyists", 48, 50, 8, 6, 12, .9f},
    {"Troll Bot", "Troll Bots", 12, 70, 3, 2, 8, .3f},      {"Super PAC", "Super PACs", 230, 32, 22, 12, 15, 1.6f},
    {"Scandal", "Scandals", 800, 26, 90, 30, 20, 2.5f},     {"Gov. Shutdown", "Gov. Shutdowns", 3200, 19, 0, 100, 24, 3}};
struct Group { EK k; int n; };
static const std::vector<std::vector<Group>> WAVES = {
    {{ROBO, 8}}, {{ROBO, 10}, {LOBBY, 4}}, {{TROLL, 18}}, {{LOBBY, 8}, {ROBO, 10}}, {{SCANDAL, 1}, {TROLL, 12}},
    {{PAC, 3}, {LOBBY, 8}}, {{ROBO, 24}}, {{TROLL, 26}, {PAC, 3}}, {{LOBBY, 14}, {PAC, 4}}, {{SCANDAL, 2}, {ROBO, 18}},
    {{PAC, 9}}, {{TROLL, 45}}, {{LOBBY, 20}, {PAC, 7}}, {{SCANDAL, 3}, {PAC, 6}}, {{SHUT, 1}, {TROLL, 20}, {LOBBY, 12}}};
static const int NWAVES = (int)WAVES.size();
static const char* NEWS[] = {
    "Poll finds 93% of voters are tired of polls", "Senate agrees to disagree, schedules more disagreeing",
    "Attack ad attacks other attack ad", "Filibuster enters hour 19, reading the phone book",
    "Breaking: robocall calls another robocall, both on hold", "Lobbyist found lobbying for more lobbying",
    "Troll bot accuses other troll bot of being a bot", "Super PAC forms Super-Duper PAC",
    "Fact-checkers rate this headline \"Mostly True\"", "Budget deadline moved to day after the deadline",
    "Committee forms subcommittee to study committees", "Local man still waiting on hold with his representative"};

// ---- State ----
struct Tower { TK k; int c, r; float x, y; int lvl; float cd; int spent; float ang; };
struct Enemy { int id; EK k; float hp, max, d, slowT, slowF, x, y, wob; bool dead; };
struct Shot { TK k; float x, y; int tg; float tx, ty, dmg, spd, splash; bool done; };
enum FxT { F_TXT, F_RING, F_BLAST, F_BOOM, F_GAVEL };
struct Fx { FxT type; float x, y, t, life, r, fx, fy; std::string s; Color c; bool big; };
enum Over { NONE, WIN, LOSE };

struct Game {
    int money = 120, wave = 0, nextId = 1, sel = -1, build = -1;
    float appr = 100, spawnT = 0;
    bool live = false, started = false, paused = false;
    int speed = 1;
    Over over = NONE;
    std::vector<Tower> towers;
    std::vector<Enemy> en;
    std::vector<Shot> shots;
    std::vector<Fx> fx;
    std::vector<EK> queue;
} S;

struct Stats { float dmg, range, rate, slow; };
static Stats stats(const Tower& t) {
    const TowerDef& d = TW[t.k];
    float l = t.lvl - 1;
    return {d.dmg * (1 + .6f * l), d.range * (1 + .12f * l), d.rate * (1 - .12f * l), d.slow - l * .08f};
}
static int upCost(const Tower& t) { return (int)std::lround(TW[t.k].cost * .8f * t.lvl); }
static int sellVal(const Tower& t) { return (int)std::lround(t.spent * .6f); }
static int towerAt(int c, int r) {
    for (size_t i = 0; i < S.towers.size(); i++) if (S.towers[i].c == c && S.towers[i].r == r) return (int)i;
    return -1;
}
static bool canPlace(int c, int r) { return c >= 0 && r >= 0 && c < COLS && r < ROWS && !blocked[c][r] && towerAt(c, r) < 0; }
static void addFx(Fx f) { S.fx.push_back(std::move(f)); }
static Fx txtFx(float x, float y, std::string s, Color c, float life = .8f, bool big = false) { return {F_TXT, x, y, 0, life, 0, 0, 0, std::move(s), c, big}; }

static void startWave() {
    if (S.live || S.over || S.wave >= NWAVES) return;
    S.queue.clear();
    for (auto& g : WAVES[S.wave]) for (int i = 0; i < g.n; i++) S.queue.push_back(g.k);
    S.live = true; S.spawnT = 0;
}
static void spawn(EK k) {
    float m = 1 + S.wave * .22f, hp = EN[k].hp * m;
    S.en.push_back({S.nextId++, k, hp, hp, -10, 0, 1, -20, PTS[0].y, rnd() * 6, false});
}
static void hurt(Enemy& e, float dmg, TK src) {
    if (e.dead) return;
    e.hp -= dmg;
    if (e.hp > 0) return;
    e.dead = true;
    int g = EN[e.k].gold;
    S.money += g;
    if (g) addFx(txtFx(e.x, e.y - 10, "+$" + std::to_string(g), GOLD_));
    if (EN[e.k].r >= 15 || rnd() < .15f) addFx(txtFx(e.x, e.y - 24, QUIP[src], WHITE, 1.1f, true));
    addFx({F_BOOM, e.x, e.y, 0, .35f, EN[e.k].r});
}
static void endGame(bool win) { if (S.over == NONE) S.over = win ? WIN : LOSE; }

static void update(float dt) {
    if (!S.queue.empty()) {
        S.spawnT -= dt;
        if (S.spawnT <= 0) { EK k = S.queue.front(); S.queue.erase(S.queue.begin()); spawn(k); S.spawnT = EN[k].gap; }
    }
    for (auto& e : S.en) {
        if (e.slowT > 0) { e.slowT -= dt; if (e.slowT <= 0) e.slowF = 1; }
        e.d += EN[e.k].spd * e.slowF * dt;
        V2 p = posAt(e.d); e.x = p.x; e.y = p.y;
        if (e.d >= PLEN && !e.dead) {
            e.dead = true; S.appr -= EN[e.k].hit;
            addFx(txtFx(600, 230, "-" + std::to_string(EN[e.k].hit) + "%", BAD, 1));
        }
    }
    for (auto& t : S.towers) {
        Stats s = stats(t);
        t.cd -= dt;
        if (t.cd > 0) continue;
        if (t.k == FILI) {
            bool any = false;
            for (auto& e : S.en) {
                if (e.dead || std::hypot(e.x - t.x, e.y - t.y) > s.range) continue;
                any = true; e.slowT = 1.4f; e.slowF = std::min(e.slowF, s.slow); hurt(e, s.dmg, FILI);
            }
            if (any) { t.cd = s.rate; addFx({F_RING, t.x, t.y, 0, .6f, s.range}); }
            continue;
        }
        Enemy* best = nullptr;
        for (auto& e : S.en) {
            if (e.dead || std::hypot(e.x - t.x, e.y - t.y) > s.range) continue;
            if (!best || (t.k == COURT ? e.hp > best->hp : e.d > best->d)) best = &e;
        }
        if (!best) continue;
        t.cd = s.rate; t.ang = std::atan2(best->y - t.y, best->x - t.x);
        if (t.k == COURT) { hurt(*best, s.dmg, COURT); addFx({F_GAVEL, best->x, best->y, 0, .4f, 0, t.x, t.y}); }
        else S.shots.push_back({t.k, t.x, t.y, best->id, best->x, best->y, s.dmg, t.k == FACT ? 420.f : 260.f, TW[t.k].splash, false});
    }
    for (auto& b : S.shots) {
        Enemy* tg = nullptr;
        for (auto& e : S.en) if (e.id == b.tg && !e.dead) { tg = &e; break; }
        if (tg) { b.tx = tg->x; b.ty = tg->y; }
        float dx = b.tx - b.x, dy = b.ty - b.y, d = std::hypot(dx, dy), st = b.spd * dt;
        if (d <= st) {
            b.done = true;
            if (b.splash > 0) {
                addFx({F_BLAST, b.tx, b.ty, 0, .35f, b.splash});
                for (auto& e : S.en) if (!e.dead && std::hypot(e.x - b.tx, e.y - b.ty) <= b.splash) hurt(e, b.dmg, AD);
            } else if (tg) hurt(*tg, b.dmg, FACT);
        } else { b.x += dx / d * st; b.y += dy / d * st; }
    }
    std::erase_if(S.shots, [](const Shot& b) { return b.done; });
    std::erase_if(S.en, [](const Enemy& e) { return e.dead; });
    for (auto& f : S.fx) f.t += dt;
    std::erase_if(S.fx, [](const Fx& f) { return f.t >= f.life; });
    if (S.appr <= 0) { S.appr = 0; endGame(false); }
    else if (S.live && S.queue.empty() && S.en.empty()) {
        S.live = false;
        int bonus = 20 + S.wave * 4;
        S.wave++; S.money += bonus;
        addFx(txtFx(W / 2.f, H / 2.f, "Wave cleared! +$" + std::to_string(bonus), GOLD_, 1.6f, true));
        if (S.wave >= NWAVES) endGame(true);
    }
}

// ---- Actions shared by mouse, keyboard and gamepad ----
static void pick(int k) { S.sel = -1; S.build = S.build == k ? -1 : k; }
static void cellAction(int c, int r, bool keepBuilding) {
    if (S.build >= 0) {
        const TowerDef& d = TW[S.build];
        if (canPlace(c, r) && S.money >= d.cost) {
            S.money -= d.cost;
            S.towers.push_back({(TK)S.build, c, r, c * (float)T + 20, r * (float)T + 20, 1, 0, d.cost, 0});
            if (!keepBuilding || S.money < d.cost) S.build = -1;
        }
    } else S.sel = towerAt(c, r);
}
static void upgradeSel() {
    if (S.sel < 0) return;
    Tower& t = S.towers[S.sel];
    int c = upCost(t);
    if (t.lvl < 3 && S.money >= c) { S.money -= c; t.spent += c; t.lvl++; }
}
static void sellSel() {
    if (S.sel < 0) return;
    S.money += sellVal(S.towers[S.sel]);
    S.towers.erase(S.towers.begin() + S.sel);
    S.sel = -1;
}

// ---- Drawing ----
static void drawTower(int k, float x, float y, int lvl, float ang) {
    Color c = TW[k].col;
    rlPushMatrix();
    rlTranslatef(x, y, 0);
    DrawRectangleRounded({-17, -17, 34, 34}, .35f, 6, Ac(hexc(0x1a2447)));
    DrawRectangleRoundedLinesEx({-16, -16, 32, 32}, .35f, 6, 3, Ac(c));
    if (k == FACT) {
        rlPushMatrix(); rlRotatef((ang + PI / 4) * RAD2DEG, 0, 0, 1);
        line(6, 6, 12, 12, 4, hexc(0xdbe4ff));
        rlPopMatrix();
        circ(-2, -2, 9, hexc(0xbcd3ff));
        DrawRing({-2, -2}, 7.8f, 10.2f, 0, 360, 24, Ac(WHITE));
        line(-7, -2, -3, 2, 3, hexc(0x1d8f3a)); line(-3, 2, 3, -7, 3, hexc(0x1d8f3a));
    } else if (k == FILI) {
        tri(-10, 12, 10, 12, 7, -4, hexc(0x8a5a2b)); tri(-10, 12, 7, -4, -7, -4, hexc(0x8a5a2b));
        rect(-11, -8, 22, 5, hexc(0xb7793c)); circ(0, 4, 3, WHITE);
        line(0, -8, 3, -13, 2, hexc(0xcccccc)); circ(3, -14, 2.5f, hexc(0x333333));
    } else if (k == AD) {
        line(-5, -9, -9, -15, 1.5f, hexc(0xdddddd)); line(5, -9, 9, -15, 1.5f, hexc(0xdddddd));
        DrawRectangleRounded({-13, -9, 26, 20}, .25f, 4, Ac(hexc(0x222222)));
        rect(-10, -6, 10, 14, RED_); rect(0, -6, 10, 14, BLUE_);
        textC("VS", 0, -4, 10, WHITE);
    } else {
        Color m = hexc(0xf4f1e8);
        tri(-14, -5, 0, -14, 14, -5, m); rect(-14, 9, 28, 4, m);
        for (int i = 0; i < 4; i++) rect(-12 + i * 7, -4, 4, 13, m);
        rlPushMatrix(); rlTranslatef(8, -12, 0); rlRotatef(-34, 0, 0, 1);
        rect(-1, 0, 2, 8, hexc(0x6b3f1f)); rect(-4, -2, 8, 4, hexc(0x6b3f1f));
        rlPopMatrix();
    }
    for (int i = 1; i < lvl; i++) circ(-12 + i * 6, 19, 2.2f, GOLD_);
    rlPopMatrix();
}

static void drawEnemy(const Enemy& e, float now) {
    float r = EN[e.k].r, bob = std::sin(now * 8 + e.wob) * 1.5f;
    rlPushMatrix();
    rlTranslatef(e.x, e.y + bob, 0);
    switch (e.k) {
    case ROBO:
        DrawRectangleRounded({-7, -10, 14, 20}, .3f, 4, hexc(0xb3261e));
        rect(-5, -7, 10, 7, hexc(0xffd7d2));
        if (std::sin(now * 14 + e.wob) > 0) {
            DrawRing({0, 0}, 12.2f, 13.8f, -57, -17, 8, hexc(0xffd7d2));
            DrawRing({0, 0}, 12.2f, 13.8f, 197, 237, 8, hexc(0xffd7d2));
        }
        break;
    case LOBBY: {
        rect(-15, 2, 8, 7, hexc(0x6b4a2b)); circ(0, 0, 11, hexc(0x3a3f4f));
        tri(-4, -10, 4, -10, 0, -2, WHITE);
        Color tie = RED_;
        tri(-1.5f, -9, 1.5f, -9, 2, 2, tie); tri(-1.5f, -9, 2, 2, -2, 2, tie); tri(-2, 2, 2, 2, 0, 5, tie);
        circ(0, -12, 5, hexc(0xe9c9a6));
        break;
    }
    case TROLL:
        line(0, -8, 0, -13, 1.5f, hexc(0x9be15d)); rect(-8, -8, 16, 16, hexc(0x4caf50));
        rect(-5, -4, 4, 4, WHITE); rect(1, -4, 4, 4, WHITE); rect(-4, 3, 8, 2, hexc(0x111111));
        break;
    case PAC:
        circ(0, 3, 14, hexc(0xcaa233)); rect(-5, -14, 10, 6, hexc(0xcaa233));
        line(-6, -8, 6, -8, 2, hexc(0x7a5d10)); textC("$", 0, -6, 20, hexc(0xfff6d0));
        break;
    case SCANDAL:
        rlRotatef(std::sin(now * 3) * 6, 0, 0, 1);
        rect(-26, -16, 52, 32, hexc(0xf4f1e8)); textC("SCANDAL", 0, -13, 10, hexc(0xb3261e));
        for (int i = 0; i < 3; i++) rect(-22, 1 + i * 5, 44, 2, hexc(0x777777));
        break;
    default:
        DrawPoly({0, 0}, 8, 27, 22.5f, hexc(0xb3261e));
        DrawPolyLinesEx({0, 0}, 8, 27, 22.5f, 2, WHITE);
        textC("GOV'T", 0, -11, 10, WHITE); textC("SHUTDOWN", 0, 1, 10, WHITE);
    }
    if (e.slowT > 0) DrawText("zzz", (int)r, (int)(-r - 8), 10, hexc(0x88ffff));
    rlPopMatrix();
    float w = std::max(18.f, r * 2), k = std::max(0.f, e.hp / e.max);
    rect(e.x - w / 2, e.y - r - 9, w, 4, BLACK);
    rect(e.x - w / 2, e.y - r - 9, w * k, 4, k > .4f ? GOOD : BAD);
}

static void drawMap() {
    for (int c = 0; c < COLS; c++) rect(c * (float)T, 0, T, H, c % 2 ? hexc(0x3b883b) : hexc(0x43963f));
    for (float w : {34.f, 30.f}) {
        Color col = w > 32 ? hexc(0x2d2f36) : hexc(0x4a4d57);
        for (auto& s : SEG) line(s.a.x, s.a.y, s.b.x, s.b.y, w, col);
        for (auto& p : PTS) circ(p.x, p.y, w / 2, col);
    }
    for (auto& s : SEG)
        for (float d = 0; d < s.l; d += 18) {
            float e = std::min(d + 8, s.l), ux = (s.b.x - s.a.x) / s.l, uy = (s.b.y - s.a.y) / s.l;
            line(s.a.x + ux * d, s.a.y + uy * d, s.a.x + ux * e, s.a.y + uy * e, 2, GOLD_);
        }
    DrawText("PENNSYLVANIA AVE", 8, 13, 10, Fade(WHITE, .75f));
    // The Capitol
    rlPushMatrix(); rlTranslatef(612, 288, 0);
    Color m = hexc(0xf4f1e8), sh = hexc(0xd9d4c5);
    rect(-30, -4, 58, 26, m);
    for (int i = 0; i < 6; i++) rect(-26 + i * 9, 0, 4, 18, sh);
    rect(-16, -16, 30, 12, m);
    DrawCircleSector({-1, -16}, 15, 180, 360, 24, m);
    rect(-3, -38, 4, 8, m); rect(-16, -12, 30, 2, sh);
    rlPopMatrix();
    rect(630, 228, 2, 20, WHITE); rect(610, 228, 20, 10, RED_); rect(610, 228, 8, 5, BLUE_);
}

struct Cell { int c, r; bool on; };

static void drawField(float now, Cell hover, bool cursorOn) {
    drawMap();
    if (S.sel >= 0) {
        Tower& t = S.towers[S.sel];
        float rg = stats(t).range;
        DrawCircleV({t.x, t.y}, rg, Fade(WHITE, .1f));
        DrawCircleLinesV({t.x, t.y}, rg, Fade(WHITE, .5f));
    }
    for (auto& t : S.towers) drawTower(t.k, t.x, t.y, t.lvl, t.ang);
    for (auto& e : S.en) drawEnemy(e, now);
    for (auto& b : S.shots) {
        rlPushMatrix(); rlTranslatef(b.x, b.y, 0);
        if (b.k == FACT) { line(-4, 0, -1, 3, 3, hexc(0x8fffa0)); line(-1, 3, 5, -4, 3, hexc(0x8fffa0)); }
        else { rlRotatef(now * 570, 0, 0, 1); rect(-6, -4, 12, 8, WHITE); rect(-6, -4, 6, 8, RED_); }
        rlPopMatrix();
    }
    for (auto& f : S.fx) {
        float k = f.t / f.life;
        gA = 1 - k;
        switch (f.type) {
        case F_TXT: {
            int size = f.big ? 20 : 10;
            float y = f.y - k * 20 - size / 2.f;
            for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) textC(f.s.c_str(), f.x + dx * 2, y + dy * 2, size, BG);
            textC(f.s.c_str(), f.x, y, size, f.c);
            break;
        }
        case F_RING:
            DrawRing({f.x, f.y}, std::max(0.f, f.r * k - 1.5f), f.r * k + 1.5f, 0, 360, 40, Ac(hexc(0x2bb3a3)));
            textC("blah blah", f.x, f.y - 26 - k * 10, 10, hexc(0xbbffff));
            break;
        case F_BLAST: circ(f.x, f.y, f.r * (.4f + k * .6f), hexc(0xffb14e)); break;
        case F_BOOM: DrawRing({f.x, f.y}, f.r + k * 12 - 1, f.r + k * 12 + 1, 0, 360, 24, Ac(WHITE)); break;
        case F_GAVEL: line(f.fx, f.fy, f.x, f.y, 3, GOLD_); circ(f.x, f.y, 14 * (1 - k) + 4, GOLD_); break;
        }
        gA = 1;
    }
    if (hover.on && S.build >= 0) {
        bool ok = canPlace(hover.c, hover.r) && S.money >= TW[S.build].cost;
        float x = hover.c * (float)T + 20, y = hover.r * (float)T + 20;
        DrawCircleV({x, y}, TW[S.build].range, ok ? Fade(WHITE, .12f) : Fade(hexc(0xff3c50), .18f));
        gA = .7f; drawTower(S.build, x, y, 1, 0); gA = 1;
        if (!ok) DrawRectangleLinesEx({x - 18, y - 18, 36, 36}, 3, hexc(0xff3c50));
    }
    if (cursorOn && hover.on) DrawRectangleLinesEx({hover.c * (float)T + 1, hover.r * (float)T + 1, T - 2, T - 2}, 2, WHITE);
    if (S.paused && S.over == NONE) {
        rect(0, 0, W, H, Fade(BLACK, .45f));
        textC("RECESS", W / 2.f, H / 2.f - 20, 40, WHITE);
    }
}

// ---- UI ----
static bool button(Rectangle r, const char* label, bool enabled, Color bg, bool hl = false, int size = 10) {
    bool hov = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRounded(r, .2f, 4, enabled ? bg : Fade(bg, .4f));
    DrawRectangleRoundedLinesEx({r.x + 1, r.y + 1, r.width - 2, r.height - 2}, .2f, 4, 2, hl ? GOLD_ : hov ? INK : enabled ? LINE : Fade(LINE, .4f));
    if (label) textC(label, r.x + r.width / 2, r.y + (r.height - size) / 2, size, enabled ? INK : Fade(INK, .45f));
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
static std::vector<std::string> wrap(const std::string& s, int maxW, int size) {
    std::vector<std::string> out;
    std::string cur, word;
    for (size_t i = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == ' ') {
            std::string test = cur.empty() ? word : cur + " " + word;
            if (MeasureText(test.c_str(), size) > maxW && !cur.empty()) { out.push_back(cur); cur = word; }
            else cur = test;
            word.clear();
        } else word += s[i];
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

static void drawHud() {
    rect(0, 0, VW, OY, BG);
    DrawText("CAPITOL", 10, 11, 20, INK);
    DrawText("DEFENSE", 10 + MeasureText("CAPITOL ", 20), 11, 20, RED_);
    char buf[32];
    struct { const char* label; Color c; } st[3] = {{"FUNDS", GOLD_}, {"APPROVAL", S.appr > 50 ? GOOD : S.appr > 25 ? GOLD_ : BAD}, {"WAVE", INK}};
    for (int i = 0; i < 3; i++) {
        Rectangle r = {340.f + i * 99, 3, 94, 34};
        DrawRectangleRounded(r, .2f, 4, PANEL);
        DrawText(st[i].label, (int)r.x + 8, (int)r.y + 3, 10, MUTED);
        if (i == 0) snprintf(buf, sizeof buf, "$%d", S.money);
        else if (i == 1) snprintf(buf, sizeof buf, "%d%%", (int)std::lround(std::max(0.f, S.appr)));
        else snprintf(buf, sizeof buf, "%d/%d", std::min(S.wave + (S.live ? 1 : 0), NWAVES), NWAVES);
        DrawText(buf, (int)r.x + 8, (int)r.y + 14, 20, st[i].c);
    }
}

static float tickX = VW;
static std::string tickText;

static void drawPanel(float realDt) {
    rect(0, OY + H, VW, VH - OY - H, BG);
    bool play = S.started && S.over == NONE;
    // Shop
    for (int k = 0; k < NT; k++) {
        Rectangle r = {4.f + k * 160, OY + H + 6.f, 152, 44};
        bool en = play && (S.money >= TW[k].cost || S.build == k);
        if (button(r, nullptr, en, S.build == k ? hexc(0x1f2f63) : PANEL, S.build == k)) pick(k);
        gA = en ? 1 : .45f;
        drawTower(k, r.x + 24, r.y + 22, 1, 0);
        char buf[48];
        snprintf(buf, sizeof buf, "%d  %s", k + 1, TW[k].name);
        DrawText(buf, (int)r.x + 48, (int)r.y + 8, 10, Ac(INK));
        snprintf(buf, sizeof buf, "$%d", TW[k].cost);
        DrawText(buf, (int)r.x + 48, (int)r.y + 20, 20, Ac(GOLD_));
        gA = 1;
    }
    // Info box + controls
    float y = OY + H + 56;
    Rectangle info = {4, y, 372, 36};
    DrawRectangleRounded(info, .2f, 4, PANEL);
    char buf[160];
    if (S.sel >= 0) {
        Tower& t = S.towers[S.sel];
        Stats s = stats(t);
        if (t.k == FILI) snprintf(buf, sizeof buf, "%s Lv%d  slow %d%%  range %d", TW[t.k].name, t.lvl, (int)std::lround((1 - s.slow) * 100), (int)s.range);
        else snprintf(buf, sizeof buf, "%s Lv%d  dmg %d  range %d", TW[t.k].name, t.lvl, (int)std::lround(s.dmg), (int)s.range);
        DrawText(buf, 12, (int)y + 13, 10, INK);
        char ub[24], sb[24];
        if (t.lvl < 3) snprintf(ub, sizeof ub, "UPGRADE $%d", upCost(t)); else snprintf(ub, sizeof ub, "MAX LEVEL");
        snprintf(sb, sizeof sb, "SELL $%d", sellVal(t));
        if (button({196, y + 4, 96, 28}, ub, play && t.lvl < 3 && S.money >= upCost(t), hexc(0x1f2f63))) upgradeSel();
        if (button({296, y + 4, 76, 28}, sb, play, hexc(0x1f2f63))) sellSel();
    } else {
        std::string s;
        if (S.build >= 0) s = std::string(TW[S.build].name) + ": " + TW[S.build].tip + " Click grass to build.";
        else if (S.wave < NWAVES) {
            s = S.live ? "Now: " : "Next: ";
            for (size_t i = 0; i < WAVES[S.wave].size(); i++) {
                auto& g = WAVES[S.wave][i];
                s += (i ? ", " : "") + std::to_string(g.n) + " " + (g.n == 1 ? EN[g.k].name : EN[g.k].plural);
            }
        }
        auto lines = wrap(s, 356, 10);
        for (size_t i = 0; i < lines.size() && i < 2; i++)
            DrawText(lines[i].c_str(), 12, (int)(y + (lines.size() > 1 ? 7 + i * 12 : 13)), 10, i == 0 && S.build < 0 ? INK : MUTED);
    }
    if (button({380, y, 104, 36}, "NEXT WAVE >", play && !S.live, BLUE_)) startWave();
    snprintf(buf, sizeof buf, "SPEED %dx", S.speed);
    if (button({488, y, 72, 36}, buf, play, PANEL)) S.speed = S.speed % 3 + 1;
    if (button({564, y, 72, 36}, S.paused ? "RESUME" : "PAUSE", play, PANEL)) S.paused = !S.paused;
    // News ticker
    float ty = y + 42;
    rect(4, ty, VW - 8, 18, RED_);
    int tw = MeasureText(tickText.c_str(), 10);
    tickX -= 45 * realDt;
    if (tickX < -tw) tickX = VW;
    BeginScissorMode(4, (int)ty, VW - 8, 18);  // virtual coords; the render target is VW x VH
    DrawText(tickText.c_str(), (int)tickX, (int)ty + 4, 10, WHITE);
    EndScissorMode();
}

static bool drawOverlay() {  // returns true when its button is clicked
    if (S.started && S.over == NONE) return false;
    rect(0, OY, W, H, Fade(hexc(0x0a1026), .86f));
    const char* title = S.over == WIN ? "RE-ELECTED!" : S.over == LOSE ? "VOTED OUT" : "HOLD THE CAPITOL";
    std::string body;
    char buf[256];
    if (S.over == WIN) { snprintf(buf, sizeof buf, "You held the Capitol through all 15 waves, including the Government Shutdown, with %d%% approval. The voters (mostly) approve.", (int)std::lround(S.appr)); body = buf; }
    else if (S.over == LOSE) { snprintf(buf, sizeof buf, "Your approval rating hit zero on wave %d. Time to write a memoir and start a podcast.", S.wave + 1); body = buf; }
    else body = "Robocalls, lobbyists, troll bots and Super PACs are marching down Pennsylvania Ave. Build defenses beside the road and keep your approval rating above zero for 15 waves.";
    textC(title, W / 2.f, OY + 110, 40, S.over == LOSE ? BAD : INK);
    auto lines = wrap(body, 420, 10);
    for (size_t i = 0; i < lines.size(); i++) textC(lines[i].c_str(), W / 2.f, OY + 164 + i * 16.f, 10, MUTED);
    textC("Mouse, keyboard (arrows, Enter, 1-4, Space) or gamepad", W / 2.f, OY + 316, 10, Fade(MUTED, .7f));
    return button({W / 2.f - 100, OY + 256, 200, 44}, S.over == NONE ? "START THE SESSION" : "RUN AGAIN", true, RED_, false, 10);
}

static void newGame() { S = Game(); S.started = true; }

// ---- Headless playtest: a scripted player builds and upgrades between waves ----
static int runSim() {
    initMap();
    S = Game(); S.started = true;
    const int spots[][2] = {{2, 2}, {4, 3}, {6, 5}, {8, 3}, {10, 3}, {12, 6}, {4, 5}, {8, 5}, {10, 6}, {6, 1}, {12, 3}, {2, 0}, {4, 1}, {8, 1}, {10, 1}, {12, 1}, {6, 3}, {2, 3}};
    const TK plan[] = {FACT, FACT, FILI, AD, FACT, COURT, AD, FILI, COURT, AD, COURT, FACT, AD, COURT, AD, COURT, COURT, AD};
    size_t i = 0;
    while (S.over == NONE && S.wave < NWAVES) {
        for (;;) {
            if (i < std::size(spots) && S.money >= TW[plan[i]].cost) { S.build = plan[i]; cellAction(spots[i][0], spots[i][1], false); i++; continue; }
            bool up = false;
            for (size_t j = 0; j < S.towers.size() && i >= 6; j++)
                if (S.towers[j].lvl < 3 && S.money >= upCost(S.towers[j])) { S.sel = (int)j; upgradeSel(); up = true; break; }
            if (!up) break;
        }
        startWave();
        for (int n = 0; S.live && S.over == NONE && n < 200000; n++) update(1 / 60.f);
        printf("wave %2d: approval %3d%%  funds $%d  towers %zu\n", S.wave, (int)S.appr, S.money, S.towers.size());
    }
    printf("result: %s\n", S.over == WIN ? "WIN" : "LOSE");
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "--sim")) return runSim();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(VW, VH, "Capitol Defense");
    SetExitKey(KEY_NULL);
    int mon = GetCurrentMonitor(), mh = GetMonitorHeight(mon), mw = GetMonitorWidth(mon);
    float s0 = mh >= 1300 ? 2.f : mh >= 950 ? 1.5f : 1.f;
    SetWindowSize((int)(VW * s0), (int)(VH * s0));
    SetWindowPosition(std::max(0, (mw - (int)(VW * s0)) / 2), std::max(0, (mh - (int)(VH * s0)) / 2));
    SetWindowMinSize(VW / 2, VH / 2);
    SetTargetFPS(60);
    initMap();
    for (auto* n : NEWS) { tickText += n; tickText += "     *     "; }
    RenderTexture2D target = LoadRenderTexture(VW, VH);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    Cell cur = {0, 0, false};
    bool cursorOn = false;
    while (!WindowShouldClose()) {
        float realDt = GetFrameTime(), dt = std::min(.05f, realDt);
        float sc = std::min(GetScreenWidth() / (float)VW, GetScreenHeight() / (float)VH);
        float ox = (GetScreenWidth() - VW * sc) / 2, oy = (GetScreenHeight() - VH * sc) / 2;
        SetMouseOffset((int)-ox, (int)-oy);
        SetMouseScale(1 / sc, 1 / sc);

        Vector2 m = GetMousePosition();
        Vector2 md = GetMouseDelta();
        if (md.x != 0 || md.y != 0) cursorOn = false;
        bool inField = m.x >= 0 && m.x < W && m.y >= OY && m.y < OY + H;
        bool play = S.started && S.over == NONE;
        bool pad = IsGamepadAvailable(0);
        auto padP = [&](int b) { return pad && IsGamepadButtonPressed(0, b); };

        // Start / restart
        if (!play && (IsKeyPressed(KEY_ENTER) || padP(GAMEPAD_BUTTON_MIDDLE_RIGHT) || padP(GAMEPAD_BUTTON_RIGHT_FACE_DOWN))) {
            if (S.over != NONE) newGame(); else S.started = true;
        } else if (play) {
            int dc = (IsKeyPressed(KEY_RIGHT) || padP(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) - (IsKeyPressed(KEY_LEFT) || padP(GAMEPAD_BUTTON_LEFT_FACE_LEFT));
            int dr = (IsKeyPressed(KEY_DOWN) || padP(GAMEPAD_BUTTON_LEFT_FACE_DOWN)) - (IsKeyPressed(KEY_UP) || padP(GAMEPAD_BUTTON_LEFT_FACE_UP));
            if (dc || dr) {
                if (!cursorOn && inField) cur = {(int)(m.x / T), (int)((m.y - OY) / T), true};
                cursorOn = true;
                cur.c = std::clamp(cur.c + dc, 0, COLS - 1); cur.r = std::clamp(cur.r + dr, 0, ROWS - 1);
            }
            for (int k = 0; k < NT; k++) if (IsKeyPressed(KEY_ONE + k) && (S.money >= TW[k].cost || S.build == k)) pick(k);
            if (padP(GAMEPAD_BUTTON_LEFT_TRIGGER_1) || padP(GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) {
                int step = padP(GAMEPAD_BUTTON_RIGHT_TRIGGER_1) ? 1 : NT - 1, k = S.build;
                for (int n = 0; n < NT; n++) { k = (k < 0 ? (step == 1 ? 0 : NT - 1) : (k + step) % NT); if (S.money >= TW[k].cost) { S.sel = -1; S.build = k; break; } }
            }
            if (IsKeyPressed(KEY_ESCAPE) || padP(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) { S.build = -1; S.sel = -1; }
            if (IsKeyPressed(KEY_SPACE) || padP(GAMEPAD_BUTTON_MIDDLE_RIGHT)) startWave();
            if (IsKeyPressed(KEY_P) || padP(GAMEPAD_BUTTON_MIDDLE_LEFT)) S.paused = !S.paused;
            if (IsKeyPressed(KEY_F) || padP(GAMEPAD_BUTTON_RIGHT_FACE_UP)) S.speed = S.speed % 3 + 1;
            if (IsKeyPressed(KEY_U) || padP(GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) upgradeSel();
            if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) sellSel();
            if (cursorOn && (IsKeyPressed(KEY_ENTER) || padP(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)))
                cellAction(cur.c, cur.r, IsKeyDown(KEY_LEFT_SHIFT));
            if (inField && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                cellAction((int)(m.x / T), (int)((m.y - OY) / T), IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
            if (!S.paused) for (int i = 0; i < S.speed; i++) update(dt);
        }
        Cell hover = cursorOn ? cur : Cell{(int)(m.x / T), (int)((m.y - OY) / T), inField};

        BeginTextureMode(target);
        ClearBackground(BG);
        rlPushMatrix(); rlTranslatef(0, OY, 0);
        drawField((float)GetTime(), hover, cursorOn && play);
        rlPopMatrix();
        drawHud();
        drawPanel(realDt);
        if (drawOverlay()) { if (S.over != NONE) newGame(); else S.started = true; }
        DrawRectangleLinesEx({0, OY - 2.f, W, H + 4.f}, 2, INK);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(target.texture, {0, 0, (float)VW, (float)-VH}, {ox, oy, VW * sc, VH * sc}, {0, 0}, 0, WHITE);
        EndDrawing();
    }
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
