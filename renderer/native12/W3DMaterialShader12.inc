R"HLSL(
cbuffer Draw:register(b0){
 row_major float4x4 current,previous,world,view,textureMatrix[4];
 float4 materialDiffuse,materialAmbient,materialEmissive,ambient,fogColor,fogSettings,options,outputSize;
 float4 lightDirection[8],lightColor[8],clipPlanes[6];
 uint4 colorOp[4],alphaOp[4],textureOptions[4];float4 textureFactor,bumpMatrix[4],bumpLuminance[4];
 float4 waterOptions,waterVertex[16],waterPixel[8];
 uint4 materialSources;float4 lightAmbient[8];
 float4 motionViewport;
 row_major float4x4 normalTransform;float4 materialSpecular,lightingCamera,lightingView;
 float4 lightPosition[8],lightAttenuation[8],lightCone[8],lightSpecular[8];
};
Texture2D tex0:register(t0);Texture2D tex1:register(t1);Texture2D tex2:register(t2);Texture2D tex3:register(t3);
SamplerState sampler0:register(s0);SamplerState sampler1:register(s1);SamplerState sampler2:register(s2);SamplerState sampler3:register(s3);
struct Input{float4 position:POSITION;float4 previous:POSITION1;float3 normal:NORMAL;float4 color:COLOR;float4 specular:COLOR1;
 float4 uv0:TEXCOORD0;float4 uv1:TEXCOORD1;float4 uv2:TEXCOORD2;float4 uv3:TEXCOORD3;};
struct Varying{float4 position:SV_POSITION;float4 color:COLOR;float4 specular:COLOR1;float4 current:TEXCOORD0;float4 previous:TEXCOORD1;
 float4 uv0:TEXCOORD2;float4 uv1:TEXCOORD3;float4 uv2:TEXCOORD4;float4 uv3:TEXCOORD5;float fog:TEXCOORD6;
 float4 clip0:SV_ClipDistance0;float2 clip1:SV_ClipDistance1;};
float4 coordinates(uint stage,Input i,float3 normal,float4 viewPosition){
 uint index=textureOptions[stage].x;float4 uv=i.uv0;
 if((index&65535)==1)uv=i.uv1;else if((index&65535)==2)uv=i.uv2;else if((index&65535)==3)uv=i.uv3;
 uint generation=index&0xffff0000;
 if(generation==0x10000)uv=float4(normal,1);
 else if(generation==0x20000)uv=viewPosition;
 else if(generation==0x30000)uv=float4(reflect(normalize(viewPosition.xyz),normal),1);
 uint flags=textureOptions[stage].y;
 if(flags!=0){uv=mul(uv,textureMatrix[stage]);if(flags&256){uint count=flags&255;uv.xy/=count==4 ? uv.w : count==3 ? uv.z : uv.y;}}
 return uv;
}
Varying VS(Input i){
 Varying o=(Varying)0;float4 worldPosition=mul(i.position,world);float4 viewPosition=mul(worldPosition,view);
 o.current=options.x!=0 ? i.position : mul(i.position,current);
 o.previous=options.x!=0 ? i.position : mul(i.previous,previous);
 if(waterOptions.y!=0){o.current=float4(dot(i.position,waterVertex[2]),dot(i.position,waterVertex[3]),dot(i.position,waterVertex[4]),dot(i.position,waterVertex[5]));o.previous=o.current;}
 // D3D8 rasterizes at integer pixel centers; D3D12 uses centers at +0.5.
 // XYZRHW positions already receive this correction during FVF conversion.
 // W3D XYZ GUI geometry includes a -0.5 bias, so restore the legacy raster
 // convention here too, otherwise adjacent atlas tiles expose vertical seams.
 o.position=o.current;
 if(options.x==0)o.position.xy+=float2(1,-1)/outputSize.zw*o.position.w;
 o.position.xy+=outputSize.xy*float2(2,-2)/outputSize.zw*o.position.w;
 o.color=i.color;o.specular=i.specular;
 float3 normal=normalize(mul(float4(i.normal,0),normalTransform).xyz);
 if(options.y!=0 && options.x==0){
  float4 diffuse=materialSources.x==1 ? i.color : materialSources.x==2 ? i.specular : materialDiffuse;
  float4 ambientMaterial=materialSources.y==1 ? i.color : materialSources.y==2 ? i.specular : materialAmbient;
  float4 emissive=materialSources.z==1 ? i.color : materialSources.z==2 ? i.specular : materialEmissive;
  uint specularSource=materialSources.w&15;
  float3 specularMaterial=specularSource==1 ? i.color.rgb : specularSource==2 ? i.specular.rgb : materialSpecular.rgb;
  float3 viewer=lightingView.w!=0 ? normalize(lightingCamera.xyz-worldPosition.xyz) : normalize(lightingView.xyz);
  float3 illumination=ambient.rgb*ambientMaterial.rgb+emissive.rgb;
  float3 highlights=0;
  [unroll]for(uint j=0;j<8;++j){uint type=uint(lightDirection[j].w);if(type==0)continue;
   float3 toLight=-lightDirection[j].xyz;float attenuation=1;
   if(type!=3){toLight=lightPosition[j].xyz-worldPosition.xyz;float distance=length(toLight);toLight/=max(distance,.000001);
    attenuation=distance>lightPosition[j].w ? 0 : 1/max(.000001,dot(lightAttenuation[j].xyz,float3(1,distance,distance*distance)));
    if(type==2){float rho=dot(-normalize(lightDirection[j].xyz),toLight);
     attenuation*=rho>=lightCone[j].x ? 1 : rho<=lightCone[j].y ? 0 : pow(saturate((rho-lightCone[j].y)/max(.000001,lightCone[j].x-lightCone[j].y)),lightAttenuation[j].w);}
   }else toLight=normalize(toLight);
   float diffuseFactor=max(0,dot(normal,toLight));
   illumination+=attenuation*(lightAmbient[j].rgb*ambientMaterial.rgb+lightColor[j].rgb*diffuseFactor*diffuse.rgb);
   if((materialSources.w&256)!=0 && diffuseFactor>0){float3 halfVector=toLight+viewer;halfVector/=max(.000001,length(halfVector));
    highlights+=attenuation*lightSpecular[j].rgb*specularMaterial*pow(max(0,dot(normal,halfVector)),max(0,lightingCamera.w));}
  }
  o.color=float4(saturate(illumination),diffuse.a);
  o.specular=float4(saturate(highlights),0);
 }
 float3 cameraNormal=normalize(mul(float4(normal,0),view).xyz);
 o.uv0=coordinates(0,i,cameraNormal,viewPosition);o.uv1=coordinates(1,i,cameraNormal,viewPosition);
 o.uv2=coordinates(2,i,cameraNormal,viewPosition);o.uv3=coordinates(3,i,cameraNormal,viewPosition);
 if(waterOptions.y!=0){o.uv0=i.uv0;o.uv1=float4(o.current.xy/o.current.w*waterVertex[6].xy+waterVertex[6].zw,0,1);o.color=i.color;}
 float distance=abs(viewPosition.z);
 o.fog=fogSettings.w==3 ? saturate((fogSettings.y-distance)/max(.00001,fogSettings.y-fogSettings.x)) :
        fogSettings.w==1 ? saturate(exp(-fogSettings.z*distance)) :
        fogSettings.w==2 ? saturate(exp(-pow(fogSettings.z*distance,2))) : 1;
 [unroll]for(uint p=0;p<6;++p){float value=clipPlanes[p].w==3.402823466e+38f ? 1 : dot(worldPosition,clipPlanes[p]);
  if(p<4)o.clip0[p]=value;else o.clip1[p-4]=value;}
 return o;
}
float4 argument(uint arg,float4 diffuse,float4 sampleColor,float4 value,float4 specular){
 float4 result=value;uint kind=arg&15;
 if(kind==0)result=diffuse;else if(kind==2)result=sampleColor;else if(kind==3)result=textureFactor;else if(kind==4)result=specular;
 if(arg&32)result=result.aaaa;if(arg&16)result=1-result;return result;
}
float4 operation(uint op,float4 a,float4 b,float4 arg0,float4 diffuse,float4 sampleColor,float4 value){
 if(op==1)return value;if(op==25)return a*b+arg0;if(op==26)return lerp(b,a,arg0);if(op==2)return a;if(op==3)return b;if(op==4)return a*b;if(op==5)return 2*a*b;if(op==6)return 4*a*b;
 if(op==7)return a+b;if(op==8)return a+b-.5;if(op==9)return 2*(a+b-.5);if(op==10)return a-b;if(op==11)return a+b-a*b;
 if(op==12)return lerp(b,a,diffuse.a);if(op==13)return lerp(b,a,sampleColor.a);if(op==14)return lerp(b,a,textureFactor.a);
 if(op==15)return a+b*(1-sampleColor.a);if(op==16)return lerp(b,a,value.a);if(op==24)return dot(a.rgb*2-1,b.rgb*2-1);
 if(op==18)return float4(a.rgb+b.rgb*a.a,a.a);if(op==19)return float4(a.rgb*b.rgb+a.a,a.a);if(op==20)return float4(a.rgb+b.rgb*(1-a.a),a.a);if(op==21)return float4((1-a.rgb)*b.rgb+a.a,a.a);
 return a*b;
}
bool compare(float a,float b,uint op){if(op==1)return false;if(op==2)return a<b;if(op==3)return a==b;if(op==4)return a<=b;
 if(op==5)return a>b;if(op==6)return a!=b;if(op==7)return a>=b;return true;}
struct Targets{float4 color:SV_TARGET0;float2 motion:SV_TARGET1;float depth:SV_TARGET2;};
Targets PS(Varying i){
 float4 value=i.color;float4 uv[4]={i.uv0,i.uv1,i.uv2,i.uv3};float luminance=1;
 [unroll]for(uint stage=0;stage<4;++stage){
  if(waterOptions.x!=0)break;
  if(colorOp[stage].x==1)break;
  float4 sampled=1;
  if(textureOptions[stage].z!=0){
   if(stage==0)sampled=tex0.Sample(sampler0,uv[0].xy);else if(stage==1)sampled=tex1.Sample(sampler1,uv[1].xy);
   else if(stage==2)sampled=tex2.Sample(sampler2,uv[2].xy);else sampled=tex3.Sample(sampler3,uv[3].xy);
  }
  sampled.rgb*=luminance;luminance=1;
  if(colorOp[stage].x==22 || colorOp[stage].x==23){
   if(stage<3){float2 bump=sampled.xy;
    uv[stage+1].xy+=float2(dot(bump,bumpMatrix[stage].xy),dot(bump,bumpMatrix[stage].zw));}
   if(colorOp[stage].x==23)luminance=sampled.z*bumpLuminance[stage].x+bumpLuminance[stage].y;
   continue;
  }
  float4 a=argument(colorOp[stage].y,i.color,sampled,value,i.specular),b=argument(colorOp[stage].z,i.color,sampled,value,i.specular);
  float4 alphaA=argument(alphaOp[stage].y,i.color,sampled,value,i.specular),alphaB=argument(alphaOp[stage].z,i.color,sampled,value,i.specular);
  float4 arg0=argument(colorOp[stage].w,i.color,sampled,value,i.specular),alpha0=argument(alphaOp[stage].w,i.color,sampled,value,i.specular);
  float4 rgb=operation(colorOp[stage].x,a,b,arg0,i.color,sampled,value),alpha=operation(alphaOp[stage].x,alphaA,alphaB,alpha0,i.color,sampled,value);
  value=float4(saturate(rgb.rgb),saturate(alpha.a));
 }
 if(waterOptions.x!=0){
  float4 t0=tex0.Sample(sampler0,uv[0].xy),t1=tex1.Sample(sampler1,uv[1].xy);
  if(waterOptions.x==1){float2 coordinate=uv[1].xy+float2(dot(t0.xy,bumpMatrix[1].xy),dot(t0.xy,bumpMatrix[1].zw));
   value=tex1.Sample(sampler1,coordinate)*i.color;
   if(textureOptions[2].z!=0)value.a=tex2.Sample(sampler2,uv[2].xy).a*i.color.a;
  }else if(waterOptions.x==2){float4 t2=tex2.Sample(sampler2,uv[2].xy),t3=tex3.Sample(sampler3,uv[3].xy);
   value=float4(i.color.rgb*t0.rgb+(t1.rgb*t2.rgb+t3.rgb)*i.color.a,t0.a*t3.a);
  }else if(waterOptions.x==3){float2 coordinate=uv[2].xy+float2(dot(t1.xy,bumpMatrix[2].xy),dot(t1.xy,bumpMatrix[2].zw));
   value=i.color*t0;value.rgb+=tex2.Sample(sampler2,coordinate).rgb*waterPixel[0].rgb;
  }else if(waterOptions.x==4){float4 t2=tex2.Sample(sampler2,uv[2].xy),t3=tex3.Sample(sampler3,uv[3].xy);
   value=i.color*t0;value.rgb=(value.rgb+t1.rgb*t2.rgb)*t3.rgb;
  }
  value=saturate(value);
 }
 if(options.z!=0 && !compare(value.a,fogColor.a,uint(options.z)))discard;
 if(waterOptions.x==0 && (materialSources.w&256)!=0)value.rgb=saturate(value.rgb+i.specular.rgb);
 if(fogSettings.w!=0)value.rgb=lerp(fogColor.rgb,value.rgb,i.fog);
 Targets o;o.color=value;o.motion=(i.previous.xy/i.previous.w-i.current.xy/i.current.w)*float2(.5,-.5)*motionViewport.xy;o.depth=i.position.z;return o;
}
)HLSL"
