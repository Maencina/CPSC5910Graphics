// MultiMesh.cpp - display and manipulate multiple OBJ meshes
// (c) Jules Bloomenthal 2019, all rights reserved. Commercial use requires license.

#include <glad.h>
#include <GLFW/glfw3.h>
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


using std::vector;
using std::string;
using namespace std;

// display
GLuint      shader = 0;
int         winW = 600, winH = 600;
// Commented by Mauricio
CameraAB    camera(0, 0, winW, winH, vec3(0,0,0), vec3(0,0,-5));

// interaction
int         xCursorOffset = 0, yCursorOffset = 0;
vec3        light(-.2f, .4f, .3f);
Framer      framer;     // to position/orient individual mesh
Mover       mover;      // to position light
void       *picked = &camera;

// Mesh Class

class Mesh {
public:
    Mesh();
    int id;
    string filename;
    // vertices and triangles
    vector<vec3> points;
    vector<vec3> normals;
    vector<vec2> uvs;
    vector<int3> triangles;
    // object to world space
    mat4 xform;
    // GPU vertex buffer and texture
    GLuint vBufferId, textureId;
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
    void main() {
        vPoint = (modelview*vec4(point, 1)).xyz;
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
    uniform sampler2D textureImage;
    void main() {
        vec3 N = normalize(vNormal);       // surface normal
        vec3 L = normalize(light-vPoint);  // light vector
        vec3 E = normalize(vPoint);        // eye vector
        vec3 R = reflect(L, N);            // highlight vector
        float d = abs(dot(N, L));          // two-sided diffuse
        float s = abs(dot(R, E));          // two-sided specular
        float intensity = clamp(d+pow(s, 50), 0, 1);
        vec3 color = texture(textureImage, vec2(vUv.x,1-vUv.y)).rgb;
        pColor = vec4(2*intensity*color, 1);
        //pColor = vec4(vec3(1,0,0), 1);
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

Mesh::Mesh() { vBufferId = textureId = id = 0; }

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
    glActiveTexture(GL_TEXTURE1+id);    // active texture corresponds with textureUnit
    glBindTexture(GL_TEXTURE_2D, textureId);
    SetUniform(shader, "textureImage", (int) textureId);
    SetUniform(shader, "modelview", camera.modelview*xform);
    SetUniform(shader, "persp", camera.persp);
    glDrawElements(GL_TRIANGLES, 3*triangles.size(), GL_UNSIGNED_INT, &triangles[0]);
}

bool Mesh::Read(int mid, char *name, mat4 *m) {
    id = mid;
    filename = string(name);
    string objectFilename = filename+".obj";
    string textureFilename = filename+".tga";
    if (!ReadAsciiObj((char *) objectFilename.c_str(), points, triangles, &normals, &uvs)) {
        printf("can't read %s\n", objectFilename.c_str());
        return false;
    }
    Normalize(points, .8f);
    Buffer();
    textureId = LoadTexture((char *) textureFilename.c_str(), id);
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
    // display objects
    for (size_t i = 0; i < meshes.size(); i++)
        meshes[i].Draw();
    // lights and frames
    if ((clock()-mouseMoved)/CLOCKS_PER_SEC < 1.f) {
        glDisable(GL_DEPTH_TEST);
        UseDrawShader(camera.fullview);
        Disk(light, 9, vec3(1,1,0));
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
    glFlush();
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

void MouseButton(GLFWwindow *w, int butn, int action, int mods) {
    double x, y;
    glfwGetCursorPos(w, &x, &y);
    CorrectMouse(w, &x, &y);
    if (action == GLFW_PRESS && butn == GLFW_MOUSE_BUTTON_LEFT) {
        void *newPicked = NULL;
        if (MouseOver(x, y, light, camera.fullview)) {
            newPicked = &mover;
            mover.Down(&light, x, y, camera.modelview, camera.persp);
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
    // init app window and GL context
    glfwInit();
    GLFWwindow *w = glfwCreateWindow(winW, winH, "MultiMesh", NULL, NULL);
    glfwSetWindowPos(w, 100, 100);
    glfwMakeContextCurrent(w);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
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
    // event loop
    glfwSwapInterval(1);
    while (!glfwWindowShouldClose(w)) {
        Display();
        glfwSwapBuffers(w);
        glfwPollEvents();
    }
    // unbind vertex buffer, free GPU memory
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    for (size_t i = 0; i < meshes.size(); i++)
        glDeleteBuffers(1, &meshes[i].vBufferId);
    glfwDestroyWindow(w);
    glfwTerminate();
}
