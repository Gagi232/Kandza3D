#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 objectColor;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 spotlightPos;
uniform float alpha;
uniform bool useTexture;
uniform sampler2D uTex;
uniform vec3 viewPos;
uniform float shininess;
uniform float specularStrength;

uniform vec3 pointLightPos;
uniform vec3 pointLightColor;

void main() {
    float ambientStrength = 0.6;
    vec3 baseAmbient = vec3(0.45);
    vec3 ambient = baseAmbient + ambientStrength * (lightColor + pointLightColor) * 0.5;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 plDir = normalize(pointLightPos - FragPos);
    float diff2 = max(dot(norm, plDir), 0.0);
    vec3 diffuse2 = diff2 * pointLightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float specFactor = pow(max(dot(norm, halfDir), 0.0), max(shininess, 1.0));
    vec3 specular = specularStrength * specFactor * lightColor;

    vec3 halfDir2 = normalize(plDir + viewDir);
    float specFactor2 = pow(max(dot(norm, halfDir2), 0.0), max(shininess, 1.0));
    vec3 specular2 = specularStrength * specFactor2 * pointLightColor;

    vec3 spotDir = vec3(0.0, -1.0, 0.0);
    vec3 lightToFrag = normalize(FragPos - spotlightPos);
    float theta = dot(lightToFrag, normalize(spotDir));
    
    vec3 spotlight = vec3(0.0);
    if(theta > 0.95) {
        float spotDiff = max(dot(norm, -lightToFrag), 0.0);
        spotlight = spotDiff * vec3(1.0, 1.0, 0.8) * 2.0;
        vec3 spotHalf = normalize(-lightToFrag + viewDir);
        float spotSpec = pow(max(dot(norm, spotHalf), 0.0), max(shininess,1.0));
        spotlight += specularStrength * spotSpec * vec3(1.0, 1.0, 0.8) * 0.5;
    }

    if(useTexture) {
        vec4 texColor = texture(uTex, TexCoords);
        if(texColor.a < 0.1) discard;
        FragColor = vec4(texColor.rgb, texColor.a * alpha);
        return;
    } else {
        vec3 result = (ambient + diffuse + diffuse2 + specular + specular2 + spotlight) * objectColor;
        FragColor = vec4(result, alpha);
    }
}