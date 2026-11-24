#include "game_settings.h"
#include "render/post_process.h"

#include <clocale>
#include <cmath>
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)

static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main() {
    // comme l'éditeur : qt met la locale du système, séparateur décimal en virgule
    if (!std::setlocale(LC_NUMERIC, "fr_FR.UTF-8")) std::printf("locale fr_FR absente, test sans elle\n");

    GameSettings s;
    s.loadFromString("shader.gamma = 0.9\nshader.brightness = -0.02\nshader.contrast = 1.15\nshader.bad = abc\n");
    CHECK(near(s.shaderParams["gamma"], 0.9f));
    CHECK(near(s.shaderParams["brightness"], -0.02f));
    CHECK(near(s.shaderParams["contrast"], 1.15f));
    CHECK(!s.shaderParams.count("bad"));

    const auto params = PostProcess::parseParams("uniform float grain; // @param 0 0 0.5\nuniform float radius; // @param 0.75 0.1 1.5\n");
    CHECK(params.size() == 2);
    if (params.size() == 2) {
        CHECK(params[0].name == "grain" && near(params[0].max, 0.5f));
        CHECK(near(params[1].value, 0.75f) && near(params[1].min, 0.1f));
    }

    if (g_failures == 0) std::printf("test_settings : OK\n");
    return g_failures == 0 ? 0 : 1;
}
