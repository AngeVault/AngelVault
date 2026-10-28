// Tests des outils d'interpolation (math/interpolation.h, math/easing.h)
#include "math/easing.h"
#include "math/interpolation.h"

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_failures; } } while (0)
static bool near(float a, float b, float eps = 1e-3f) { return std::abs(a - b) <= eps; }
static bool near(sf::Vector2f a, sf::Vector2f b, float eps = 1e-3f) { return near(a.x, b.x, eps) && near(a.y, b.y, eps); }

int main() {
    using namespace Math;

    // lerp / inverse / remap
    CHECK(near(lerp(0.f, 10.f, 0.25f), 2.5f));
    CHECK(near(lerp(0.f, 10.f, 2.f), 10.f));               // borné
    CHECK(near(lerpUnclamped(0.f, 10.f, 2.f), 20.f));
    CHECK(near(inverseLerp(10.f, 20.f, 15.f), 0.5f));
    CHECK(near(remap(50.f, 0.f, 100.f, -1.f, 1.f), 0.f));
    CHECK(near(lerp(sf::Vector2f{ 0, 0 }, sf::Vector2f{ 10, 20 }, 0.5f), sf::Vector2f{ 5, 10 }));

    // moveTowards : vitesse constante, s'arrête pile
    CHECK(near(moveTowards(0.f, 10.f, 3.f), 3.f));
    CHECK(near(moveTowards(9.f, 10.f, 3.f), 10.f));
    CHECK(near(moveTowards(sf::Vector2f{ 0, 0 }, sf::Vector2f{ 3, 4 }, 2.5f), sf::Vector2f{ 1.5f, 2.f }));
    CHECK(near(moveTowards(sf::Vector2f{ 0, 0 }, sf::Vector2f{ 3, 4 }, 99.f), sf::Vector2f{ 3, 4 }));

    // damp : même résultat à 30 et 144 FPS
    float a = 0.f, b = 0.f;
    for (int i = 0; i < 30; ++i) a = damp(a, 100.f, 5.f, 1.f / 30.f);
    for (int i = 0; i < 144; ++i) b = damp(b, 100.f, 5.f, 1.f / 144.f);
    CHECK(near(a, b, 0.01f));
    CHECK(a > 99.f && a < 100.f);                         // ~99.3 après 1 s
    CHECK(near(dampHalfLife(0.f, 100.f, 0.5f, 0.5f), 50.f));

    // smoothDamp : arrive à la cible, sans la dépasser
    float pos = 0.f, vel = 0.f, maxPos = 0.f;
    for (int i = 0; i < 600; ++i) { pos = smoothDamp(pos, 10.f, vel, 0.3f, 1.f / 60.f); maxPos = std::max(maxPos, pos); }
    CHECK(near(pos, 10.f, 0.01f));
    CHECK(maxPos <= 10.f + 1e-4f);
    sf::Vector2f p{ 0, 0 }, v{ 0, 0 };
    for (int i = 0; i < 600; ++i) p = smoothDamp(p, sf::Vector2f{ 30, -40 }, v, 0.3f, 1.f / 60.f);
    CHECK(near(p, sf::Vector2f{ 30, -40 }, 0.01f));

    // angles
    CHECK(near(deltaAngle(350.f, 10.f), 20.f));
    CHECK(near(deltaAngle(10.f, 350.f), -20.f));
    CHECK(near(repeat(lerpAngle(350.f, 10.f, 0.5f), 360.f), 0.f));
    CHECK(near(moveTowardsAngle(350.f, 10.f, 5.f), 355.f));
    CHECK(near(angleOf({ 0.f, 1.f }), 90.f));
    CHECK(near(fromAngle(180.f), sf::Vector2f{ -1.f, 0.f }));

    // divers
    CHECK(near(pingPong(1.5f, 1.f), 0.5f));
    CHECK(near(repeat(370.f, 360.f), 10.f));
    CHECK(near(normalize({ 3.f, 4.f }), sf::Vector2f{ 0.6f, 0.8f }));
    CHECK(near(normalize({ 0.f, 0.f }), sf::Vector2f{ 0.f, 0.f }));

    // easing : toutes les courbes vont de 0 à 1
    for (int i = 0; i <= int(Ease::Type::InOutBounce); ++i) {
        const auto type = Ease::Type(i);
        CHECK(near(Ease::apply(type, 0.f), 0.f));
        CHECK(near(Ease::apply(type, 1.f), 1.f));
    }
    CHECK(Ease::outBack(0.8f) > 1.f);                     // dépasse avant d'arriver
    CHECK(near(Ease::smoothStep(0.5f), 0.5f));
    CHECK(near(Ease::lerp(0.f, 10.f, 0.5f, Ease::Type::InQuad), 2.5f));

    if (g_failures) { std::printf("%d test(s) en échec\n", g_failures); return EXIT_FAILURE; }
    std::printf("test_math : OK\n");
    return EXIT_SUCCESS;
}
