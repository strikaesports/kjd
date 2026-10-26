#version 330 core

in vec3 vFragPos;
in vec3 vNormal;
in vec3 vColor;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uViewPos;

void main() {
    // For wireframe edges (normal length near zero), output crisp unlit white color
    if (length(vNormal) < 0.1) {
        FragColor = vec4(vColor, 1.0);
        return;
    }

    // Constant multi-directional illumination from all sides
    // High ambient base guarantees every side is brightly and constantly lit
    float ambientStrength = 0.80;
    vec3 ambient = ambientStrength * vColor;

    // Balanced fill lighting from multiple opposing directions to retain 3D depth and edge clarity
    vec3 norm = normalize(vNormal);
    vec3 light1 = normalize(vec3(0.577, 0.577, 0.577));   // Top-right-front
    vec3 light2 = normalize(vec3(-0.577, -0.577, -0.577)); // Bottom-left-back

    float diff1 = max(dot(norm, light1), 0.0) * 0.12;
    float diff2 = max(dot(norm, light2), 0.0) * 0.08;
    vec3 diffuse = (diff1 + diff2) * vColor;

    vec3 result = clamp(ambient + diffuse, 0.0, 1.0);
    FragColor = vec4(result, 1.0);
}
