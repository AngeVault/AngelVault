// post-process du jeu : applique a toute l'image apres le rendu
// choisi dans Edition > Parametres du jeu... (ou "shader = ..." dans assets/settings.cfg)
//
// fournis par le moteur
//   frame       l'image du jeu
//   resolution  taille de l'image en pixels
//   time        secondes depuis le chargement du shader
//
// reglages : "uniform float <nom>; // @param <defaut> <min> <max>"
// l'editeur en fait des curseurs, les valeurs vont dans settings.cfg (shader.<nom> = ...)
// avec les valeurs par defaut l'image ne change pas
// pas d'accents ici : certains drivers refusent de compiler

#version 120

uniform sampler2D frame;
uniform vec2 resolution;
uniform float time;

uniform float exposure;       // @param 0 -3 3
uniform float brightness;     // @param 0 -1 1
uniform float contrast;       // @param 1 0 3
uniform float saturation;     // @param 1 0 3
uniform float gamma;          // @param 1 0.2 3
uniform float temperature;    // @param 0 -1 1
uniform float tint;           // @param 0 -1 1
uniform float aberration;     // @param 0 0 10
uniform float vignette;       // @param 0 0 1
uniform float vignetteRadius; // @param 0.75 0.1 1.5
uniform float vignetteSoft;   // @param 0.45 0.01 1
uniform float grain;          // @param 0 0 0.5
uniform float scanlines;      // @param 0 0 1
uniform float pixelate;       // @param 1 1 16

// poids de chaque canal dans la luminosite percue (Rec. 709) : l'oeil voit surtout le vert
const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);

// bruit pseudo aleatoire 0..1 a partir d'une position
// sin() d'un produit scalaire * un grand nombre, on garde la partie decimale (fract)
// la valeur saute d'un pixel a l'autre sans motif visible
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = gl_TexCoord[0].xy;   // 0..1 sur l'image

    // pixelisation : uv arrondi a une grille de cases de "pixelate" pixels
    // floor() ramene tous les pixels d'une case au meme coin, +0.5 vise le centre de la case
    if (pixelate > 1.0) {
        vec2 cell = pixelate / resolution;
        uv = (floor(uv / cell) + 0.5) * cell;
    }

    // aberration chromatique : rouge et bleu lus un peu decales, en sens opposes
    // dir va du centre vers le pixel donc le decalage grandit vers les bords
    // / resolution : "aberration" est en pixels ecran
    vec2 dir = uv - 0.5;
    vec2 shift = dir * aberration / resolution * 2.0;
    vec3 color;
    color.r = texture2D(frame, uv + shift).r;
    color.g = texture2D(frame, uv).g;
    color.b = texture2D(frame, uv - shift).b;

    // exposition en "stops" comme un appareil photo : 2^exposure
    // +1 double la lumiere, -1 la divise par 2
    color *= pow(2.0, exposure);

    // luminosite : meme valeur ajoutee aux 3 canaux, tout monte vers le blanc ou descend vers le noir
    color += brightness;

    // contraste : on ecarte (> 1) ou rapproche (< 1) chaque canal du gris moyen 0.5
    // (c - 0.5) centre sur 0, * contrast etire, + 0.5 remet en place
    color = (color - 0.5) * contrast + 0.5;

    // saturation : melange entre la version noir et blanc et la couleur
    // luma = moyenne ponderee des canaux, mix(a, b, t) = a + (b - a) * t
    // 0 = gris, 1 = inchange, > 1 = couleurs poussees
    float luma = dot(color, LUMA);
    color = mix(vec3(luma), color, saturation);

    // temperature : + plus chaud (rouge monte, bleu baisse), - plus froid
    // teinte : + vers le magenta (moins de vert), - vers le vert
    color.r += temperature * 0.1;
    color.b -= temperature * 0.1;
    color.g -= tint * 0.1;

    // gamma : courbe c^(1/gamma) sur 0..1, 0 et 1 ne bougent pas, seuls les tons moyens changent
    // gamma > 1 eclaircit, < 1 assombrit
    // clamp avant pow : pow d'un nombre negatif n'est pas defini
    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / gamma));

    // vignettage : on assombrit selon la distance au centre
    // d corrige le format de l'ecran, sinon le cercle devient un ovale sur un ecran large
    // smoothstep(radius, radius - soft, dist) vaut 1 au centre et descend a 0 entre les deux rayons
    // mix(1, v, vignette) regle la force : 0 = aucun effet, 1 = bords noirs
    vec2 d = dir * vec2(resolution.x / resolution.y, 1.0);
    float v = smoothstep(vignetteRadius, vignetteRadius - vignetteSoft, length(d));
    color *= mix(1.0, v, vignette);

    // scanlines : une ligne de pixels sur deux plus sombre, comme un vieil ecran CRT
    // sin(y * pi) fait une vague de periode 2 pixels, * 0.5 + 0.5 la ramene en 0..1
    float wave = sin(uv.y * resolution.y * 3.14159) * 0.5 + 0.5;
    color *= 1.0 - scanlines * (1.0 - wave) * 0.5;

    // grain de pellicule : bruit centre sur 0 (-0.5..0.5) ajoute a chaque pixel
    // time dans le hash : le bruit change a chaque image comme un vrai grain
    float noise = hash(uv * resolution + fract(time) * 100.0) - 0.5;
    color += noise * grain;

    gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
