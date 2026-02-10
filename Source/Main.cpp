#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstddef>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../Header/Util.h"

enum GameState { WAITING_FOR_COIN, PLAYING, RETURNING };
GameState currentState = WAITING_FOR_COIN;

struct Toy {
    glm::vec3 pos;
    glm::vec3 color;
    bool isCaught = false;
    bool isDropped = false;
    bool isTaken = false;
    bool isFalling = false;
    float verticalVelocity = 0.0f;
};

std::vector<Toy> toys = {
    {glm::vec3(0.3f, 1.15f, -0.4f), glm::vec3(0.1f, 0.5f, 0.8f)},
    {glm::vec3(-0.3f, 1.15f, 0.2f), glm::vec3(0.9f, 0.2f, 0.2f)}
};

float yaw = -90.0f, pitch = 0.0f, cameraAngle = 0.0f, cameraRadius = 10.0f;
float lastX = 400, lastY = 300;
bool firstMouse = true;

float clawX = 0.0f, clawZ = 0.0f, clawY = 4.3f;
bool movingDown = false, movingUp = false, clawIsHolding = false;
float joystickRotX = 0.0f, joystickRotZ = 0.0f;

double lastTime = 0;
const double targetFPS = 75.0;
const double frameTime = 1.0 / targetFPS;

bool depthTestEnabled = true;
bool cullFaceEnabled = false;
unsigned int potpisTex;

unsigned int sphereVAO = 0; unsigned int sphereVertexCount = 0;

Model toyModel1, toyModel2;

void createSphere(int latSegments, int lonSegments, unsigned int &outVAO, unsigned int &outVertexCount) {
    struct V { float x,y,z; float nx,ny,nz; float u,v; };
    std::vector<V> verts;
    for (int y = 0; y < latSegments; ++y) {
        for (int x = 0; x < lonSegments; ++x) {
            float x0 = (float)x / lonSegments;
            float x1 = (float)(x+1) / lonSegments;
            float y0 = (float)y / latSegments;
            float y1 = (float)(y+1) / latSegments;

            float theta0 = x0 * 2.0f * (float)M_PI;
            float theta1 = x1 * 2.0f * (float)M_PI;
            float phi0 = y0 * (float)M_PI;
            float phi1 = y1 * (float)M_PI;

            glm::vec3 p00 = glm::vec3(sin(phi0)*cos(theta0), cos(phi0), sin(phi0)*sin(theta0));
            glm::vec3 p10 = glm::vec3(sin(phi0)*cos(theta1), cos(phi0), sin(phi0)*sin(theta1));
            glm::vec3 p01 = glm::vec3(sin(phi1)*cos(theta0), cos(phi1), sin(phi1)*sin(theta0));
            glm::vec3 p11 = glm::vec3(sin(phi1)*cos(theta1), cos(phi1), sin(phi1)*sin(theta1));

            auto pushV = [&](const glm::vec3 &p, float u, float v){ V vv; vv.x = p.x; vv.y = p.y; vv.z = p.z; vv.nx = p.x; vv.ny = p.y; vv.nz = p.z; vv.u = u; vv.v = v; verts.push_back(vv); };

            pushV(p00, x0, y0);
            pushV(p01, x0, y1);
            pushV(p10, x1, y0);

            pushV(p10, x1, y0);
            pushV(p01, x0, y1);
            pushV(p11, x1, y1);
        }
    }

    if (verts.empty()) return;
    GLuint VAO, VBO; glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,x));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,nx));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,u));

    outVAO = VAO; outVertexCount = (unsigned int)verts.size();
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = (xpos - lastX) * 0.1f;
    float yoffset = (lastY - ypos) * 0.1f;
    lastX = xpos; lastY = ypos;
    yaw += xoffset; pitch += yoffset;
    if (pitch > 89.0f) pitch = 89.0f; if (pitch < -89.0f) pitch = -89.0f;
}

void drawObject(unsigned int vao, int vertexCount, unsigned int shader, glm::mat4 model, glm::vec3 color, float alpha = 1.0f, bool useTex = false) {
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(color));
    glUniform1f(glGetUniformLocation(shader, "alpha"), alpha);
    glUniform1i(glGetUniformLocation(shader, "useTexture"), useTex);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}

void drawObject(unsigned int vao, unsigned int shader, glm::mat4 model, glm::vec3 color, float alpha = 1.0f, bool useTex = false) {
    drawObject(vao, 36, shader, model, color, alpha, useTex);
}

int main() {
    if (!glfwInit()) return -1;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "3D Kandza - RG Projekat", monitor, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    if (glewInit() != GLEW_OK) return -1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float vertices[] = {
        -0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  0.0f, 0.0f,  0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  1.0f, 0.0f,
         0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  1.0f, 1.0f,  0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  1.0f, 1.0f,
        -0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  0.0f, 1.0f, -0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  0.0f, 0.0f,
        -0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,  0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
         0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,  0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
        -0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f, -0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 0.0f, -0.5f, 0.5f,-0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 1.0f,
        -0.5f,-0.5f,-0.5f, -1.0f, 0.0f, 0.0f,  0.0f, 1.0f, -0.5f,-0.5f,-0.5f, -1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
        -0.5f,-0.5f, 0.5f, -1.0f, 0.0f, 0.0f,  0.0f, 0.0f, -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
         0.5f, 0.5f, 0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f,  0.5f, 0.5f,-0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f,
         0.5f,-0.5f,-0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f,  0.5f,-0.5f,-0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
         0.5f,-0.5f, 0.5f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f,  0.5f, 0.5f, 0.5f,  1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
        -0.5f,-0.5f,-0.5f,  0.0f,-1.0f, 0.0f,  0.0f, 1.0f,  0.5f,-0.5f,-0.5f,  0.0f,-1.0f, 0.0f,  1.0f, 1.0f,
         0.5f,-0.5f, 0.5f,  0.0f,-1.0f, 0.0f,  1.0f, 0.0f,  0.5f,-0.5f, 0.5f,  0.0f,-1.0f, 0.0f,  1.0f, 0.0f,
        -0.5f,-0.5f, 0.5f,  0.0f,-1.0f, 0.0f,  0.0f, 0.0f, -0.5f,-0.5f,-0.5f,  0.0f,-1.0f, 0.0f,  0.0f, 1.0f,
        -0.5f, 0.5f,-0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f,  0.5f, 0.5f,-0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f,
         0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,  0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
        -0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, -0.5f, 0.5f,-0.5f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);

    createSphere(24, 24, sphereVAO, sphereVertexCount);

    unsigned int shaderProgram = createShader("Resources/shader.vert", "Resources/shader.frag");
    unsigned int coinTex = loadImageToTexture("Resources/img.png");

    potpisTex = loadImageToTexture("Resources/img.png");

    toyModel1 = loadOBJWithCandidates({"Resources/Toy1/model.obj", "Resources/Toy1/toy.obj", "Resources/Toy1.obj", "Resources/Toy1/model.obj"});
    toyModel2 = loadOBJWithCandidates({"Resources/Toy2/model.obj", "Resources/Toy2/toy.obj", "Resources/Toy2.obj", "Resources/Toy2/model.obj"});

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        if (currentTime - lastTime < frameTime) continue;
        lastTime = currentTime;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

        static bool dPressed = false;
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !dPressed) {
            depthTestEnabled = !depthTestEnabled;
            dPressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE) {
            dPressed = false;
        }

        static bool cPressed = false;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !cPressed) {
            cullFaceEnabled = !cullFaceEnabled;
            cPressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE) {
            cPressed = false;
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) cameraAngle -= 0.03f;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) cameraAngle += 0.03f;
        glm::vec3 orbitPos = glm::vec3(sin(cameraAngle) * cameraRadius, 5.0f, cos(cameraAngle) * cameraRadius);
        glm::vec3 frontVec = glm::vec3(cos(glm::radians(yaw)) * cos(glm::radians(pitch)), sin(glm::radians(pitch)), sin(glm::radians(yaw)) * cos(glm::radians(pitch)));
        glm::mat4 view = glm::lookAt(orbitPos, orbitPos + frontVec, glm::vec3(0, 1, 0));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)mode->width / (float)mode->height, 0.1f, 100.0f);

        bool isFront = (cos(cameraAngle) > 0.7f);
        joystickRotX = 0.0f; joystickRotZ = 0.0f;

        bool prizeInChute = false;
        for (auto& t : toys) if (t.isDropped && !t.isTaken) prizeInChute = true;

        if (isFront && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (currentState == WAITING_FOR_COIN) currentState = PLAYING;
            for (auto& t : toys) if (t.isDropped && !t.isTaken) t.isTaken = true;
        }

        if (currentState == PLAYING && !movingDown && !movingUp) {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { clawZ -= 0.05f; joystickRotX = -20.0f; }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { clawZ += 0.05f; joystickRotX = 20.0f; }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { clawX -= 0.05f; joystickRotZ = 20.0f; }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { clawX += 0.05f; joystickRotZ = -20.0f; }

            static bool spaceWasPressed = false;
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !spaceWasPressed) {
                if (!clawIsHolding) movingDown = true;
                else {
                    for (auto& t : toys) if (t.isCaught) {
                        t.isCaught = false; t.isFalling = true;
                        t.verticalVelocity = 0.0f;
                        clawIsHolding = false;
                    }
                    currentState = WAITING_FOR_COIN;
                }
                spaceWasPressed = true;
            }
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) spaceWasPressed = false;
            clawX = glm::clamp(clawX, -1.7f, 1.7f); clawZ = glm::clamp(clawZ, -1.7f, 1.7f);
        }

        if (movingDown) {
            clawY -= 0.06f;
            if (clawY <= 1.7f) {
                movingDown = false; movingUp = true;
                for (auto& t : toys) {
                    if (!t.isDropped && !t.isTaken && glm::distance(glm::vec2(clawX, clawZ), glm::vec2(t.pos.x, t.pos.z)) < 0.45f) {
                        t.isCaught = true; clawIsHolding = true; break;
                    }
                }
            }
        }
        else if (movingUp) {
            clawY += 0.06f; if (clawY >= 4.3f) movingUp = false;
        }

        for (int i = 0; i < (int)toys.size(); ++i) {
            auto &t = toys[i];
            if (t.isCaught) {
                float halfH = 0.25f;
                if (i == 0 && toyModel1.VAO != 0) halfH = toyModel1.halfHeight;
                if (i == 1 && toyModel2.VAO != 0) halfH = toyModel2.halfHeight;

                float baseOffset = 0.35f;
                float modelScaleForAttach = 1.0f;
                float offset = baseOffset + halfH * modelScaleForAttach * 0.6f;

                glm::vec3 desiredPos = glm::vec3(clawX, clawY - offset, clawZ);

                float lateralRadius = halfH * modelScaleForAttach * 0.8f;
                glm::vec3 halfE = glm::vec3(0.25f);
                if (i==0 && toyModel1.VAO!=0) halfE = toyModel1.halfExtents * modelScaleForAttach;
                if (i==1 && toyModel2.VAO!=0) halfE = toyModel2.halfExtents * modelScaleForAttach;

                float glassInset = 0.06f;
                float minX = -1.95f + halfE.x + glassInset;
                float maxX =  1.95f - halfE.x - glassInset;
                float minZ = -1.95f + halfE.z + glassInset;
                float maxZ =  1.95f - halfE.z - glassInset;

                desiredPos.x = glm::clamp(desiredPos.x, minX, maxX);
                desiredPos.z = glm::clamp(desiredPos.z, minZ, maxZ);

                float topLimit = 6.8f;
                float bottomLimit = 0.36f;
                desiredPos.y = glm::clamp(desiredPos.y, bottomLimit + halfE.y, topLimit - halfE.y);

                float followLerp = 0.35f;
                t.pos = glm::mix(t.pos, desiredPos, followLerp);
            }

            if (t.isFalling) {
                t.verticalVelocity -= 0.015f;
                t.pos.y += t.verticalVelocity;
                float floorLevel = (t.pos.x < -0.8f && t.pos.z > 0.5f) ? 0.35f : 1.15f;
                if (t.pos.y <= floorLevel) {
                    t.pos.y = floorLevel; t.isFalling = false; t.verticalVelocity = 0;
                    if (floorLevel < 1.0f) t.isDropped = true;
                }
            }
        }

        if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);

        if (cullFaceEnabled) glEnable(GL_CULL_FACE);
        else glDisable(GL_CULL_FACE);

        glClearColor(0.4f, 0.5f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniform1i(glGetUniformLocation(shaderProgram, "uTex"), 0);
        glUniform3f(glGetUniformLocation(shaderProgram, "spotlightPos"), 1.6f, 4.8f, 1.6f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 0.0f, 8.0f, 4.0f);
        glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 1.0f, 1.0f);

        glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), orbitPos.x, orbitPos.y, orbitPos.z);
        glUniform1f(glGetUniformLocation(shaderProgram, "shininess"), 64.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "specularStrength"), 0.6f);

        glUniform3f(glGetUniformLocation(shaderProgram, "pointLightPos"), 0.0f, 5.0f, 1.5f);
        glUniform3f(glGetUniformLocation(shaderProgram, "pointLightColor"), 0.9f, 0.85f, 0.8f);

        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.01f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(60.0f, 0.01f, 60.0f)), glm::vec3(0.15f, 0.05f, 0.1f));
        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(0, 15.0f, -20.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(60.0f, 30.0f, 0.2f)), glm::vec3(0.4f, 0.15f, 0.1f));
        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(-30.0f, 15.0f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.2f, 30.0f, 60.0f)), glm::vec3(0.35f, 0.12f, 0.08f));
        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(30.0f, 15.0f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.2f, 30.0f, 60.0f)), glm::vec3(0.35f, 0.12f, 0.08f));
        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(0, 15.0f, 25.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(60.0f, 30.0f, 0.2f)), glm::vec3(0.4f, 0.15f, 0.1f));

        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.05, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(4, 0.1, 4)), glm::vec3(0.02));
        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(0.6f, 0.6f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(2.8f, 1.1f, 4.0f)), glm::vec3(0.5f, 0.05f, 0.05f));
        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(-1.4f, 0.6f, -0.75f)) * glm::scale(glm::mat4(1.0f), glm::vec3(1.2f, 1.1f, 2.5f)), glm::vec3(0.5f, 0.05f, 0.05f));

        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(0, 5, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(4, 0.4, 4)), glm::vec3(0.35, 0, 0));

        glm::vec3 lampColor;
        if (prizeInChute) {
            if ((int)(glfwGetTime() * 4) % 2 == 0) lampColor = glm::vec3(0, 1, 0);
            else lampColor = glm::vec3(1, 0, 0);
        }
        else if (currentState == PLAYING) {
            lampColor = glm::vec3(0, 0, 1);
        }
        else {
            lampColor = glm::vec3(0, 0, 0);
        }
        glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), lampColor.r, lampColor.g, lampColor.b);

        glm::mat4 lampModelMat = glm::translate(glm::mat4(1.0f), glm::vec3(1.6, 4.8, 1.6)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.25));
        if (sphereVAO != 0 && sphereVertexCount > 0) {
            drawObject(sphereVAO, sphereVertexCount, shaderProgram, lampModelMat, lampColor);
        } else {
            drawObject(VAO, shaderProgram, lampModelMat, lampColor);
        }

        glm::vec3 joyBasePos = glm::vec3(1.1f, 1.1f, 2.2f);

        float machineTopY = 1.15f;
        glm::vec3 connStart = glm::vec3(1.1f, machineTopY + 0.005f, 2.01f);
        glm::vec3 connEnd = glm::vec3(joyBasePos.x, joyBasePos.y - 0.15f, 2.05f);
        glm::vec3 connMid = (connStart + connEnd) * 0.5f;
        float connHeight = glm::distance(connStart, connEnd);
        glm::mat4 connModel = glm::translate(glm::mat4(1.0f), connMid) * glm::scale(glm::mat4(1.0f), glm::vec3(0.04f, connHeight * 0.5f, 0.04f));
        drawObject(VAO, shaderProgram, connModel, glm::vec3(0.2f, 0.2f, 0.2f));

        glm::mat4 mount = glm::translate(glm::mat4(1.0f), glm::vec3(1.1f, machineTopY + 0.02f, 2.01f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.18f, 0.02f, 0.03f));
        drawObject(VAO, shaderProgram, mount, glm::vec3(0.15f, 0.15f, 0.15f));

        glm::mat4 joyBase = glm::translate(glm::mat4(1.0f), joyBasePos);
        glm::mat4 joyHandle = glm::rotate(joyBase, glm::radians(joystickRotX), glm::vec3(1, 0, 0));
        joyHandle = glm::rotate(joyHandle, glm::radians(joystickRotZ), glm::vec3(0, 0, 1));
        drawObject(VAO, shaderProgram, joyHandle * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.25, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.05, 0.5, 0.05)), glm::vec3(0.1));
        drawObject(VAO, shaderProgram, joyHandle * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.5, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.2)), glm::vec3(0.8, 0, 0));

        if (coinTex != 0) {
            glm::mat4 slotBase = glm::translate(glm::mat4(1.0f), glm::vec3(0.3f, 0.74f, 2.01f))
                * glm::scale(glm::mat4(1.0f), glm::vec3(0.35f, 0.02f, 0.02f));
            drawObject(VAO, shaderProgram, slotBase, glm::vec3(0.06f, 0.06f, 0.06f));

            glm::mat4 slotRim = glm::translate(glm::mat4(1.0f), glm::vec3(0.3f, 0.755f, 2.01f))
                * glm::scale(glm::mat4(1.0f), glm::vec3(0.38f, 0.01f, 0.022f));
            drawObject(VAO, shaderProgram, slotRim, glm::vec3(0.6f, 0.6f, 0.6f));

            glm::mat4 slotInner = glm::translate(glm::mat4(1.0f), glm::vec3(0.3f, 0.745f, 2.01f))
                * glm::scale(glm::mat4(1.0f), glm::vec3(0.28f, 0.015f, 0.018f));
            drawObject(VAO, shaderProgram, slotInner, glm::vec3(0.12f, 0.12f, 0.12f));
        }

        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(clawX, clawY, clawZ)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.4f, 0.2f, 0.4f)), glm::vec3(0.7f, 0.7f, 0.75f));
        float rLen = 5.0f - clawY;
        drawObject(VAO, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(clawX, clawY + rLen / 2.0f, clawZ)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.02f, rLen, 0.02f)), glm::vec3(0.2f));

        float fingerAngle = (clawIsHolding || movingDown) ? 15.0f : 45.0f;
        for (int i = 0; i < 4; i++) {
            glm::mat4 fM = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(clawX, clawY - 0.1f, clawZ)), glm::radians(i * 90.0f), glm::vec3(0, 1, 0));
            fM = glm::rotate(glm::translate(fM, glm::vec3(0.15f, 0, 0)), glm::radians(fingerAngle), glm::vec3(0, 0, 1));
            drawObject(VAO, shaderProgram, fM * glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.2f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.08f, 0.4f, 0.08f)), glm::vec3(0.5f, 0.5f, 0.55f));
            drawObject(VAO, shaderProgram, fM * glm::translate(glm::mat4(1.0f), glm::vec3(-0.05f, -0.4f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.15f, 0.05f, 0.08f)), glm::vec3(0.4f, 0.4f, 0.4f));
        }

        for (int i=0;i<toys.size();++i) {
            auto &t = toys[i];
            if (t.isTaken) continue;
            float modelScale = 1.0f;
            float extraYOffset = 0.01f;
            if (i==0 && toyModel1.VAO!=0) {
                glm::mat4 modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(t.pos.x, t.pos.y + toyModel1.halfHeight * modelScale + extraYOffset, t.pos.z))
                    * glm::scale(glm::mat4(1.0f), glm::vec3(modelScale));
                drawObject(toyModel1.VAO, toyModel1.vertexCount, shaderProgram, modelMat, t.color);
            } else if (i==1 && toyModel2.VAO!=0) {
                glm::mat4 modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(t.pos.x, t.pos.y + toyModel2.halfHeight * modelScale + extraYOffset, t.pos.z))
                    * glm::scale(glm::mat4(1.0f), glm::vec3(modelScale));
                drawObject(toyModel2.VAO, toyModel2.vertexCount, shaderProgram, modelMat, t.color);
            } else {
                glm::mat4 modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(t.pos.x, t.pos.y + 0.25f + extraYOffset, t.pos.z)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5, 0.4, 0.5));
                drawObject(VAO, 36, shaderProgram, modelMat, t.color);
            }
        }

        glDepthMask(GL_FALSE);
        drawObject(VAO, 36, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(0, 3, -1.95)) * glm::scale(glm::mat4(1.0f), glm::vec3(3.9, 3.8, 0.01)), glm::vec3(0.7, 0.8, 1.0), 0.15f);
        drawObject(VAO, 36, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(1.95, 3, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.01, 3.8, 3.9)), glm::vec3(0.7, 0.8, 1.0), 0.15f);
        drawObject(VAO, 36, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(-1.95, 3, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.01, 3.8, 3.9)), glm::vec3(0.7, 0.8, 1.0), 0.15f);
        drawObject(VAO, 36, shaderProgram, glm::translate(glm::mat4(1.0f), glm::vec3(0, 3, 1.95)) * glm::scale(glm::mat4(1.0f), glm::vec3(3.9, 3.8, 0.01)), glm::vec3(0.7, 0.8f, 1.0), 0.15f);
        glDepthMask(GL_TRUE);

        glDisable(GL_DEPTH_TEST);
        glUseProgram(shaderProgram);

        glm::mat4 identity = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(identity));

        glm::mat4 modelSign = glm::translate(identity, glm::vec3(0.65f, 0.8f, 0.0f))
            * glm::scale(identity, glm::vec3(0.4f, 0.2f, 1.0f));

        glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 1.0f, 1.0f, 1.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "alpha"), 0.8f);
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), true);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, potpisTex);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelSign));

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(shaderProgram); glDeleteTextures(1, &coinTex);
    glDeleteVertexArrays(1, &VAO); glDeleteBuffers(1, &VBO);
    glfwTerminate();
    return 0;
}