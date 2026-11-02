// SMAA 1x (Jimenez et al., https://github.com/iryoku/smaa, SMAA.hlsl), preprocessed for this mod:
// SMAA_HLSL_4_1, SMAA_PRESET_HIGH, SMAA_RT_METRICS from the root constants (rt), the samplers as static
// samplers s0 (linear) / s1 (point), comments and blank lines dropped, and our three full-screen entry
// points (edgeVs/edgePs, weightVs/weightPs, blendVs/blendPs) appended. Its licence (MIT):
// Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
// Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
// Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
// Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
// Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software. As clarification, there is no
// requirement that the copyright notice and permission be included in binary
// distributions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#pragma once
namespace spidy::smaa {
// Split under MSVC's 16380-byte limit for one string literal.
inline constexpr const char shader[] =
    R"SMAA(cbuffer Frame : register(b0) { float4 rt; };
SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
float3 SMAAGatherNeighbours(float2 texcoord,
float4 offset[3],
Texture2D tex) {
return tex.Gather(LinearSampler, texcoord + rt.xy * float2(-0.5, -0.5), 0).grb;
}
float2 SMAACalculatePredicatedThreshold(float2 texcoord,
float4 offset[3],
Texture2D predicationTex) {
float3 neighbours = SMAAGatherNeighbours(texcoord, offset, predicationTex);
float2 delta = abs(neighbours.xx - neighbours.yz);
float2 edges = step(0.01, delta);
return 2.0 * 0.1 * (1.0 - 0.4 * edges);
}
void SMAAMovc(bool2 cond, inout float2 variable, float2 value) {
[flatten] if (cond.x) variable.x = value.x;
[flatten] if (cond.y) variable.y = value.y;
}
void SMAAMovc(bool4 cond, inout float4 variable, float4 value) {
SMAAMovc(cond.xy, variable.xy, value.xy);
SMAAMovc(cond.zw, variable.zw, value.zw);
}
void SMAAEdgeDetectionVS(float2 texcoord,
out float4 offset[3]) {
offset[0] = mad(rt.xyxy, float4(-1.0, 0.0, 0.0, -1.0), texcoord.xyxy);
offset[1] = mad(rt.xyxy, float4( 1.0, 0.0, 0.0, 1.0), texcoord.xyxy);
offset[2] = mad(rt.xyxy, float4(-2.0, 0.0, 0.0, -2.0), texcoord.xyxy);
}
void SMAABlendingWeightCalculationVS(float2 texcoord,
out float2 pixcoord,
out float4 offset[3]) {
pixcoord = texcoord * rt.zw;
offset[0] = mad(rt.xyxy, float4(-0.25, -0.125, 1.25, -0.125), texcoord.xyxy);
offset[1] = mad(rt.xyxy, float4(-0.125, -0.25, -0.125, 1.25), texcoord.xyxy);
offset[2] = mad(rt.xxyy,
float4(-2.0, 2.0, -2.0, 2.0) * float(16),
float4(offset[0].xz, offset[1].yw));
}
void SMAANeighborhoodBlendingVS(float2 texcoord,
out float4 offset) {
offset = mad(rt.xyxy, float4( 1.0, 0.0, 0.0, 1.0), texcoord.xyxy);
}
float2 SMAALumaEdgeDetectionPS(float2 texcoord,
float4 offset[3],
Texture2D colorTex
) {
float2 threshold = float2(0.1, 0.1);
float3 weights = float3(0.2126, 0.7152, 0.0722);
float L = dot(colorTex.Sample(PointSampler, texcoord).rgb, weights);
float Lleft = dot(colorTex.Sample(PointSampler, offset[0].xy).rgb, weights);
float Ltop = dot(colorTex.Sample(PointSampler, offset[0].zw).rgb, weights);
float4 delta;
delta.xy = abs(L - float2(Lleft, Ltop));
float2 edges = step(threshold, delta.xy);
if (dot(edges, float2(1.0, 1.0)) == 0.0)
discard;
float Lright = dot(colorTex.Sample(PointSampler, offset[1].xy).rgb, weights);
float Lbottom = dot(colorTex.Sample(PointSampler, offset[1].zw).rgb, weights);
delta.zw = abs(L - float2(Lright, Lbottom));
float2 maxDelta = max(delta.xy, delta.zw);
float Lleftleft = dot(colorTex.Sample(PointSampler, offset[2].xy).rgb, weights);
float Ltoptop = dot(colorTex.Sample(PointSampler, offset[2].zw).rgb, weights);
delta.zw = abs(float2(Lleft, Ltop) - float2(Lleftleft, Ltoptop));
maxDelta = max(maxDelta.xy, delta.zw);
float finalDelta = max(maxDelta.x, maxDelta.y);
edges.xy *= step(finalDelta, 2.0 * delta.xy);
return edges;
}
float2 SMAAColorEdgeDetectionPS(float2 texcoord,
float4 offset[3],
Texture2D colorTex
) {
float2 threshold = float2(0.1, 0.1);
float4 delta;
float3 C = colorTex.Sample(PointSampler, texcoord).rgb;
float3 Cleft = colorTex.Sample(PointSampler, offset[0].xy).rgb;
float3 t = abs(C - Cleft);
delta.x = max(max(t.r, t.g), t.b);
float3 Ctop = colorTex.Sample(PointSampler, offset[0].zw).rgb;
t = abs(C - Ctop);
delta.y = max(max(t.r, t.g), t.b);
float2 edges = step(threshold, delta.xy);
if (dot(edges, float2(1.0, 1.0)) == 0.0)
discard;
float3 Cright = colorTex.Sample(PointSampler, offset[1].xy).rgb;
t = abs(C - Cright);
delta.z = max(max(t.r, t.g), t.b);
float3 Cbottom = colorTex.Sample(PointSampler, offset[1].zw).rgb;
t = abs(C - Cbottom);
delta.w = max(max(t.r, t.g), t.b);
float2 maxDelta = max(delta.xy, delta.zw);
float3 Cleftleft = colorTex.Sample(PointSampler, offset[2].xy).rgb;
t = abs(C - Cleftleft);
delta.z = max(max(t.r, t.g), t.b);
float3 Ctoptop = colorTex.Sample(PointSampler, offset[2].zw).rgb;
t = abs(C - Ctoptop);
delta.w = max(max(t.r, t.g), t.b);
maxDelta = max(maxDelta.xy, delta.zw);
float finalDelta = max(maxDelta.x, maxDelta.y);
edges.xy *= step(finalDelta, 2.0 * delta.xy);
return edges;
}
float2 SMAADepthEdgeDetectionPS(float2 texcoord,
float4 offset[3],
Texture2D depthTex) {
float3 neighbours = SMAAGatherNeighbours(texcoord, offset, depthTex);
float2 delta = abs(neighbours.xx - float2(neighbours.y, neighbours.z));
float2 edges = step((0.1 * 0.1), delta);
if (dot(edges, float2(1.0, 1.0)) == 0.0)
discard;
return edges;
}
float2 SMAADecodeDiagBilinearAccess(float2 e) {
e.r = e.r * abs(5.0 * e.r - 5.0 * 0.75);
return round(e);
}
float4 SMAADecodeDiagBilinearAccess(float4 e) {
e.rb = e.rb * abs(5.0 * e.rb - 5.0 * 0.75);
return round(e);
}
float2 SMAASearchDiag1(Texture2D edgesTex, float2 texcoord, float2 dir, out float2 e) {
float4 coord = float4(texcoord, -1.0, 1.0);
float3 t = float3(rt.xy, 1.0);
while (coord.z < float(8 - 1) &&
coord.w > 0.9) {
coord.xyz = mad(t, float3(dir, 1.0), coord.xyz);
e = edgesTex.SampleLevel(LinearSampler, coord.xy, 0).rg;
coord.w = dot(e, float2(0.5, 0.5));
}
return coord.zw;
}
float2 SMAASearchDiag2(Texture2D edgesTex, float2 texcoord, float2 dir, out float2 e) {
float4 coord = float4(texcoord, -1.0, 1.0);
coord.x += 0.25 * rt.x;
float3 t = float3(rt.xy, 1.0);
while (coord.z < float(8 - 1) &&
coord.w > 0.9) {
coord.xyz = mad(t, float3(dir, 1.0), coord.xyz);
e = edgesTex.SampleLevel(LinearSampler, coord.xy, 0).rg;
e = SMAADecodeDiagBilinearAccess(e);
coord.w = dot(e, float2(0.5, 0.5));
}
return coord.zw;
}
float2 SMAAAreaDiag(Texture2D areaTex, float2 dist, float2 e, float offset) {
float2 texcoord = mad(float2(20, 20), e, dist);
texcoord = mad((1.0 / float2(160.0, 560.0)), texcoord, 0.5 * (1.0 / float2(160.0, 560.0)));
texcoord.x += 0.5;
texcoord.y += (1.0 / 7.0) * offset;
return areaTex.SampleLevel(LinearSampler, texcoord, 0).rg;
}
float2 SMAACalculateDiagWeights(Texture2D edgesTex, Texture2D areaTex, float2 texcoord, float2 e, float4 subsampleIndices) {
float2 weights = float2(0.0, 0.0);
float4 d;
float2 end;
if (e.r > 0.0) {
d.xz = SMAASearchDiag1(edgesTex, texcoord, float2(-1.0, 1.0), end);
d.x += float(end.y > 0.9);
} else
d.xz = float2(0.0, 0.0);
d.yw = SMAASearchDiag1(edgesTex, texcoord, float2(1.0, -1.0), end);
[branch]
if (d.x + d.y > 2.0) {
float4 coords = mad(float4(-d.x + 0.25, d.x, d.y, -d.y - 0.25), rt.xyxy, texcoord.xyxy);
float4 c;
c.xy = edgesTex.SampleLevel(LinearSampler, coords.xy, 0, int2(-1, 0)).rg;
c.zw = edgesTex.SampleLevel(LinearSampler, coords.zw, 0, int2( 1, 0)).rg;
c.yxwz = SMAADecodeDiagBilinearAccess(c.xyzw);
float2 cc = mad(float2(2.0, 2.0), c.xz, c.yw);
SMAAMovc(bool2(step(0.9, d.zw)), cc, float2(0.0, 0.0));
weights += SMAAAreaDiag(areaTex, d.xy, cc, subsampleIndices.z);
}
d.xz = SMAASearchDiag2(edgesTex, texcoord, float2(-1.0, -1.0), end);
if (edgesTex.SampleLevel(LinearSampler, texcoord, 0, int2(1, 0)).r > 0.0) {
d.yw = SMAASearchDiag2(edgesTex, texcoord, float2(1.0, 1.0), end);
d.y += float(end.y > 0.9);
} else
d.yw = float2(0.0, 0.0);
[branch]
if (d.x + d.y > 2.0) {
float4 coords = mad(float4(-d.x, -d.x, d.y, d.y), rt.xyxy, texcoord.xyxy);
float4 c;
c.x = edgesTex.SampleLevel(LinearSampler, coords.xy, 0, int2(-1, 0)).g;
c.y = edgesTex.SampleLevel(LinearSampler, coords.xy, 0, int2( 0, -1)).r;
c.zw = edgesTex.SampleLevel(LinearSampler, coords.zw, 0, int2( 1, 0)).gr;
float2 cc = mad(float2(2.0, 2.0), c.xz, c.yw);
SMAAMovc(bool2(step(0.9, d.zw)), cc, float2(0.0, 0.0));
weights += SMAAAreaDiag(areaTex, d.xy, cc, subsampleIndices.w).gr;
}
return weights;
}
float SMAASearchLength(Texture2D searchTex, float2 e, float offset) {
float2 scale = float2(66.0, 33.0) * float2(0.5, -1.0);
float2 bias = float2(66.0, 33.0) * float2(offset, 1.0);
scale += float2(-1.0, 1.0);
bias += float2( 0.5, -0.5);
scale *= 1.0 / float2(64.0, 16.0);
bias *= 1.0 / float2(64.0, 16.0);
return searchTex.SampleLevel(LinearSampler, mad(scale, e, bias), 0).r;
}
float SMAASearchXLeft(Texture2D edgesTex, Texture2D searchTex, float2 texcoord, float end) {
float2 e = float2(0.0, 1.0);
while (texcoord.x > end &&
e.g > 0.8281 &&
e.r == 0.0) {
e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).rg;
texcoord = mad(-float2(2.0, 0.0), rt.xy, texcoord);
}
float offset = mad(-(255.0 / 127.0), SMAASearchLength(searchTex, e, 0.0), 3.25);
return mad(rt.x, offset, texcoord.x);
}
float SMAASearchXRight(Texture2D edgesTex, Texture2D searchTex, float2 texcoord, float end) {
float2 e = float2(0.0, 1.0);
while (texcoord.x < end &&
e.g > 0.8281 &&
e.r == 0.0) {
e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).rg;
texcoord = mad(float2(2.0, 0.0), rt.xy, texcoord);
}
float offset = mad(-(255.0 / 127.0), SMAASearchLength(searchTex, e, 0.5), 3.25);
return mad(-rt.x, offset, texcoord.x);
}
float SMAASearchYUp(Texture2D edgesTex, Texture2D searchTex, float2 texcoord, float end) {
float2 e = float2(1.0, 0.0);
while (texcoord.y > end &&
e.r > 0.8281 &&
e.g == 0.0) {
e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).rg;
texcoord = mad(-float2(0.0, 2.0), rt.xy, texcoord);
}
float offset = mad(-(255.0 / 127.0), SMAASearchLength(searchTex, e.gr, 0.0), 3.25);
return mad(rt.y, offset, texcoord.y);
}
float SMAASearchYDown(Texture2D edgesTex, Texture2D searchTex, float2 texcoord, float end) {
float2 e = float2(1.0, 0.0);
while (texcoord.y < end &&
e.r > 0.8281 &&
e.g == 0.0) {
e = edgesTex.SampleLevel(LinearSampler, texcoord, 0).rg;
texcoord = mad(float2(0.0, 2.0), rt.xy, texcoord);
}
float offset = mad(-(255.0 / 127.0), SMAASearchLength(searchTex, e.gr, 0.5), 3.25);
return mad(-rt.y, offset, texcoord.y);
}
float2 SMAAArea(Texture2D areaTex, float2 dist, float e1, float e2, float offset) {
float2 texcoord = mad(float2(16, 16), round(4.0 * float2(e1, e2)), dist);
texcoord = mad((1.0 / float2(160.0, 560.0)), texcoord, 0.5 * (1.0 / float2(160.0, 560.0)));
texcoord.y = mad((1.0 / 7.0), offset, texcoord.y);
return areaTex.SampleLevel(LinearSampler, texcoord, 0).rg;
}
void SMAADetectHorizontalCornerPattern(Texture2D edgesTex, inout float2 weights, float4 texcoord, float2 d) {
float2 leftRight = step(d.xy, d.yx);
float2 rounding = (1.0 - (float(25) / 100.0)) * leftRight;
rounding /= leftRight.x + leftRight.y;
float2 factor = float2(1.0, 1.0);
factor.x -= rounding.x * edgesTex.SampleLevel(LinearSampler, texcoord.xy, 0, int2(0, 1)).r;
factor.x -= rounding.y * edgesTex.SampleLevel(LinearSampler, texcoord.zw, 0, int2(1, 1)).r;
factor.y -= rounding.x * edgesTex.SampleLevel(LinearSampler, texcoord.xy, 0, int2(0, -2)).r;
factor.y -= rounding.y * edgesTex.SampleLevel(LinearSampler, texcoord.zw, 0, int2(1, -2)).r;
weights *= saturate(factor);
}
void SMAADetectVerticalCornerPattern(Texture2D edgesTex, inout float2 weights, float4 texcoord, float2 d) {
float2 leftRight = step(d.xy, d.yx);
float2 rounding = (1.0 - (float(25) / 100.0)) * leftRight;
rounding /= leftRight.x + leftRight.y;
float2 factor = float2(1.0, 1.0);
factor.x -= rounding.x * edgesTex.SampleLevel(LinearSampler, texcoord.xy, 0, int2( 1, 0)).g;
factor.x -= rounding.y * edgesTex.SampleLevel(LinearSampler, texcoord.zw, 0, int2( 1, 1)).g;
factor.y -= rounding.x * edgesTex.SampleLevel(LinearSampler, texcoord.xy, 0, int2(-2, 0)).g;
factor.y -= rounding.y * edgesTex.SampleLevel(LinearSampler, texcoord.zw, 0, int2(-2, 1)).g;
weights *= saturate(factor);
}
float4 SMAABlendingWeightCalculationPS(float2 texcoord,
float2 pixcoord,
float4 offset[3],
Texture2D edgesTex,
Texture2D areaTex,
Texture2D searchTex,
float4 subsampleIndices) {
float4 weights = float4(0.0, 0.0, 0.0, 0.0);
float2 e = edgesTex.Sample(LinearSampler, texcoord).rg;
[branch]
if (e.g > 0.0) {
weights.rg = SMAACalculateDiagWeights(edgesTex, areaTex, texcoord, e, subsampleIndices);
[branch]
if (weights.r == -weights.g) {
float2 d;
float3 coords;
coords.x = SMAASearchXLeft(edgesTex, searchTex, offset[0].xy, offset[2].x);
coords.y = offset[1].y;
d.x = coords.x;
float e1 = edgesTex.SampleLevel(LinearSampler, coords.xy, 0).r;
coords.z = SMAASearchXRight(edgesTex, searchTex, offset[0].zw, offset[2].y);
d.y = coords.z;
)SMAA"
    R"SMAA(d = abs(round(mad(rt.zz, d, -pixcoord.xx)));
float2 sqrt_d = sqrt(d);
float e2 = edgesTex.SampleLevel(LinearSampler, coords.zy, 0, int2(1, 0)).r;
weights.rg = SMAAArea(areaTex, sqrt_d, e1, e2, subsampleIndices.y);
coords.y = texcoord.y;
SMAADetectHorizontalCornerPattern(edgesTex, weights.rg, coords.xyzy, d);
} else
e.r = 0.0;
}
[branch]
if (e.r > 0.0) {
float2 d;
float3 coords;
coords.y = SMAASearchYUp(edgesTex, searchTex, offset[1].xy, offset[2].z);
coords.x = offset[0].x;
d.x = coords.y;
float e1 = edgesTex.SampleLevel(LinearSampler, coords.xy, 0).g;
coords.z = SMAASearchYDown(edgesTex, searchTex, offset[1].zw, offset[2].w);
d.y = coords.z;
d = abs(round(mad(rt.ww, d, -pixcoord.yy)));
float2 sqrt_d = sqrt(d);
float e2 = edgesTex.SampleLevel(LinearSampler, coords.xz, 0, int2(0, 1)).g;
weights.ba = SMAAArea(areaTex, sqrt_d, e1, e2, subsampleIndices.x);
coords.x = texcoord.x;
SMAADetectVerticalCornerPattern(edgesTex, weights.ba, coords.xyxz, d);
}
return weights;
}
float4 SMAANeighborhoodBlendingPS(float2 texcoord,
float4 offset,
Texture2D colorTex,
Texture2D blendTex
) {
float4 a;
a.x = blendTex.Sample(LinearSampler, offset.xy).a;
a.y = blendTex.Sample(LinearSampler, offset.zw).g;
a.wz = blendTex.Sample(LinearSampler, texcoord).xz;
[branch]
if (dot(a, float4(1.0, 1.0, 1.0, 1.0)) < 1e-5) {
float4 color = colorTex.SampleLevel(LinearSampler, texcoord, 0);
return color;
} else {
bool h = max(a.x, a.z) > max(a.y, a.w);
float4 blendingOffset = float4(0.0, a.y, 0.0, a.w);
float2 blendingWeight = a.yw;
SMAAMovc(bool4(h, h, h, h), blendingOffset, float4(a.x, 0.0, a.z, 0.0));
SMAAMovc(bool2(h, h), blendingWeight, a.xz);
blendingWeight /= dot(blendingWeight, float2(1.0, 1.0));
float4 blendingCoord = mad(blendingOffset, float4(rt.xy, -rt.xy), texcoord.xyxy);
float4 color = blendingWeight.x * colorTex.SampleLevel(LinearSampler, blendingCoord.xy, 0);
color += blendingWeight.y * colorTex.SampleLevel(LinearSampler, blendingCoord.zw, 0);
return color;
}
}
float4 SMAAResolvePS(float2 texcoord,
Texture2D currentColorTex,
Texture2D previousColorTex
) {
float4 current = currentColorTex.Sample(PointSampler, texcoord);
float4 previous = previousColorTex.Sample(PointSampler, texcoord);
return lerp(current, previous, 0.5);
}
void SMAASeparatePS(float4 position,
float2 texcoord,
out float4 target0,
out float4 target1,
Texture2DMS<float4, 2> colorTexMS) {
int2 pos = int2(position.xy);
target0 = colorTexMS.Load(pos, 0);
target1 = colorTexMS.Load(pos, 1);
}
Texture2D t0 : register(t0);
Texture2D t1 : register(t1);
Texture2D t2 : register(t2);
struct EdgeV { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 o0 : TEXCOORD1; float4 o1 : TEXCOORD2; float4 o2 : TEXCOORD3; };
struct WeightV { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float2 pix : TEXCOORD1; float4 o0 : TEXCOORD2; float4 o1 : TEXCOORD3; float4 o2 : TEXCOORD4; };
struct BlendV { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 o : TEXCOORD1; };
float2 corner(uint id) { return float2((id << 1) & 2, id & 2); }
float4 place(float2 uv) { return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1); }
EdgeV edgeVs(uint id : SV_VertexID) {
EdgeV v; v.uv = corner(id); v.pos = place(v.uv);
float4 o[3]; SMAAEdgeDetectionVS(v.uv, o); v.o0 = o[0]; v.o1 = o[1]; v.o2 = o[2];
return v;
}
float2 edgePs(EdgeV v) : SV_TARGET {
float4 o[3] = {v.o0, v.o1, v.o2};
return SMAALumaEdgeDetectionPS(v.uv, o, t0);
}
WeightV weightVs(uint id : SV_VertexID) {
WeightV v; v.uv = corner(id); v.pos = place(v.uv);
float4 o[3]; SMAABlendingWeightCalculationVS(v.uv, v.pix, o); v.o0 = o[0]; v.o1 = o[1]; v.o2 = o[2];
return v;
}
float4 weightPs(WeightV v) : SV_TARGET {
float4 o[3] = {v.o0, v.o1, v.o2};
return SMAABlendingWeightCalculationPS(v.uv, v.pix, o, t0, t1, t2, float4(0, 0, 0, 0));
}
BlendV blendVs(uint id : SV_VertexID) {
BlendV v; v.uv = corner(id); v.pos = place(v.uv);
SMAANeighborhoodBlendingVS(v.uv, v.o);
return v;
}
float4 blendPs(BlendV v) : SV_TARGET { return SMAANeighborhoodBlendingPS(v.uv, v.o, t0, t1); }
)SMAA";
} // namespace spidy::smaa
