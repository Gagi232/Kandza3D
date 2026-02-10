#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform float alpha;
uniform bool useTexture;
uniform sampler2D uTex;

void main() {
    // Ambient osvetljenje
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse osvetljenje
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec4 texColor = texture(uTex, TexCoords);
    
    vec3 finalColor;
    if(useTexture) {
        finalColor = (ambient + diffuse) * texColor.rgb;
    } else {
        finalColor = (ambient + diffuse) * objectColor;
    }

    FragColor = vec4(finalColor, alpha);
}