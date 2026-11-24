// Jour / nuit (DayNightSystem) : la sortie est MULTIPLIÉE avec l'image du jeu (BlendMultiply).
// Blanc = image inchangée, noir = image noire.
//
// uv (gl_TexCoord[0]) : 0..1 sur la zone de la caméra.
// ambient  : couleur du moment (DayNightCycle::ambient), 0..1
// vignette : force du vignettage (réglage × obscurité), 0 en plein jour

uniform vec3 ambient;
uniform float vignette;

void main() {
    vec2 uv = gl_TexCoord[0].xy;
    float edge = smoothstep(0.35, 0.85, distance(uv, vec2(0.5)));
    gl_FragColor = vec4(ambient * (1.0 - vignette * edge), 1.0);
}
