static float4 FragColor;

struct SPIRV_Cross_Output
{
    float4 FragColor : SV_Target0;
};

void frag_main()
{
    FragColor = (((1.0f.xxxx + 1.0f.xxxx) + (1.0f.xxx.xyzz + 1.0f.xxxx)) + (1.0f.xxxx + 2.0f.xxxx)) + (1.0f.xx.xyxy + 2.0f.xxxx);
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
