#version 450

in float v0;
in float v1;
out float FragColor;

void main()
{
    float a = v0;
    float b = v1;
    float _17 = a;
    a = v1;
    FragColor = (_17 + b) * b;
}

