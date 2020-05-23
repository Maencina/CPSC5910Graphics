// DispBumpTexSTH-Simple.cpp: displacement/bump/texture-mapped sphere/tube/height-field

#include <glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>
#include "Camera.h"
#include "Draw.h"
#include "GLXtras.h"
#include "Misc.h"
#include "Text.h"
#include "VecMat.h"
#include "Widgets.h"

const int nScenes = 5;

class Scene {
public:
    std::string    depthFile, textureFile;
    int            depthWidth, depthHeight, res;
    unsigned char *bumpPixels, *depthPixels;
    int            textureUnit, bumpUnit, depthUnit;
    GLuint         textureName, bumpName, depthName;
    int            view;  // 0:scene, 1:texture map, 2:normal map, 3: depth map
    float          pixelScale, displaceScale;
    int            disableBumpMap;      // 0: use bump map, 1: no bump map
    int            disableTextureMap;
    int            disableDiffuse;
    int            disableSpecular;
    int            approxNormals;
    int            oneSided;
    int            showLines;
    int            uberRes;
    int            hiliteColor;
    float          lineWidth, outlineTransition;
    bool           stripesTexture, warpTexture;
    Scene() {
        depthPixels = bumpPixels = NULL;
        pixelScale = 25;
        uberRes = 1;
        hiliteColor = 1;
        displaceScale = 0;
        res = 70;
        view = 0;
        showLines = disableBumpMap = disableTextureMap = disableDiffuse = disableSpecular = 0;
        approxNormals = 0;
        oneSided = 1;
        lineWidth = 1;
        outlineTransition = 1;
        stripesTexture = warpTexture = false;
    }
    void UpdateBumps() {
        delete [] bumpPixels; // ugh
        bumpPixels = GetNormals(depthPixels, depthWidth, depthHeight, pixelScale);
        LoadTexture(bumpPixels, depthWidth, depthHeight, bumpUnit, bumpName);
    }
    void UpdateBlur() {
        int blur = 3;
        int nBytes = 3*depthWidth*depthHeight;
        unsigned char *tmp = new unsigned char[nBytes];
        memcpy(tmp, depthPixels, nBytes);
        for (int y = 0; y < depthHeight; y++) {
            int j0 = y > blur? y-blur : 0, j1 = y < depthHeight-blur? y+blur : depthHeight-1;
            for (int x = 0; x < depthWidth; x++) {
                int i0 = x > blur? x-blur : 0, i1 = x < depthWidth-blur? x+blur : depthWidth-1;
                unsigned int val[] = {0, 0, 0}, num = 0;
                for (int j = j0; j <= j1; j++) {
                    for (int i = i0; i <= i1; i++) {
                        int off = 3*(j*depthWidth+i);
                        for (int k = 0; k < 3; k++)
                            val[k] += (unsigned int) tmp[off+k];
                        num++;
                    }
                }
                for (int k = 0; k < 3; k++)
                    depthPixels[3*(y*depthWidth+x)+k] = (unsigned char) (val[k]/num);
            }
        }
        delete [] tmp;
        UpdateDepth();
    }
    void ResetDepth() {
        depthPixels = ReadTarga(depthFile.c_str(), depthWidth, depthHeight);
        UpdateDepth();
    }
    void UpdateDepth() {
        LoadTexture(depthPixels, depthWidth, depthHeight, depthUnit, depthName, true); // BGR true?
        bumpPixels = GetNormals(depthPixels, depthWidth, depthHeight, pixelScale);
        LoadTexture(bumpPixels, depthWidth, depthHeight, bumpUnit, bumpName, true);
    }
    void Init(int id, std::string depthFilename, std::string textureFilename) {
        if (id == 2)
            displaceScale = .6f;
        depthFile = depthFilename;
        textureFile = textureFilename;
        textureUnit = 3*id;
        bumpUnit = textureUnit+1;
        depthUnit = textureUnit+2;
        textureName = LoadTexture(textureFile.c_str(), textureUnit);
        depthPixels = ReadTarga(depthFile.c_str(), depthWidth, depthHeight);
        bumpPixels = GetNormals(depthPixels, depthWidth, depthHeight, pixelScale);
        depthName = LoadTexture(depthPixels, depthWidth, depthHeight, depthUnit, true);
        bumpName = LoadTexture(bumpPixels, depthWidth, depthHeight, bumpUnit, true);
    }
    ~Scene() {
        delete [] depthPixels;
        delete [] bumpPixels;
        glDeleteBuffers(1, &textureName);
        glDeleteBuffers(1, &bumpName);
        glDeleteBuffers(1, &depthName);
    }
} scenes[nScenes];

// scene
int             scene = 0; // 0:ball, 1:tube, 2:quad-Yosemite, 3:quad-Penny, 4:quad-ripples
float           leftEdge = 0, rightEdge = 1;
float           topEdge = scene == 2? -.02f : 0, bottomEdge = scene == 2? .9f : 1; // kluge for Yosemite Valley

// view
int             winWidth = 800, winHeight = 800;
int             xCursorOffset = -7, yCursorOffset = -3;
Camera          camera(winWidth, winHeight, vec3(scene==2?-45:0,0,scene==2?180:0), vec3(0,0,-5));
bool            showText = true, showLightInfo = false;

// stripes
const char     *stripesFilename = "C:/Users/jules/CodeBlocks/Aids/Stripes.tga";
int             stripesTextureId = 5, stripesTextureUnit = 3*stripesTextureId;
GLuint          stripesTextureName = 0;
int             warpTextureId = 6, warpTextureUnit = stripesTextureUnit+1;
GLuint          warpTextureName = 0;

// lights
class Light {
public:
    vec3 p;
    int cid;
    Light(vec3 p = vec3(0,0,0), int cid = 0) : p(p), cid(cid) { }
};
std::vector<Light> lights(3, Light(vec3(.46f,-.47f,.55f), 3)); // vec3(.17f,-.66f,1.36f); .6f, -.6f, .8); (-.2f, .4f, .8f);
Light l1 = lights[1] = Light(vec3(-.53f,-.13f,.74f), 2), l2 = lights[2] = Light(vec3(.6f,.06f,.4f), 1);
vec3 colors[] = {vec3(1,1,1), vec3(1,0,0), vec3(0,1,0), vec3(0,0,1), vec3(1,1,0), vec3(1,0.6f,0), vec3(1,0,1), vec3(0,1,1)};

// interaction
void           *picked = NULL, *hover = NULL, *light = NULL;
Mover           mover;
time_t          mouseMove = clock();

// shader indices
GLuint          shapeShader, imageShader;

// vertex shader
const char *vShaderCode = "void main() { gl_Position = vec4(0); }";

// tessellation evaluation: set vertex position/normal/uv for sphere, tube, height field
const char *teShaderCode = R"(
    #version 400 core
    layout (quads, equal_spacing, ccw) in;
    uniform mat4 modelview;
    uniform mat4 persp;
    uniform sampler2D heightField;
    uniform float displaceScale = 0;
    uniform int shape = 0; // 0: quad, 1: ball, 2: tube
    uniform int approxNormals = 0;
    uniform vec2 uvOffset = vec2(0);
    uniform float uvScale = 1;
    out vec3 tePoint;
    out vec3 teNormal;
    out vec2 teUv;
    vec3 PtFromSphere(float u, float v) {
        // u is longitude, v is latitude (PI/2 = N. pole, 0 = equator, -PI/2 = S. pole)
        float _PI = 3.141592;
        float elevation = -_PI/2+_PI*v; // _PI/2-_PI*v;
        float eFactor = cos(elevation);
        float y = sin(elevation);
        float angle = 2*_PI*(1-u); // 2*_PI*u;
        float x = eFactor*cos(angle), z = eFactor*sin(angle);
        return vec3(x, y, z);
    }
    void Quad(float u, float v, out vec3 p, out vec3 n) {
        p = vec3(2*u-1, 2*v-1, 0);
        n = vec3(0, 0, 1);
    }
    void Ball(float u, float v, out vec3 p, out vec3 n) {
        p = PtFromSphere(u, v);
        n = p;
        p = .75*p;
    }
    void Tube(float u, float v, out vec3 p, out vec3 n) {
        float c = cos(2*3.1415*u), s = sin(2*3.1415*u);
        p = vec3(-1+2*v, .4*c, .4*s);
        n = vec3(0, c, s);
    }
    void PN(float u, float v, out vec3 p, out vec3 n) {
        shape == 0? Ball(u, v, p, n) : shape == 1? Tube(u, v, p, n) : Quad(u, v, p, n);
        float height = texture(heightField, vec2(u, v)).r;
        p += displaceScale*height*n;
    }
    vec3 P(float u, float v) {
        vec3 p, n;
        PN(u, v, p, n);
        return p;
    }
    vec3 NormalFromField(float u, float v) {
        // approximate normal with central differencing
        float du = 1/gl_TessLevelInner[0], dv = 1/gl_TessLevelInner[1];
        vec3 texL = P(max(0,u-du), v);
        vec3 texR = P(min(1,u+du), v);
        vec3 texT = P(u, max(0,v-dv));
        vec3 texB = P(u, min(1,v+dv));
        return -normalize(cross(texB-texT, texR-texL));
    }
    void main() {
        teUv = uvOffset+uvScale*gl_TessCoord.st;
        float u = teUv.s, v = teUv.t;
        vec3 p, n;
        PN(u, v, p, n);
        if (approxNormals == 1)
            n = NormalFromField(u, v);
        tePoint = (modelview*vec4(p, 1)).xyz;
        teNormal = (modelview*vec4(n, 0)).xyz;
        gl_Position = persp*vec4(tePoint, 1);
    }
)";

// geometry shader with line-drawing
const char *gShaderCode = R"(
    #version 330 core
    layout (triangles) in;
    layout (triangle_strip, max_vertices = 3) out;
    in vec3 tePoint[];
    in vec3 teNormal[];
    in vec2 teUv[];
    out vec3 gPoint;
    out vec3 gNormal;
    out vec2 gUv;
    noperspective out vec3 gEdgeDistance;
    uniform mat4 viewptM;
    vec3 ViewPoint(int i) {
        return vec3(viewptM*(gl_in[i].gl_Position/gl_in[i].gl_Position.w));
    }
    void main() {
        float ha = 0, hb = 0, hc = 0;
        // transform each vertex into viewport space
        vec3 p0 = ViewPoint(0), p1 = ViewPoint(1), p2 = ViewPoint(2);
        // find altitudes ha, hb, hc
        float a = length(p2-p1), b = length(p2-p0), c = length(p1-p0);
        float alpha = acos((b*b+c*c-a*a)/(2.*b*c));
        float beta = acos((a*a+c*c-b*b)/(2.*a*c));
        ha = abs(c*sin(beta));
        hb = abs(c*sin(alpha));
        hc = abs(b*sin(alpha));
        // send triangle vertices and edge distances
        vec3 edgeDists[3] = { vec3(ha, 0, 0), vec3(0, hb, 0), vec3(0, 0, hc) };
        for (int i = 0; i < 3; i++) {
            gEdgeDistance = edgeDists[i];
            gPoint = tePoint[i];
            gNormal = teNormal[i];
            gUv = teUv[i];
            gl_Position = gl_in[i].gl_Position;
            EmitVertex();
        }
        EndPrimitive();
    }
)";

// pixel shader
const char *pShaderCode = R"(
    #version 410 core
    in vec3 gPoint;
    in vec3 gNormal;
    in vec2 gUv;
    noperspective in vec3 gEdgeDistance;
    uniform float leftEdge = 0;
    uniform float rightEdge = 1;
    uniform float topEdge = 0;
    uniform float bottomEdge = 1;
    uniform int nlights;
    uniform vec3 lights[20];
    uniform vec3 lightColors[20];
    uniform float displaceScale = 0;
    uniform int disableBumpMap = 0;
    uniform int disableTextureMap = 0;
    uniform int disableDiffuse = 0;
    uniform int disableSpecular = 0;
    uniform int oneSided = 1;
    uniform int hiliteColor = 1;
    uniform sampler2D textureImage;
    uniform sampler2D bumpMap;
    uniform vec4 outlineColor = vec4(0, 0, 0, 1);
    uniform float outlineWidth = 1;
    uniform float transition = 1;
    uniform int outlineOn = 1;
    out vec4 pColor;
    vec3 TransformToLocal(vec3 v, vec3 x, vec3 y, vec3 z) {
        float xx = v.x*x.x + v.y*y.x + v.z*z.x;
        float yy = v.x*x.y + v.y*y.y + v.z*z.y;
        float zz = v.x*x.z + v.y*y.z + v.z*z.z;
        return normalize(vec3(xx, yy, zz));
    }
    vec3 BumpNormal(vec2 uv) {
        vec4 bumpV = texture(bumpMap, uv);
        // map red, grn to [-1,1], blu to [0,1]
        vec3 b = vec3(2*bumpV.r-1, 2*bumpV.g-1, bumpV.b);
        // b.z = b.z / displaceScale;
        return b;
    }
    void main() {
        // shading issues .... compare 20-Example-BallTessQuad.cpp
        vec3 E = -normalize(gPoint);    // !!!
        vec3 N = -normalize(gNormal);   // !!!
        float d = 0, s = 0;
        vec3 specColor = vec3(0);
        vec2 uv = vec2(leftEdge+gUv.s*(rightEdge-leftEdge), topEdge+gUv.t*(bottomEdge-topEdge));
        vec3 texColor = disableTextureMap == 0? texture(textureImage, uv).rgb : vec3(1);
        for (int i = 0; i < nlights; i++) {
            vec3 L = normalize(lights[i]-gPoint);
            // which side of plane def. by point & normal are eye, light?
//          bool sideLight = dot(L, N) < 0;
//          bool sideViewer = dot(E, N) < 0;
            bool sideLight = (dot(N, L)-dot(gPoint, N)) < 0;
            bool sideViewer = N.z < 0;
            if (oneSided == 0 || sideLight == sideViewer) {
                if (disableBumpMap == 0) {
                    vec2 du = dFdy(uv), dv = dFdx(uv);
                    vec3 dx = dFdy(gPoint), dy = dFdx(gPoint);
                    vec3 x = normalize(du.x*dx+du.y*dy);
                    vec3 y = normalize(dv.x*dx+dv.y*dy);
                    vec3 n = BumpNormal(uv);
                    vec3 nn = TransformToLocal(n, x, y, N);
                    N = nn;
                }
                if (disableDiffuse == 0)
                    d += oneSided == 1? max(0, dot(N, L)) : abs(dot(N, L));
                if (disableSpecular == 0) {
                    vec3 R = reflect(L, N);         // highlight vector
                    float h = max(0, dot(R, -E));   // highlight term (perplexed by -E)
                    float ss = pow(h, 100);         // specular term
                    if (hiliteColor == 1) {
                        specColor.r += ss*lightColors[i].r*texColor.r;
                        specColor.g += ss*lightColors[i].g*texColor.g;
                        specColor.b += ss*lightColors[i].b*texColor.b;
                        // specColor += ss*lightColors[i];
                        s += ss;                    // accumulated specular term
                    }
                }
            }
        }
        float a = .1f, ad = clamp(a+d, 0, 1);
        if (hiliteColor == 0)
            specColor = s*texColor;
        pColor = vec4(ad*texColor+specColor, 1);
        if (outlineOn > 0) {
            float minDist = min(gEdgeDistance.x, gEdgeDistance.y);
            minDist = min(minDist, gEdgeDistance.z);
            float t = smoothstep(outlineWidth-transition, outlineWidth+transition, minDist);
            if (outlineOn == 2) pColor = vec4(1,1,1,1);
            pColor = mix(outlineColor, pColor, t);
        }
    }
)";

// shaders for image display

const char *imageVShader = R"(
    #version 330
    out vec2 vUv;
    void main() {
        vec2 pt[] = vec2[4](vec2(-1,-1), vec2(-1,1), vec2(1,1), vec2(1,-1));
        vec2 uv[] = vec2[4](vec2(0,0), vec2(0,1), vec2(1,1), vec2(1,0));
        vUv = uv[gl_VertexID];
        gl_Position = vec4(pt[gl_VertexID], 0, 1);
    }
)";

const char *imagePShader = R"(
    #version 330
    in vec2 vUv;
    uniform sampler2D textureImage;
    uniform sampler2D bumpMap;
    uniform sampler2D heightField;
    uniform int view;
    out vec4 pColor;
    void main() {
        if (view == 1)
            pColor = vec4(texture(textureImage, vUv).rgb, 1);
        if (view == 2)
            pColor = vec4(texture(bumpMap, vUv).rgb, 1);
        if (view == 3)
            pColor = vec4(texture(heightField, vUv).rgb, 1);
    }
)";

// Texture Warp

void Warp() {
    int width, height;
    unsigned char *pixels = ReadTarga(stripesFilename, width, height);
    int bytesPerPixel = 3, bytesPerImage = width*height*bytesPerPixel;
    unsigned char *warp = new unsigned char[bytesPerImage], *w = warp;
    for (int j = 0; j < height; j++) {
        float h = (float) j / (float) (height-1), a = (h-.5f)*3.141592f, c = cos(a);
        for (int i = 0; i < width; i++) {
            float f = (((float)i/(float)(width-1))-.5f)*c, x = .5f+f;
            int ix = (int) (x*(width-1));
            unsigned char *p = pixels+bytesPerPixel*(j*width+ix);
            for (int k = 0; k < bytesPerPixel; k++)
                *w++ = *p++;
        }
    }
    warpTextureName = LoadTexture(warp, width, height, warpTextureUnit, true, true);
    delete [] pixels;
    delete [] warp;
}

// Display

vec3 background(1, 1, 1);

void Display(GLFWwindow *w) {
    // background, blending, zbuffer
    glClearColor(background.x, background.y, background.z, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_NICEST);
    // setup scene, shader
    Scene &s = scenes[scene]; // scene = 0: ball, 1: tube, 2, 3, 4: quad
    GLuint shader = s.view == 0? shapeShader : imageShader;
    glUseProgram(shader);
    // texture, bump maps
    int tUnit = s.stripesTexture? (s.warpTexture? warpTextureUnit : stripesTextureUnit) : s.textureUnit;
    int tName = s.stripesTexture? (s.warpTexture? warpTextureName : stripesTextureName) : s.textureName;
    glActiveTexture(GL_TEXTURE0+tUnit);
    glBindTexture(GL_TEXTURE_2D, tName);
    SetUniform(shader, "textureImage", tUnit);
    glActiveTexture(GL_TEXTURE0+s.bumpUnit);
    glBindTexture(GL_TEXTURE_2D, s.bumpName);
    SetUniform(shader, "bumpMap", s.bumpUnit);
    // height field
    glActiveTexture(GL_TEXTURE0+s.depthUnit);
    glBindTexture(GL_TEXTURE_2D, s.depthName);
    SetUniform(shader, "heightField", s.depthUnit);
    if (s.view == 0) {
        SetUniform(shader, "modelview", camera.modelview);
        SetUniform(shader, "persp", camera.persp);
        SetUniform(shader, "shape", scene > 2? 2 : scene);
        // HLE params
        SetUniform(shader, "outlineOn", s.showLines);
        SetUniform(shader, "outlineWidth", s.lineWidth);
        SetUniform(shader, "transition", s.outlineTransition);
        // texture scaling
        SetUniform(shader, "leftEdge", leftEdge);
        SetUniform(shader, "rightEdge", rightEdge);
        SetUniform(shader, "topEdge", topEdge);
        SetUniform(shader, "bottomEdge", bottomEdge);
        // displacement/bump params
        SetUniform(shader, "displaceScale", s.displaceScale);
        SetUniform(shader, "disableBumpMap", s.disableBumpMap);
        // shading params
        SetUniform(shader, "hiliteColor", s.hiliteColor);
        SetUniform(shader, "disableTextureMap", s.disableTextureMap);
        SetUniform(shader, "disableDiffuse", s.disableDiffuse);
        SetUniform(shader, "disableSpecular", s.disableSpecular);
        SetUniform(shader, "approxNormals", s.approxNormals);
        SetUniform(shader, "oneSided", s.oneSided);
        // update matrices
        SetUniform(shader, "viewptM", Viewport());
        // transform lights and send
        int nlights = lights.size();
        std::vector<vec3> xLights(nlights), lColors(nlights);
        for (int i = 0; i < nlights; i++) {
            Light l = lights[i];
            vec4 h = camera.modelview*vec4(l.p, 1);
            xLights[i] = vec3(h.x, h.y, h.z);
            lColors[i] = colors[l.cid];
        }
        SetUniform(shader, "nlights", nlights);
        glUniform3fv(glGetUniformLocation(shader, "lights"), nlights, (float *) &xLights[0]);
        glUniform3fv(glGetUniformLocation(shader, "lightColors"), nlights, (float *) &lColors[0]);
        // LOD params
        float r = (float) s.res;
        float outerLevels[] = {r, r, r, r}, innerLevels[] = {r, r};
        glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outerLevels);
        glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, innerLevels);
        // draw patch at uber resolution
        glPatchParameteri(GL_PATCH_VERTICES, 4);
        SetUniform(shader, "uvScale", 1.f/s.uberRes);
        for (int i = 0; i < s.uberRes; i++)
            for (int j = 0; j < s.uberRes; j++) {
                SetUniform(shader, "uvOffset", 1.f/s.uberRes*vec2(i, j));
                glDrawArrays(GL_PATCHES, 0, 4);
            }
        glDisable(GL_DEPTH_TEST);
        // draw light source
        if ((clock()-mouseMove)/CLOCKS_PER_SEC < .9f) {
            UseDrawShader(camera.fullview);
            for (size_t i = 0; i < lights.size(); i++) {
                Light &l = lights[i];
                int dia = hover == &lights[i]? 18 : 12;
                bool visible = IsVisible(l.p, camera.fullview);
                Disk(l.p, dia+3,  visible? vec3(0,0,0) : vec3(1,1,1));
                Disk(l.p, dia,  visible? colors[l.cid] : vec3(0,0,0));
            }
        }
    }
    if (s.view > 0) {
        SetUniform(shader, "view", s.view);
        glDrawArrays(GL_QUADS, 0, 4);
    }
    // text
    if (showText) {
        vec3 p0 = lights[0].p;
        UseDrawShader(ScreenMode());
        glDisable(GL_DEPTH_TEST);
        Quad(vec3(0,0,0), vec3(0,150,0), vec3(330,150,0), vec3(330,0,0), true, vec3(1,1,1));
        if (showLightInfo) {
            int nlights = lights.size(), textHeight = 10+20*(nlights-1);
            for (int i = 0; i < nlights; i++) {
                Light &l = lights[i];
                Text(10, textHeight-20*i, vec3(0,0,0), 8, "light[%i]: p=(%3.2f,%3.2f,%3.2f), cid=%i", i, l.p.x, l.p.y, l.p.z, l.cid);
            }
        }
        else {
            Text(10, 130, vec3(0,0,0), 8, "left %3.2f, right %3.2f, top %3.2f, bottom %3.2f", leftEdge, rightEdge, topEdge, bottomEdge);
            Text(10, 110, vec3(0,0,0), 8, "scene %i, light[0]=(%3.2f,%3.2f,%3.2f)", scene, p0.x, p0.y, p0.z);
            Text(10, 90, vec3(0,0,0), 8, "res %i, %i quads, %i triangles", s.res, s.uberRes, 2*s.res*s.res*s.uberRes*s.uberRes);
            Text(10, 70, vec3(0,0,0), 8, "displace scale: %4.3f, pixel scale: %4.3f", s.displaceScale, s.pixelScale);
            Text(10, 50, vec3(0,0,0), 8, "texture %s, bump %s", s.disableTextureMap? "disabled" : "enabled", s.disableBumpMap? "disabled" : "enabled");
            Text(10, 30, vec3(0,0,0), 8, "diffuse %s, specular %s", s.disableDiffuse? "off" : "on", s.disableSpecular? "off" : "on");
            Text(10, 10, vec3(0,0,0), 8, "approx normals: %s", s.approxNormals? "on" : "off");
        }
    }
    glFlush();
}

// Mouse

int WindowHeight(GLFWwindow *w) {
    int width, height;
    glfwGetWindowSize(w, &width, &height);
    return height;
}

bool Shift(GLFWwindow *w) {
    return glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
           glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
}

void GetRay(int x, int y, vec3 &p, vec3 &v) {
    vec3 p1, p2;
    ScreenLine((float) x, (float) y, camera.modelview, camera.persp, p1, p2);
    v = normalize(p2-(p = p1));
}

void MouseButton(GLFWwindow *w, int butn, int action, int mods) {
    double x, y;
    glfwGetCursorPos(w, &x, &y);
    y = WindowHeight(w)-y;
    if (action == GLFW_RELEASE)
        camera.MouseUp();
    hover = picked = NULL;
    bool ctrl = mods & GLFW_MOD_CONTROL;
    vec3 p, v;
    if (ctrl && butn == GLFW_MOUSE_BUTTON_LEFT && scene > 1) {
        GetRay(x, y, p, v);
    }
    if (action == GLFW_RELEASE && butn == GLFW_MOUSE_BUTTON_RIGHT) {
        GetRay(x, y, p, v);
        float alpha = RaySphere(p, v, vec3(0,0,0), .5f);
        camera.SetRotateCenter(alpha < 0? vec3(0,0,0) : p+alpha*v);
    }
    if (action == GLFW_PRESS && butn == GLFW_MOUSE_BUTTON_LEFT) {
        light = NULL;
        for (size_t i = 0; i < lights.size(); i++)
            if (MouseOver(x, y, lights[i].p, camera.fullview, xCursorOffset, yCursorOffset)) {
                picked = &lights;
                light = &lights[i];
                mover.Down(&lights[i].p, x, y, camera.modelview, camera.persp);
            }
        if (picked == NULL) {
            picked = &camera;
            camera.MouseDown(x, y);
        }
    }
}

void MouseMove(GLFWwindow *w, double x, double y) {
    mouseMove = clock();
    y = WindowHeight(w)-y;
    if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        if (picked == &lights)
            mover.Drag(x, y, camera.modelview, camera.persp);
        if (picked == &camera)
            camera.MouseDrag(x, y, Shift(w));
    }
    else {
        hover = NULL;
        for (size_t i = 0; i < lights.size(); i++)
            if (MouseOver(x, y, lights[i].p, camera.fullview, xCursorOffset, yCursorOffset))
                hover = (void *) &lights[i];
    }
}

void MouseWheel(GLFWwindow *w, double xoffset, double yoffset) { camera.MouseWheel(yoffset, Shift(w)); }

// Application

void Resize(GLFWwindow *w, int width, int height) {
    camera.Resize(width, height);
    glViewport(0, 0, width, height);
}

void Keyboard(GLFWwindow *w, int k, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        bool shift = mods & GLFW_MOD_SHIFT;
        Scene &s = scenes[scene];
        int nlights = lights.size();
        float inc = shift? -.01f : .01f;
        switch (k) {
            case GLFW_KEY_LEFT:  leftEdge  += inc;                  break;
            case GLFW_KEY_RIGHT: rightEdge += inc;                  break;
            case GLFW_KEY_UP:    topEdge += inc;                    break;
            case GLFW_KEY_DOWN:  bottomEdge += inc;                 break;
            case 'S': scene = (scene+1)%nScenes;                    break;
            case 'V': s.view = (s.view+1)%4;                        break;
            case 'G': s.hiliteColor = 1-s.hiliteColor;              break;
            case 'B': s.disableBumpMap = 1-s.disableBumpMap;        break;
            case 'E': s.disableTextureMap = 1-s.disableTextureMap;  break;
            case 'F': s.disableDiffuse = 1-s.disableDiffuse;        break;
            case 'X': s.disableSpecular = 1-s.disableSpecular;      break;
            case 'N': s.approxNormals = 1-s.approxNormals;          break;
            case '1': s.oneSided = 1-s.oneSided;                    break;
            case 'D': s.displaceScale += shift? -.01f : .01f;       break;
            case 'T': s.outlineTransition *= (shift? .8f : 1.2f);   break;
            case 'W': s.lineWidth *= (shift? .8f : 1.2f);           break;
            case 'L': s.showLines = (s.showLines+1)%3;              break;
            case 'U': shift? s.ResetDepth() : s.UpdateBlur();       break;
            case 'J': showText = !showText;                         break;
            case 'K': showLightInfo = !showLightInfo;               break;
            case 'O': s.stripesTexture = !s.stripesTexture;         break;
            case 'Q': s.warpTexture = !s.warpTexture;               break;
            case 'C':
                if (light) {
                    Light *l = (Light *) light;
                    l->cid = (l->cid+1) % 8;
                }
                break;
            case 'A':
                {
                    Light *l = &lights[0];
                    if (light != NULL)
                        l = (Light *) light;
                    if (shift) {
                        int id = (l-&lights[0]);
                        lights.erase(lights.begin()+id);
                        light = NULL;
                    }
                    else {
                        Light ll = *l;
                        lights.resize(nlights+1);
                        lights[nlights] = Light(ll.p, (ll.cid+1) % 8);
                        light = &lights[nlights];
                    }
                }
                break;
            case'R':
                s.res += shift? -1 : 1;
                s.res = s.res < 1? 1 : s.res;
                break;
            case 'P':
                s.pixelScale *= Shift(w)? .8f : 1.5f;
                s.pixelScale = s.pixelScale < 1? 1 : s.pixelScale > 500? 500 : s.pixelScale;
                s.UpdateBumps();
                break;
            case 'H':
                s.uberRes += shift? -1 : 1;
                s.uberRes = s.uberRes < 1? 1 : s.uberRes;
                break;
            case 'Z':
                background = background.x < .7f? vec3(1,1,1) : vec3(.6f, .6f, .6f);
                break;
            default: break;
        }
    }
    if (scene == 2) {
        topEdge = -.02f;
        bottomEdge = .9f;
    }
    else {
        leftEdge = topEdge = 0;
        rightEdge = bottomEdge = 1;
    }
}

// taken: a, b, c, d, e, f, g, h, j, k, l, n, p, r, s, t, u, v, w, x, 1
// free: i, m, q, y, 2+
const char *usage = "\
    general:\n\
      S: cycle scene\n\
      J: toggle text box\n\
      K: toggle show light info\n\
      C: change light color\n\
      a/A: add/del light\n\
    per scene:\n\
      V: cycle scene/tex/nrml/bump\n\
      L: lines-off/lines-on/HLE\n\
      t/T: +/- line transition\n\
      w/W: +/- line width\n\
      p/P: +/- pixelScale\n\
      d/D: +/- depth\n\
      r/R: +/- res\n\
      h/H: +/- ubeRes\n\
      G: toggle hiLite color\n\
      B: toggle bump map\n\
      E: toggle texture map\n\
      F: toggle diffuse\n\
      X: toggle specular\n\
      N: toggle approx normal\n\
      O: toggle stripes texture\n\
      Z: toggle gray/white background\n\
      1: one/two sided\n\
      U: blur\n\
      Q: warp texture\n\
";

int main(int argc, char **argv) {
    // init app window and GL context
    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 4); // anti-alias
    GLFWwindow *w = glfwCreateWindow(winWidth, winHeight, "Displaced/Bumped/Textured Sphere/Tube/Height", NULL, NULL);
    glfwSetWindowPos(w, 100, 100);
    glfwMakeContextCurrent(w);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    // build, use shader programs
    imageShader = LinkProgramViaCode(&imageVShader, &imagePShader);
    shapeShader = LinkProgramViaCode(&vShaderCode, NULL, &teShaderCode, &gShaderCode, &pShaderCode);
    // init scenes
    std::string dir = "C:/Users/jules/CodeBlocks/Aids/";
    scenes[0].Init(0, dir+"EarthHeight.tga", dir+"Earth.tga");
    scenes[1].Init(1, dir+"BarkHeight.tga", dir+"UsFlag.tga");
    scenes[2].Init(2, dir+"YosemiteHeight.tga", dir+"YosemiteValley.tga"); // Labelled.tga");
    scenes[3].Init(3, dir+"PennyDepth.tga", dir+"PennyTexture.tga");
    scenes[4].Init(4, dir+"RipplesDepth.tga", dir+"Gray.tga");
    stripesTextureName = LoadTexture(stripesFilename, stripesTextureUnit);
    Warp();
    glfwSetCursorPosCallback(w, MouseMove);
    glfwSetMouseButtonCallback(w, MouseButton);
    glfwSetScrollCallback(w, MouseWheel);
    glfwSetKeyCallback(w, Keyboard);
    glfwSetWindowSizeCallback(w, Resize);
    printf("Usage:\n%s\n", usage);
    // event loop
    glfwSwapInterval(1);
    while (!glfwWindowShouldClose(w)) {
        glfwPollEvents();
        Display(w);
        glfwSwapBuffers(w);
        glUseProgram(0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glfwDestroyWindow(w);
    glfwTerminate();
}

//  if (view == 1) {
//      glDisable(GL_DEPTH_TEST);
//      UseDrawShader(camera.fullview);
//      DrawSphere();
//  }
/* void DrawPoint(float u, float v) {
    float _PI = 3.141592;
    float elevation = -_PI/2+_PI*v;
    float eFactor = cos(elevation);
    float y = sin(elevation);
    float angle = 2*_PI*(1-u);
    float x = eFactor*cos(angle), z = eFactor*sin(angle);
    vec3 n(x, y, z), p = .5f*n;
    vec3 ua = normalize(vec3(-p.z, 0, p.x));
    vec3 va = normalize(cross(n, ua));
    ArrowV(p, .03f*n, camera.fullview, vec3(1,0,0), 2, 4);
    ArrowV(p, .03f*ua, camera.fullview, vec3(0,1,0), 1, 4);
    ArrowV(p, .03f*va, camera.fullview, vec3(0,0,1), 1, 4);
    Disk(p, 7, vec3(0,0,0));
}

void DrawSphere() {
    Scene &s = scenes[scene];
    float d = 1.f/(float) s.res;
    for (float u = 0; u <= 1; u += d)
        for (float v = 0; v <= 1.01f; v += d)
            DrawPoint(u, v);
} */
