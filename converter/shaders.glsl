-- Quad.VS

in vec2 Position;
in vec2 TexCoord;
out vec2 vTexCoord;

void main()
{
    vTexCoord = TexCoord;
    gl_Position = vec4(Position, 0, 1);
}

-- Quad.FS

in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D Sampler;

void main()
{
    FragColor = texture(Sampler, vTexCoord);
}

-- Mesh.VS

in vec4 Position;
in vec3 Normal;

uniform mat4 Projection;
uniform mat4 Modelview;
uniform mat3 NormalMatrix;
uniform vec3 DiffuseMaterial = vec3(0.75, 0.75, 0.5);

out vec3 EyespaceNormal;
out vec3 Diffuse;

void main()
{
    EyespaceNormal = NormalMatrix * Normal;
    gl_Position = Projection * Modelview * Position;
    Diffuse = DiffuseMaterial;
}

-- Mesh.FS

in vec3 EyespaceNormal;
in vec3 Diffuse;
out vec4 FragColor;

uniform vec3 LightPosition = vec3(0.25, 0.25, 1.0);
uniform vec3 AmbientMaterial = vec3(0.04, 0.04, 0.04);
uniform vec3 SpecularMaterial = vec3(0.5, 0.5, 0.5);
uniform float Shininess = 50;

void main()
{
    vec3 N = normalize(EyespaceNormal);
    vec3 L = normalize(LightPosition);
    vec3 Eye = vec3(0, 0, 1);
    vec3 H = normalize(L + Eye);
    
    float df = max(0.0, dot(N, L));
    float sf = max(0.0, dot(N, H));
    sf = pow(sf, Shininess);

    vec3 color = AmbientMaterial + df * Diffuse + sf * SpecularMaterial;
    FragColor = vec4(color, 1.0);
}
