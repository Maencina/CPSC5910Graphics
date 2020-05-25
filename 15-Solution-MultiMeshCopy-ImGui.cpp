// MultiMesh.cpp - display and manipulate multiple OBJ meshes
// (c) Jules Bloomenthal 2019, all rights reserved. Commercial use requires license.


#include <glad.h>
//#include <GLFW/glfw3.h>
#include <time.h>
#include "CameraArcball.h"
#include "Draw.h"
#include "GLXtras.h"
#include "Mesh.h"
#include "Misc.h"
#include "Widgets.h"
#include <stdio.h>
#include "Draw.h"
#include "Quaternion.h"
#include "GL/glut.h"
//#include <AntTweakBar.h>
#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
// Stuff above works -- testing below
//#include <imgui_impl_glfw_gl3.h>
//#include "thread"
#include <chrono>

//// About Desktop OpenGL function loaders:
////  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
////  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
////  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
//#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
//#include <GL/gl3w.h>            // Initialize with gl3wInit()
//#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
//#include <GL/glew.h>            // Initialize with glewInit()
//#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
//#include <glad/glad.h>          // Initialize with gladLoadGL()
//#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
//#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
//#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
//#include <glbinding/gl/gl.h>
//using namespace gl;
//#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
//#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
//#include <glbinding/glbinding.h>// Initialize with glbinding::initialize()
//#include <glbinding/gl/gl.h>
//using namespace gl;
//#else
//#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
//#endif
#include <GL/gl3w.h> 
#include <GLFW/glfw3.h>


static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

using std::vector;
using std::string;
using namespace std;

// display
GLuint      shader = 0;
int         winW = 1650, winH = 800;
CameraAB    camera(0, 0, winW, winH, vec3(0,0,0), vec3(0,0,-5));

// interaction
int         xCursorOffset = 0, yCursorOffset = 0;
vec3        light(-.2f, .4f, .3f);
vec3        light2(-.3f, .3f, .1f);

Framer      framer;     // to position/orient individual mesh
Mover       mover;      // to position light
void       *picked = &camera;


// Mesh Class

class Mesh {
public:
    Mesh();
    int id,id2, id3, id4, id5, id6, id7, id8, id9, id10, id11;
    string filename;
    // vertices and triangles
    vector<vec3> points;
    vector<vec3> normals;
    vector<vec2> uvs;
    vector<int3> triangles;
    // object to world space
    mat4 xform;
    // GPU vertex buffer and texture
    GLuint vBufferId, textureId, textureId2, textureId3, textureId4, textureId5;
    GLuint textureId6, textureId7, textureId8, textureId9, textureId10, textureId11;
    // operations
    void Buffer();
    void Draw();
    bool Read(int id, char *fileame, mat4 *m = NULL);
        // read in object file (with normals, uvs) and texture map, initialize matrix, build vertex buffer
};



// Shaders

const char *vertexShader = R"(
    #version 130
    in vec3 point;
    in vec3 normal;
    in vec2 uv;
    out vec3 vPoint;
    out vec3 vNormal;
    out vec2 vUv;
    uniform mat4 modelview;
    uniform mat4 persp;
    uniform int current_time;
    uniform bool move_guitar_updown;
    void main() {
        vPoint = (modelview*vec4(point, 1)).xyz;
        if (move_guitar_updown)
        {        
            vPoint.y += 0.5*sin(current_time);
        }
        vNormal = (modelview*vec4(normal, 0)).xyz;
        gl_Position = persp*vec4(vPoint, 1);
        vUv = uv;
    }
)";

const char *pixelShader = R"(
    #version 130
    in vec3 vPoint;
    in vec3 vNormal;
    in vec2 vUv;
    out vec4 pColor;
    uniform vec3 light;
    uniform vec3 light2;
    //uniform vec3 lightDir;
    uniform sampler2D Albedo_Map;
    uniform sampler2D Normal_Map;
    uniform sampler2D AO_Map;
    uniform sampler2D Metallic_Map;
    uniform sampler2D Roughness_Map;
    uniform mat4 modelview;
    uniform bool use_albedo_body;
    
    // Spot Lights
    uniform bool spot_light1;

    // Difusse
    uniform bool show_lambert_model;
    uniform bool show_disney_model;
    uniform bool show_orennayar_model;

    // Specular
    uniform bool show_blinnphong_model;
    uniform bool show_cooktorrance_model;

    float F_Schlick(float VoH, float f0, float f90) 
    {
        return f0 + (f90 - f0) * pow(1.0 - VoH, 5.0);
    }

    float disneyDiffuse(float NoE, float NoL, float LoH, float f0, float roughness)
    {
        float PI = 3.1652;
        float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
        float lightScatter = F_Schlick(NoL, 1.0, f90);
        float viewScatter = F_Schlick(NoE, 1.0, f90);
        return lightScatter * viewScatter * (1.0 / PI);
    }

    vec3 GeometryFunction(float NoV, float NoL, vec3 a) 
    {
                vec3 a2 = a * a;
                vec3 GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
                vec3 GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
                return 0.5 / (GGXV + GGXL);
     }

     vec3 DistributionFunction(float NoH, vec3 a) //D_GGX_Float3
     {
                float PI = 3.141592;
                vec3 a2 = a * a;
                vec3 f = (NoH * a2 - NoH) * NoH + 1.0;
                return a2 / (PI * f * f);
     }

    

    void main() {
     
        //Textures Files
        //Extension 19
        vec3 Normal = texture(Normal_Map, vec2(vUv.x,vUv.y)).rgb;
        vec3 AOTexture = texture(AO_Map, vec2(vUv.x,vUv.y)).rgb;
        vec3 albedo;        

        if (use_albedo_body)
        {
            albedo = texture(Albedo_Map, vec2(vUv.x,1-vUv.y)).rgb;
        }
        else
        {
            //test
            albedo = texture(Albedo_Map, vec2(vUv.x,vUv.y)).rgb;
            //EOT
        }
        
        vec3 metallicTexture = texture(Metallic_Map, vec2(vUv.x,vUv.y)).rgb;
        vec3 roughnessTexture = texture(Roughness_Map, vec2(vUv.x,vUv.y)).rgb;
        
        //Constants
        float PI = 3.1415;
    
        //Information from the Vertex Shader
        vec3 N = normalize(vNormal);       // surface normal
        vec3 L = normalize(light-vPoint);  // light vector
        vec3 L1 = normalize(light2-vPoint);  // light vector2
        vec3 E = normalize(vPoint);        // eye vector
        
        
        //Directional Light
        vec3 lightDir = vec3(0.0, 0.0, 1.0);
        L = lightDir; 
        vec3 lightDirColor = vec3(1, 1, 1);  
        float LightDirIntensity = 3.0;     

        //Dots + Half Vector - Directional Light
        vec3 R = reflect(L, N);
        vec3 H = normalize(E + L); //Half Vector            
        float NoL = clamp(dot(N, L), 0.0, 1.0);                   
        float NoE = abs(dot(N, E));
        float NoH = clamp(dot(N, H), 0.0,1.0);
        float LoH = max(dot(L, H),0.0);

        //Point Light 1
        vec3 lightPoint1Color = vec3(1, 1, 1);  
        float LightPoint1Intensity = 3.0; 

        //Dots + Half Vector - Point Light1
        vec3 R1 = reflect(L1, N);
        vec3 H1 = normalize(E + L1); //Half Vector            
        float NoL1 = clamp(dot(N, L1), 0.0, 1.0);                 
        float NoH1 = clamp(dot(N, H1), 0.0,1.0);
        float LoH1 = max(dot(L1, H1),0.0);
        
        //f0
        float reflectance = 0.5;
        vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallicTexture) + albedo * metallicTexture;
        

        //Diffuse
        //Lambert
        float Lambert = 1/PI;
        //Disney
        float Disney = disneyDiffuse(NoE, NoL, LoH, f0.r, roughnessTexture.r);

        //Disney1
        float Disney1 = disneyDiffuse(NoE, NoL1, LoH1, f0.r, roughnessTexture.r);

        vec3 diffuse = vec3(1,0,0);
        vec3 diffuse1 = vec3(1,0,0);
        vec3 specular = vec3(1,0,0);
        vec3 specular1 = vec3(1,0,0);
        
    
        //Specular
        //Blinn Phong - Directional Light
        float Blinn = pow(NoH, 70);

        //Blinn Phong - Point Light 1
        float Blinn1 = pow(NoH1, 70);

        // Cook Torrance - Directional Light
        float f90 = 0.5 + 2.0 * roughnessTexture.r * LoH * LoH;
        vec3 D = DistributionFunction(NoH, roughnessTexture);
        vec3 G = GeometryFunction(NoE, NoL, roughnessTexture);
        float F = F_Schlick(NoL, 1.0, f90);
        vec3 CookTorrance = (D * F * G);

        // Cook Torrance - Point Light 1
        float f901 = 0.5 + 2.0 * roughnessTexture.r * LoH1 * LoH1;
        vec3 D1 = DistributionFunction(NoH1, roughnessTexture);
        vec3 G1 = GeometryFunction(NoE, NoL1, roughnessTexture);
        float F1 = F_Schlick(NoL1, 1.0, f901);
        vec3 CookTorrance1 = (D1 * F1 * G1);
        
        
        //Jules Code
        //Light Intensity
        //float d = NoL;
        //float s = RoE;
        //float intensity = clamp(d+pow(s, 50), 0, 1);
       
        // Test
        if (spot_light1)
        {}
        
        if (show_lambert_model && show_blinnphong_model)
        {
           diffuse = vec3(LightDirIntensity * albedo.rgb * Lambert * lightDirColor); 
           diffuse1 = vec3(LightPoint1Intensity * albedo.rgb * Lambert * lightPoint1Color);

            vec3 specular = vec3(Blinn, Blinn, Blinn);
            vec3 specular1 = vec3(Blinn1, Blinn1, Blinn1);

            vec3 direct = (diffuse + specular) * NoL;
            vec3 direct1 = (diffuse1 + specular1) * NoL1;
            pColor = vec4(direct + direct1, 1);
        }

        else if (show_disney_model && show_blinnphong_model)
        {
            diffuse = vec3(LightDirIntensity * albedo.rgb * Disney * lightDirColor);
            diffuse1 = vec3(LightPoint1Intensity * albedo.rgb * Disney1 * lightPoint1Color);

            specular = vec3(Blinn, Blinn, Blinn);
            specular1 = vec3(Blinn1, Blinn1, Blinn1);

            vec3 direct = (diffuse + specular) * NoL;
            vec3 direct1 = (diffuse1 + specular1) * NoL1;

            pColor = vec4(direct + direct1, 1);
        }
        else if (show_orennayar_model && show_blinnphong_model)
        {
            pColor = vec4(1,0,0,1);
        }
        else if (show_lambert_model && show_cooktorrance_model)
        {
           diffuse = vec3(LightDirIntensity * albedo.rgb * Lambert * lightDirColor); 
           diffuse1 = vec3(LightPoint1Intensity * albedo.rgb * Lambert * lightPoint1Color);

            vec3 specular = CookTorrance;
            vec3 specular1 = CookTorrance1;

            vec3 direct = (diffuse + specular) * NoL;
            vec3 direct1 = (diffuse1 + specular1) * NoL1;
            pColor = vec4(direct + direct1, 1);
        }
        else
        {
            pColor = vec4(1,0,0,1);
        }

    }
)";

// Scene

const char  *sceneFilename = "Test.scene";
const char  *directory = "./";
//Mauricio
const char  *defaultNames[] = {"HousePlant", "Rose", "Cat", "Cerberus"};
vector<Mesh> meshes;

void NewMesh(char *filename, mat4 *m) {
    int nmeshes = meshes.size();
    meshes.resize(nmeshes+1);
    Mesh &mesh = meshes[nmeshes];
    if (!mesh.Read(nmeshes, filename, m))
        meshes.resize(nmeshes);
}

void WriteMatrix(FILE *file, mat4 &m) {
    fprintf(file, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n",
        m[0][0], m[0][1], m[0][2], m[0][3],
        m[1][0], m[1][1], m[1][2], m[1][3],
        m[2][0], m[2][1], m[2][2], m[2][3],
        m[3][0], m[3][1], m[3][2], m[3][3]);
}

bool ReadMatrix(FILE *file, mat4 &m) {
    char buf[500];
    if (fgets(buf, 500, file) == NULL)
        return false;
    int nitems = sscanf(buf, "%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f",
        &m[0][0], &m[0][1], &m[0][2], &m[0][3],
        &m[1][0], &m[1][1], &m[1][2], &m[1][3],
        &m[2][0], &m[2][1], &m[2][2], &m[2][3],
        &m[3][0], &m[3][1], &m[3][2], &m[3][3]);
    return nitems == 16;
}

void SaveScene() {
    FILE *file = fopen(sceneFilename, "w");
    if (!file) {
        printf("Can't write %s\n", sceneFilename);
        return;
    }
    WriteMatrix(file, camera.modelview);
    for (size_t i = 0; i < meshes.size(); i++) {
        Mesh &mesh = meshes[i];
        fprintf(file, "%s\n", mesh.filename.c_str());
        WriteMatrix(file, mesh.xform);
    }
    fclose(file);
    printf("Saved %i meshes\n", meshes.size());
}

bool ReadScene(const char *filename) {
    char meshName[500];
    FILE *file = fopen(filename, "r");
    if (!file)
        return false;
    mat4 mv;
    if (!ReadMatrix(file, mv)) {
        printf("Can't read modelview\n");
        return false;
    }
    camera.SetModelview(mv);
    meshes.resize(0);
    while (fgets(meshName, 500, file) != NULL) {
        meshName[strlen(meshName)-1] = 0; // remove carriage-return
        mat4 m;
        if (!ReadMatrix(file, m)) {
            printf("Can't read matrix\n");
            return false;
        }
        NewMesh(meshName, &m);
    }
    fclose(file);
    return true;
}

void ListScene() {
    int nmeshes = meshes.size();
    printf("%i meshes:\n", nmeshes);
    for (int i = 0; i < nmeshes; i++)
        printf("  %i: %s\n", i, meshes[i].filename.c_str());
}

void DeleteMesh() {
    char buf[500];
    int n = -1;
    printf("delete mesh number: ");
    gets_s(buf);
    if (sscanf(buf, "%i", &n) == 1 && n >= 0 && n < (int) meshes.size()) {
        printf("deleted mesh[%i]\n", n);
        meshes.erase(meshes.begin()+n);
    }
}

void AddMesh() {
    char buf[500], meshName[500];
    printf("name of new mesh: ");
    gets_s(buf);
    int nMeshes = meshes.size();
    meshes.resize(nMeshes+1);
    sprintf(meshName, "%s/%s", directory, buf);
    meshes[nMeshes].Read(nMeshes, meshName);
}

// Mesh

Mesh::Mesh() {
    vBufferId = textureId = id = 0;
    textureId2 = id2 = 1;
    textureId3 = id3 = 2;
    textureId4 = id4 = 3;
    textureId5 = id5 = 4;
    textureId6 = id6 = 5;
    textureId7 = id7 = 6;
    textureId8 = id8 = 7;
    textureId9 = id9 = 8;
    textureId10 = id10 = 9;
    textureId11 = id11 = 10;
}

void Mesh::Buffer() {
    // create a vertex buffer for the mesh
    glGenBuffers(1, &vBufferId);
    glBindBuffer(GL_ARRAY_BUFFER, vBufferId);
    // allocate GPU memory for vertex locations and colors
    int sizePoints = points.size()*sizeof(vec3);
    int sizeNormals = normals.size()*sizeof(vec3);
    int sizeUvs = uvs.size()*sizeof(vec2);
    int bufferSize = sizePoints+sizeUvs+sizeNormals;
    glBufferData(GL_ARRAY_BUFFER, bufferSize, NULL, GL_STATIC_DRAW);
    // load data to buffer
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizePoints, &points[0]);
    glBufferSubData(GL_ARRAY_BUFFER, sizePoints, sizeNormals, &normals[0]);
    glBufferSubData(GL_ARRAY_BUFFER, sizePoints+sizeNormals, sizeUvs, &uvs[0]);
}

void Mesh::Draw() {
    // use vertex buffer for this mesh
    glBindBuffer(GL_ARRAY_BUFFER, vBufferId);
    // connect shader inputs to GPU buffer
    int sizePoints = points.size()*sizeof(vec3);
    int sizeNormals = normals.size()*sizeof(vec3);
    // vertex feeder
    VertexAttribPointer(shader, "point", 3, 0, (void *) 0);
    VertexAttribPointer(shader, "normal", 3, 0, (void *) sizePoints);
    VertexAttribPointer(shader, "uv", 2, 0, (void *) (sizePoints+sizeNormals));
    // set custom transform (xform = mesh transforms X view transform)
    glActiveTexture(GL_TEXTURE1+id);
    // active texture corresponds with textureUnit
    glBindTexture(GL_TEXTURE_2D, textureId);

    //Normals Map
    glActiveTexture(GL_TEXTURE1 + id2);
    glBindTexture(GL_TEXTURE_2D, textureId2);

    //AO map
    glActiveTexture(GL_TEXTURE1 + id3);
    glBindTexture(GL_TEXTURE_2D, textureId3);

    //AO map
    glActiveTexture(GL_TEXTURE1 + id4);
    glBindTexture(GL_TEXTURE_2D, textureId4);
    //AO map
    glActiveTexture(GL_TEXTURE1 + id5);
    glBindTexture(GL_TEXTURE_2D, textureId5);
    //AO map
    glActiveTexture(GL_TEXTURE1 + id6);
    glBindTexture(GL_TEXTURE_2D, textureId6);
    //AO map
    glActiveTexture(GL_TEXTURE1 + id7);
    glBindTexture(GL_TEXTURE_2D, textureId7);
    //AO map
    glActiveTexture(GL_TEXTURE1 + id8);
    glBindTexture(GL_TEXTURE_2D, textureId8);
    //AO map
    glActiveTexture(GL_TEXTURE1 + id9);
    glBindTexture(GL_TEXTURE_2D, textureId9);
    //AO map
    glActiveTexture(GL_TEXTURE1 + id10);
    glBindTexture(GL_TEXTURE_2D, textureId10);
    //AO map
    glActiveTexture(GL_TEXTURE1 + id11);
    glBindTexture(GL_TEXTURE_2D, textureId11);

    SetUniform(shader, "Albedo_Map", (int)textureId);
    SetUniform(shader, "Normal_Map", (int)textureId2);
    SetUniform(shader, "AO_Map", (int)textureId3);
    SetUniform(shader, "Metallic_Map", (int)textureId4);
    SetUniform(shader, "Roughness_Map", (int)textureId5);
    SetUniform(shader, "use_albedo_body", 1);
    // all 20 textures go here
    //SetUniform(shader, "textureImage_internal_AO", (int)textureId11);

    SetUniform(shader, "modelview", camera.modelview*xform);
    SetUniform(shader, "persp", camera.persp);
    //glDrawElements(GL_TRIANGLES, 3 * triangles.size(), GL_UNSIGNED_INT, &triangles[0]);

    glDrawElements(GL_TRIANGLES, 3 * 2664, GL_UNSIGNED_INT, &triangles[0]);

    SetUniform(shader, "Albedo_Map", (int)textureId6);
    SetUniform(shader, "Normal_Map", (int)textureId7);
    SetUniform(shader, "AO_Map", (int)textureId8);
    SetUniform(shader, "Metallic_Map", (int)textureId9);
    SetUniform(shader, "Roughness_Map", (int)textureId10);
    SetUniform(shader, "use_albedo_body", 0);

    SetUniform(shader, "modelview", camera.modelview * xform);
    SetUniform(shader, "persp", camera.persp);


    glDrawElements(GL_TRIANGLES, 3 * (triangles.size() - 2664), GL_UNSIGNED_INT, &triangles[2664]);
}

bool Mesh::Read(int mid, char *name, mat4 *m) {
    id = mid;
    filename = string(name);
    string objectFilename = filename+".obj";
    //string objectFilename =  "lespaul_details.obj";
    string textureFilename = "lespaul_Albedo.tga";
    string textureFilename2 = "lespaulnormal.tga";
    string textureFilename3 = "lespaul_19_AO.tga";
    string textureFilename4 = "lespaul_19_Metallic.tga";
    string textureFilename5 = "lespaul_19_Roughness.tga";
    string textureFilename6 = "lespaul_20_Base_Color.tga";
    string textureFilename7 = "lespaul_20_Default_Normal.tga";
    string textureFilename8 = "lespaul_20_AO.tga"; 
    string textureFilename9 = "lespaul_20_Metallic.tga";
    string textureFilename10 = "lespaul_20_Roughness.tga";
    //string textureFilename11 = "lespaul_internal_AO.tga";
    if (!ReadAsciiObj((char *) objectFilename.c_str(), points, triangles, &normals, &uvs)) {
        printf("can't read %s\n", objectFilename.c_str());
        return false;
    }
    Normalize(points, .8f);
    Buffer();
    textureId = LoadTexture((char *) textureFilename.c_str(), id);
    textureId2 = LoadTexture((char*)textureFilename2.c_str(), id2);
    textureId3 = LoadTexture((char*)textureFilename3.c_str(), id3);
    textureId4 = LoadTexture((char*)textureFilename4.c_str(), id4);
    textureId5 = LoadTexture((char*)textureFilename5.c_str(), id5);
    textureId6 = LoadTexture((char*)textureFilename6.c_str(), id6);
    textureId7 = LoadTexture((char*)textureFilename7.c_str(), id7);
    textureId8 = LoadTexture((char*)textureFilename8.c_str(), id8);
    textureId9 = LoadTexture((char*)textureFilename9.c_str(), id9);
    textureId10 = LoadTexture((char*)textureFilename10.c_str(), id10);
    //textureId11 = LoadTexture((char*)textureFilename11.c_str(), id11);
    if (m)
        xform = *m;
    framer.Set(&xform, 100, camera.persp*camera.modelview);
    return true;
}

// Display

time_t mouseMoved;

void Display() {
    // clear screen, depth test, blend
    glClearColor(.5f, .5f, .5f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glUseProgram(shader);
    // update light
    vec4 xlight = camera.modelview*vec4(light, 1);
    SetUniform3(shader, "light", (float *) &xlight.x);

    // Testing
    vec4 xlight2 = camera.modelview * vec4(light2, 1);
    SetUniform3(shader, "light2", (float*)&xlight2.x);
    // EOT

    // display objects
    for (size_t i = 0; i < meshes.size(); i++)
        meshes[i].Draw();
    // lights and frames
    if ((clock()-mouseMoved)/CLOCKS_PER_SEC < 1.f) {
        glDisable(GL_DEPTH_TEST);
        UseDrawShader(camera.fullview);
        Disk(light, 9, vec3(1,1,0));
        
        // Change the color of the dot to green
        Disk(light2, 9, vec3(0, 1, 0));
        for (size_t i = 0; i < meshes.size(); i++) {
            mat4 &f = meshes[i].xform;
            vec3 base(f[0][3], f[1][3], f[2][3]);
            Disk(base, 9, vec3(1,1,1));
        }
        if (picked == &framer)
            framer.Draw(camera.fullview);
        if (picked == &camera)
            camera.arcball.Draw(); // camera.fullview);
    }

    // Since we are using ImGui and glFlush() can be
    // called only once per cycle,
    // Call glFlush() in the main loop instead.
    //glFlush();
}

// Mouse

int WindowHeight(GLFWwindow *w) {
    int width, height;
    glfwGetWindowSize(w, &width, &height);
    return height;
}

void CorrectMouse(GLFWwindow *w, double *x, double *y) {
    *y = WindowHeight(w)-*y+yCursorOffset;
    *x += xCursorOffset;
}

bool Shift(GLFWwindow *w) {
    return glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
           glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
}

bool isMouseInMenu(int x, int y)
{
    // TODO: ImGUI crashes w/o the context
    // Do I have to pass the ImGUI context?
    // Is there a better way to do this?
    //ImVec2 winSize = ImGui::GetWindowSize();
    //ImVec2 winPos = ImGui::GetWindowPos();

    if (x <= 250 && y <= 350)
        return true;
    else
        return false;

}
void MouseButton(GLFWwindow *w, int butn, int action, int mods) {
    double x, y;
    glfwGetCursorPos(w, &x, &y);
    CorrectMouse(w, &x, &y);

    if (isMouseInMenu(x, y))
    {
        return;
    }

    if (action == GLFW_PRESS && butn == GLFW_MOUSE_BUTTON_LEFT) {
        void *newPicked = NULL;
        if (MouseOver(x, y, light, camera.fullview)) {
            newPicked = &mover;
            mover.Down(&light, x, y, camera.modelview, camera.persp);
        }
        if (MouseOver(x, y, light2, camera.fullview)) {
            newPicked = &mover;
            mover.Down(&light2, x, y, camera.modelview, camera.persp);
        }

        if (!newPicked)
            // test for mesh base hit
            for (size_t i = 0; i < meshes.size(); i++) {
                Mesh &m = meshes[i];
                vec3 base(m.xform[0][3], m.xform[1][3], m.xform[2][3]);
                if (MouseOver(x, y, base, camera.fullview)) {
                    newPicked = &framer;
                    framer.Set(&m.xform, 100, camera.fullview);
                    framer.Down(x, y, camera.modelview, camera.persp);
                }
            }
        if (!newPicked && picked == &framer && framer.Hit(x, y)) {
            framer.Down(x, y, camera.modelview, camera.persp);
            newPicked = &framer;
        }
        picked = newPicked;
        if (!picked) {
            picked = &camera;
            camera.MouseDown(x, y);
        }
    }
    if (action == GLFW_RELEASE) {
        if (picked == &camera)
            camera.MouseUp();
        if (picked == &framer)
            framer.Up();
    }
}

void MouseMove(GLFWwindow *w, double x, double y) {
    mouseMoved = clock();
    // insert code here
    if (isMouseInMenu(x, y))
    {
        return;
    }

    if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) { // drag
        y = WindowHeight(w)-y;
        if (picked == &mover)
            mover.Drag(x, y, camera.modelview, camera.persp);
        if  (picked == &framer)
            framer.Drag(x, y, camera.modelview, camera.persp);
        if (picked == &camera)
            camera.MouseDrag(x, y, Shift(w));
    }
}

void MouseWheel(GLFWwindow *w, double xoffset, double direction) {
    if (picked == &framer)
        framer.Wheel(direction, Shift(w));
    if (picked == &camera)
        camera.MouseWheel(direction, Shift(w));
}

// Application

void Resize(GLFWwindow *w, int width, int height) {
    glViewport(0, 0, winW = width, winH = height);
    camera.Resize(width, height);
}

void Keyboard(GLFWwindow *w, int c, int scancode, int action, int mods) {
    if (action == GLFW_PRESS)
        switch (c) {
            case 'R': ReadScene(sceneFilename); break;
            case 'S': SaveScene(); break;
            case 'L': ListScene(); break;
            case 'D': DeleteMesh(); break;
            case 'A': AddMesh(); break;
            default: break;
        }
}

int main(int ac, char **av) { 
    
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);

    // init app window and GL context
    glfwInit();

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    GLFWwindow *w = glfwCreateWindow(winW, winH, "BRDF", NULL, NULL);
    glfwSetWindowPos(w, 100, 100);
    glfwMakeContextCurrent(w);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    //change window title
    glfwSetWindowTitle(w, "Biderectional Reflectance Distribution Function ");
    
    // Imgui stuff

        // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
    bool err = false;
    glbinding::Binding::initialize();
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
    bool err = false;
    glbinding::initialize([](const char* name) { return (glbinding::ProcAddress)glfwGetProcAddress(name); });
#else
    bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // No need to declare a default font
    // However, the default font is ugly.
    io.Fonts->AddFontFromFileTTF("BAUHS93.TTF", 15);
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    
    // Original Stuff
    ImGui_ImplGlfw_InitForOpenGL(w, true);
    //ImGui_ImplOpenGL3_Init(glsl_version);
    ImGui_ImplOpenGL3_Init((char*)glGetString(GL_NUM_SHADING_LANGUAGE_VERSIONS));

    // End of Imgui stuff


    // build shader program, read scene file
    shader = LinkProgramViaCode(&vertexShader, &pixelShader);
    if (ReadScene(sceneFilename))
        printf("Read %i meshes\n", meshes.size());
    else {
        printf("Can't read %s, using default scene\n", sceneFilename);
        // read default meshes
        char meshName[100];
        int nMeshes = sizeof(defaultNames)/sizeof(char *);
        meshes.resize(nMeshes);
        for (int i = 0; i < nMeshes; i++) {
            sprintf(meshName, "%s/%s", directory, defaultNames[i]);
            if (!meshes[i].Read(i, meshName))
                meshes.resize(--nMeshes);
        }
    }

    Resize(w, winW, winH); // initialize camera.arcball.fixedBase
    printf("Usage:\n  R: read scene\n  S: save scene\n  L: list scene\n  D: delete mesh\n  A: add mesh\n");
    // callbacks
    glfwSetCursorPosCallback(w, MouseMove);
    glfwSetMouseButtonCallback(w, MouseButton);
    glfwSetScrollCallback(w, MouseWheel);
    glfwSetKeyCallback(w, Keyboard);
    glfwSetWindowSizeCallback(w, Resize);

    /* ImGUI Stuff*/
    bool show_demo_window = false;
    bool show_settings_window = true;
    bool show_lambert_model = false;
    bool show_disney_model = false;
    bool show_orennayar_model = false;
    bool show_blinnphong_model = false;
    bool show_cooktorrance_model = false;
    bool show_extras = true;
    bool move_guitar_updown = false;
    bool spot_light1 = false;
    bool enable_spot_light1 = true;
    //time_t current_time;
    
    //ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImVec4 clear_color = ImVec4(0.5f, 0.5f, 0.5f, 1.00f);
    /* End of ImGUI stuff*/

    // event loop
    glfwSwapInterval(1);
    while (!glfwWindowShouldClose(w)) {
        Display();
        glfwPollEvents();

        /*************** IMGUI Code *************************/
        // Start the Dear ImGui frame

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // Show the Lambert Model
        if (show_lambert_model)
        {
            SetUniform(shader, "show_lambert_model", 1);
        }
        else
        {
            SetUniform(shader, "show_lambert_model", 0);
        }

        // Show the Disney Model
        if (show_disney_model)
            SetUniform(shader, "show_disney_model", 1);
        else
            SetUniform(shader, "show_disney_model", 0);

        // Show the Oren Nayar Model
        if (show_orennayar_model)
            SetUniform(shader, "show_orennayar_model", 1);
        else
            SetUniform(shader, "show_orennayar_model", 0);

        // Show the Blinn Phong Model
        if (show_blinnphong_model)
            SetUniform(shader, "show_blinnphong_model", 1);
        else
            SetUniform(shader, "show_blinnphong_model", 0);

        // Show the Cook Torrance Model
        if (show_cooktorrance_model)
            SetUniform(shader, "show_cooktorrance_model", 1);
        else
            SetUniform(shader, "show_cooktorrance_model", 0);

        if (show_settings_window)
        {
            //static float f = 0.0f;
            // Create a window called "BRDF Models" and append into it.
            ImGui::Begin("BRDF Models", nullptr, ImGuiWindowFlags_NoResize);
            ImGui::SetWindowSize(ImVec2(260, 390));
            ImGui::SetWindowPos(ImVec2(0, 0));
            //ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize;
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Spot Light");
            
            if (enable_spot_light1)
            {
                ImGui::Checkbox("Spot Light1", &spot_light1);
                SetUniform(shader, "spot_light1", 0);
            }



            //ImVec2 winsize = ImGui::GetWindowSize();

            // Display some text (you can use a format strings too)
            //ImGui::Text("Select a Model:");
            // Checkboxes
            ImGui::SetWindowFontScale(1.3);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Difusse");
            //ImGui::Text("Difusse: ");
            
            // checkboxes work, but can select multiple
            /*ImGui::SetWindowFontScale(1.1);
            ImGui::Checkbox("Demo Window", &show_demo_window);      
            ImGui::Checkbox("Lambert", &show_lambert_model);          
            ImGui::Checkbox("Disney", &show_disney_model);
            ImGui::Checkbox("Oren Nayar", &show_orennayar_model);*/
            // End of checkboxes

            // Trying with radio buttons
            ImGui::SetWindowFontScale(1.1);
            static int e = -1;
            ImGui::RadioButton("Lambert   ", &e, 0);
            ImGui::RadioButton("Disney    ", &e, 1); //ImGui::SameLine();
            ImGui::RadioButton("Oren Nayar ", &e, 2);
            static int i1 = 0;

            if (e == 0)
            {
                SetUniform(shader, "show_lambert_model", 1);
            }
            else if (e == 1)
            {
                SetUniform(shader, "show_disney_model", 1);
                ImGui::SliderInt("Roughfness", &i1, 1, 3);
                ImGui::SetWindowSize(ImVec2(270, 420));
            }
            else if (e == 2)
            {
                SetUniform(shader, "show_orennayar_model", 1);
                ImGui::SliderInt("Roughfness", &i1, 1, 3);
                ImGui::SetWindowSize(ImVec2(270, 420));
            }
            // End of Testing radio buttons
            
            // Specular
            ImGui::SetWindowFontScale(1.3);
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "Specular");
            //ImGui::Text("Specular: ");
            ImGui::SetWindowFontScale(1.1);
            
            static int e1 = -1;
            ImGui::RadioButton("Blinn Phong ", &e1, 0);
            ImGui::RadioButton("Cook Torrance ", &e1, 1);

            if (e1 == 0)
            {
                SetUniform(shader, "show_blinnphong_model", 1);
            }
            else if (e1 == 1)
            {
                SetUniform(shader, "show_cooktorrance_model", 1);
            }

            ImGui::NewLine();
            ImGui::SetWindowFontScale(1.5);
            if (ImGui::Button("  Reset  "))
            {
                ImGui::SetWindowSize(ImVec2(260, 390));
                show_lambert_model = false;
                show_disney_model = false;
                show_orennayar_model = false;
                show_blinnphong_model = false;
                show_cooktorrance_model = false;
                e = -1;
                e1 = -1;
            }
            
            // Checkboxes for specular
            //ImGui::Checkbox("Blinn Phong", &show_blinnphong_model);
            //ImGui::Checkbox("Cook Torrance", &show_cooktorrance_model);
            // End of checkboxes for specular
            
            // Could we use a slider in the future to manipulate
            // Intensity?
            //ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            

            // Display FPS
            //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            /*if (ImGui::TreeNode("Difusse Test"))
            {
                static bool selection[3] = { true, false, false };
                ImGui::Selectable("1. Lambert", &selection[0]);
                ImGui::Selectable("2. Disney", &selection[1]);
                ImGui::Selectable("3. Oren Nayar", &selection[2]);
                ImGui::TreePop();
            }*/

            // Testing drop down menu
            /*if (ImGui::TreeNode("Difusse Dropdown menu:"))
            {
                static int selected = -1;
                for (int n = 0; n < 5; n++)
                {
                    char buf[32];
                    sprintf(buf, "Object %d", n);
                    if (ImGui::Selectable(buf, selected == n))
                        selected = n;
                }
                ImGui::TreePop();
            }*/

            if (show_extras)
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Extras:");
                ImGui::Checkbox("Move Guitar", &move_guitar_updown);
                if (move_guitar_updown)
                {
                    SetUniform(shader, "move_guitar_updown", 1);
                }
                else
                {
                    SetUniform(shader, "move_guitar_updown", 0);
                }
            }

            //auto t1 = std::chrono::high_resolution_clock::now();
            //auto t2 = std::chrono::high_resolution_clock::now();
            // Test Mauricio
            auto duration = std::chrono::system_clock::now().time_since_epoch();
            auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();           

            //current_time = clock();
            //current_time = time(0);
            //auto current_time = std::chrono::system_clock::now();
            //cout << "current time: " << current_time << endl;
            //SetUniform(shader, "current_time", (int)current_time);
            SetUniform(shader, "current_time", (int)current_time);

            ImGui::End();
            
        }
            
        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        /*************** End IMGUI Code *************************/
 
        glFlush();
        glfwSwapBuffers(w);

    }
    // unbind vertex buffer, free GPU memory
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    for (size_t i = 0; i < meshes.size(); i++)
        glDeleteBuffers(1, &meshes[i].vBufferId);

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(w);
    glfwTerminate();
}

