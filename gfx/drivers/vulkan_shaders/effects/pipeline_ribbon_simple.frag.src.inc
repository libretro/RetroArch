"#version 310 es\nprecision mediump float;layout(std140,set=0,binding=0)uniform UBO{float time;float yflip;float alpha;}co"
"nstants;layout(location=0)out vec4 FragColor;void main(){FragColor=vec4(0.05,0.05,0.05,constants.alpha);}"
