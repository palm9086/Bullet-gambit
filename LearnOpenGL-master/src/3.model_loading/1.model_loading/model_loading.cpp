#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>
#include <learnopengl/shader.h>

#include <stb_image.h>
#include <ctime>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <random>

// Callback functions
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void renderCube();

// Settings
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

// Camera
Camera camera(glm::vec3(0.0f, 30.0f, 30.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float camYaw = 0.0f;
float camPitch = 45.0f;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Cube
unsigned int cubeVAO = 0, cubeVBO = 0;

// ====================
// Russian Roulette Data
// ====================
bool player1Turn = true;
bool gameOver = false;
bool chamber[6];
int currentChamber = 0;
std::string gameMessage = "Player 1's turn";

// ====================
// Create cube VAO
// ====================
void renderCube()
{
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // positions          // normals           // texcoords
            // Back face
            -0.5f, -0.5f, -0.5f, 0.0f,0.0f, -1.0f, 0.0f,0.0f,
             0.5f,  0.5f, -0.5f, 0.0f,0.0f, -1.0f, 1.0f,1.0f,
             0.5f, -0.5f, -0.5f, 0.0f,0.0f, -1.0f, 1.0f,0.0f,
             0.5f,  0.5f, -0.5f, 0.0f,0.0f, -1.0f,1.0f,
            -0.5f, -0.5f, -0.5f, 0.0f,0.0f, -1.0f,0.0f,0.0f,
            -0.5f,  0.5f, -0.5f, 0.0f,0.0f, -1.0f,0.0f,1.0f,

            // Front face
            -0.5f, -0.5f, 0.5f, 0.0f,0.0f, 1.0f, 0.0f,0.0f,
             0.5f, -0.5f, 0.5f, 0.0f,0.0f, 1.0f, 1.0f,0.0f,
             0.5f,  0.5f, 0.5f, 0.0f,0.0f, 1.0f, 1.0f,1.0f,
             0.5f,  0.5f, 0.5f, 0.0f,0.0f, 1.0f, 1.0f,1.0f,
            -0.5f,  0.5f, 0.5f, 0.0f,0.0f, 1.0f, 0.0f,1.0f,
            -0.5f, -0.5f, 0.5f, 0.0f,0.0f, 1.0f, 0.0f,0.0f,

            // Left face
            -0.5f,  0.5f,  0.5f, -1.0f,0.0f,0.0f, 1.0f,0.0f,
            -0.5f,  0.5f, -0.5f, -1.0f,0.0f,0.0f, 1.0f,1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,0.0f,0.0f, 0.0f,1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,0.0f,0.0f, 0.0f,1.0f,
            -0.5f, -0.5f,  0.5f, -1.0f,0.0f,0.0f, 0.0f,0.0f,
            -0.5f,  0.5f,  0.5f, -1.0f,0.0f,0.0f, 1.0f,0.0f,

            // Right face
             0.5f,  0.5f,  0.5f, 1.0f,0.0f,0.0f, 1.0f,0.0f,
             0.5f, -0.5f, -0.5f, 1.0f,0.0f,0.0f, 0.0f,1.0f,
             0.5f,  0.5f, -0.5f, 1.0f,0.0f,0.0f, 1.0f,1.0f,
             0.5f, -0.5f, -0.5f, 1.0f,0.0f,0.0f, 0.0f,1.0f,
             0.5f,  0.5f,  0.5f, 1.0f,0.0f,0.0f, 1.0f,0.0f,
             0.5f, -0.5f,  0.5f, 1.0f,0.0f,0.0f, 0.0f,0.0f,

             // Bottom face
             -0.5f, -0.5f, -0.5f, 0.0f,-1.0f,0.0f, 0.0f,1.0f,
              0.5f, -0.5f, -0.5f, 0.0f,-1.0f,0.0f, 1.0f,1.0f,
              0.5f, -0.5f,  0.5f, 0.0f,-1.0f,0.0f, 1.0f,0.0f,
              0.5f, -0.5f,  0.5f, 0.0f,-1.0f,0.0f, 1.0f,0.0f,
             -0.5f, -0.5f,  0.5f, 0.0f,-1.0f,0.0f, 0.0f,0.0f,
             -0.5f, -0.5f, -0.5f, 0.0f,-1.0f,0.0f, 0.0f,1.0f,

             // Top face
             -0.5f, 0.5f, -0.5f, 0.0f,1.0f,0.0f, 0.0f,1.0f,
              0.5f, 0.5f,  0.5f, 0.0f,1.0f,0.0f, 1.0f,0.0f,
              0.5f, 0.5f, -0.5f, 0.0f,1.0f,0.0f, 1.0f,1.0f,
              0.5f, 0.5f,  0.5f, 0.0f,1.0f,0.0f, 1.0f,0.0f,
             -0.5f, 0.5f, -0.5f, 0.0f,1.0f,0.0f, 0.0f,1.0f,
             -0.5f, 0.5f,  0.5f, 0.0f,1.0f,0.0f, 0.0f,0.0f
        };

        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        glBindVertexArray(cubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    }

    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Bullet Gambit Game", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    Shader ourShader("1.model_loading.vs", "1.model_loading.fs");

    // Floor setup
    unsigned int floorTexture;
    glGenTextures(1, &floorTexture);
    glBindTexture(GL_TEXTURE_2D, floorTexture);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(FileSystem::getPath("resources/textures/padoru/Floor.jpg").c_str(), &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = nrChannels == 3 ? GL_RGB : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    stbi_image_free(data);

    // Russian Roulette setup
    std::fill(std::begin(chamber), std::end(chamber), false);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 5);
    int bulletPos = dis(gen);
    chamber[bulletPos] = true;
    currentChamber = 0;
    gameMessage = "Player 1's turn";
    std::cout << gameMessage << std::endl;

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 1000.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        // Ground
        ourShader.setInt("texture_diffuse1", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTexture);
        glm::mat4 groundModel = glm::scale(glm::mat4(1.0f), glm::vec3(500.0f, 1.0f, 500.0f));
        ourShader.setMat4("model", groundModel);
        renderCube();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// ====================
// Input and callbacks
// ====================
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (gameOver)
    {
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        {
            std::fill(std::begin(chamber), std::end(chamber), false);
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 5);
            int bulletPos = dis(gen);
            chamber[bulletPos] = true;
            currentChamber = 0;
            player1Turn = true;
            gameOver = false;
            gameMessage = "Player 1's turn";
            std::cout << "New round started!" << std::endl;
        }
        return;
    }

    static bool leftPressed = false;
    static bool rightPressed = false;

    // Shoot opponent
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !leftPressed)
    {
        leftPressed = true;
        bool fired = chamber[currentChamber];
        currentChamber = (currentChamber + 1) % 6;

        if (fired)
        {
            gameMessage = player1Turn ? "Player 1 shot Player 2 — Player 1 wins!" : "Player 2 shot Player 1 — Player 2 wins!";
            std::cout << gameMessage << std::endl;
            gameOver = true;
        }
        else
        {
            player1Turn = !player1Turn;
            gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
            std::cout << gameMessage << std::endl;
        }
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
        leftPressed = false;

    // Shoot self
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && !rightPressed)
    {
        rightPressed = true;
        bool fired = chamber[currentChamber];
        currentChamber = (currentChamber + 1) % 6;

        if (fired)
        {
            gameMessage = player1Turn ? "Player 1 shot self - Player 2 wins!" : "Player 2 shot self - Player 1 wins!";
            std::cout << gameMessage << std::endl;
            gameOver = true;
        }
        else
        {
            player1Turn = !player1Turn;
            gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
            std::cout << gameMessage << std::endl;
        }
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE)
        rightPressed = false;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    static float lastX = SCR_WIDTH / 2.0f;
    static float lastY = SCR_HEIGHT / 2.0f;
    static bool firstMouse = true;

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
