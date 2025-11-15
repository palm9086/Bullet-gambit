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
#include <vector>
#include <string>

// ====================================================
// === CONTROLS ===
// ====================================================
//
//  ESC .............. Exit Game
//  L-Click .......... Shoot Opponent
//  R-Click .......... Shoot Yourself (gain random item if survive)
//  1-4 .............. Use Item Slot
//  R ................ Restart Game (after someone wins)
//
// ====================================================

// Callback functions
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void renderCube();

// Random integer generator
int randomInt(int min, int max) { return min + rand() % (max - min + 1); }

// Settings
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

// Camera
Camera camera(glm::vec3(0.0f, 30.0f, 30.0f));
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f, lastFrame = 0.0f;

// Cube data
unsigned int cubeVAO = 0, cubeVBO = 0;

// ====================
// Game Data
// ====================
bool player1Turn = true;
bool gameOver = false;
bool chamber[6];
int currentChamber = 0;
std::string gameMessage = "Player 1's turn";

// ====================
// Item System
// ====================
enum ItemType { ITEM_NONE = 0, ITEM_ROLL = 1, ITEM_MOVE_BULLET = 2, ITEM_SKIP = 3 };
std::vector<ItemType> player1Items;
std::vector<ItemType> player2Items;
bool skipNextTurn = false;
GLFWwindow* g_window = nullptr;

std::string itemName(ItemType type)
{
    switch (type)
    {
    case ITEM_ROLL: return "Roll";
    case ITEM_MOVE_BULLET: return "Move";
    case ITEM_SKIP: return "Skip";
    default: return "Empty";
    }
}

// === Print Player Items ===
void printPlayerItems(bool forPlayer1)
{
    auto& items = forPlayer1 ? player1Items : player2Items;
    std::cout << (forPlayer1 ? "Player 1" : "Player 2") << " Items:\n";
    for (int i = 0; i < 4; ++i)
    {
        if (i < (int)items.size())
            std::cout << "  Slot " << (i + 1) << ": " << itemName(items[i]) << "\n";
        else
            std::cout << "  Slot " << (i + 1) << ": Empty\n";
    }
    std::cout << std::endl;
}

// === Update HUD (window title) ===
void updateHUD()
{
    std::string title = "Bullet Gambit | " + gameMessage + " | ";
    title += player1Turn ? "P1 Items: " : "P2 Items: ";

    auto& items = player1Turn ? player1Items : player2Items;
    for (int i = 0; i < 4; ++i)
    {
        if (i < (int)items.size())
            title += "[" + std::to_string(i + 1) + ":" + itemName(items[i]) + "] ";
        else
            title += "[" + std::to_string(i + 1) + ":Empty] ";
    }

    title += "| L-Click: Shoot Opp | R-Click: Shoot Self | 1-4: Use Item | R: Restart";
    glfwSetWindowTitle(g_window, title.c_str());
}

// === Random Item Giving ===
void giveRandomItem(bool forPlayer1)
{
    if (forPlayer1 && player1Items.size() >= 4) return;
    if (!forPlayer1 && player2Items.size() >= 4) return;

    int itemID = randomInt(1, 3);
    ItemType newItem = static_cast<ItemType>(itemID);

    if (forPlayer1)
    {
        player1Items.push_back(newItem);
        std::cout << "Player 1 got item: " << itemName(newItem) << std::endl;
    }
    else
    {
        player2Items.push_back(newItem);
        std::cout << "Player 2 got item: " << itemName(newItem) << std::endl;
    }
    updateHUD();
}

// === Item Usage ===
void useItem(bool forPlayer1, int slot)
{
    auto& items = forPlayer1 ? player1Items : player2Items;
    if (slot < 0 || slot >= (int)items.size()) return;

    ItemType item = items[slot];
    items.erase(items.begin() + slot);

    switch (item)
    {
    case ITEM_ROLL:
        std::fill(std::begin(chamber), std::end(chamber), false);
        chamber[randomInt(0, 5)] = true;
        currentChamber = 0;
        std::cout << "Chamber rolled!" << std::endl;
        break;
    case ITEM_MOVE_BULLET:
        currentChamber = (currentChamber + 1) % 6;
        std::cout << "Bullet moved forward one chamber." << std::endl;
        break;
    case ITEM_SKIP:
        skipNextTurn = true;
        std::cout << "Next turn skipped!" << std::endl;
        break;
    default: break;
    }

    updateHUD();
}

// === Cube Rendering ===
void renderCube()
{
    if (cubeVAO == 0)
    {
        float vertices[] = {
            -0.5f,-0.5f,-0.5f, 0,0,-1, 0,0,
             0.5f, 0.5f,-0.5f, 0,0,-1, 1,1,
             0.5f,-0.5f,-0.5f, 0,0,-1, 1,0,
             0.5f, 0.5f,-0.5f, 0,0,-1, 1,1,
            -0.5f,-0.5f,-0.5f, 0,0,-1, 0,0,
            -0.5f, 0.5f,-0.5f, 0,0,-1, 0,1,
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        glBindVertexArray(cubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }

    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

// ====================================================
// === MAIN ===
// ====================================================
int main()
{
    srand(static_cast<unsigned>(time(0)));

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Bullet Gambit", NULL, NULL);
    if (!g_window)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(g_window);
    glfwSetFramebufferSizeCallback(g_window, framebuffer_size_callback);
    glfwSetCursorPosCallback(g_window, mouse_callback);
    glfwSetScrollCallback(g_window, scroll_callback);
    glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    Shader ourShader("1.model_loading.vs", "1.model_loading.fs");

    // === Initialize Game ===
    std::fill(std::begin(chamber), std::end(chamber), false);
    chamber[randomInt(0, 5)] = true;
    currentChamber = 0;
    gameMessage = "Player 1's turn";
    updateHUD();

    // --- Print Controls First ---
    std::cout << "\n=== BULLET GAMBIT CONTROLS ===\n";
    std::cout << "ESC .............. Exit Game\n";
    std::cout << "L-Click .......... Shoot Opponent\n";
    std::cout << "R-Click .......... Shoot Yourself (gain item if survive)\n";
    std::cout << "1-4 .............. Use Item Slot\n";
    std::cout << "R ................ Restart Game after Win\n";
    std::cout << "=====================================\n\n";

    // --- THEN show player 1 items ---
    printPlayerItems(true);

    // === Game Loop ===
    while (!glfwWindowShouldClose(g_window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(g_window);

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        glm::mat4 model = glm::mat4(1.0f);
        ourShader.setMat4("model", model);
        renderCube();

        glfwSwapBuffers(g_window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// ====================================================
// === Input / Callbacks ===
// ====================================================

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (gameOver)
    {
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        {
            std::fill(std::begin(chamber), std::end(chamber), false);
            chamber[randomInt(0, 5)] = true;
            currentChamber = 0;
            player1Turn = true;
            gameOver = false;
            skipNextTurn = false;
            player1Items.clear();
            player2Items.clear();
            gameMessage = "Player 1's turn";
            std::cout << "\n=== GAME RESTARTED ===\n";
            updateHUD();
            printPlayerItems(true);
        }
        return;
    }

    for (int i = 0; i < 4; ++i)
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS)
            useItem(player1Turn, i);

    static bool leftPressed = false, rightPressed = false;

    if (skipNextTurn)
    {
        skipNextTurn = false;
        player1Turn = !player1Turn;
        gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
        printPlayerItems(player1Turn);
        updateHUD();
        return;
    }

    // Left Click: Shoot Opponent
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !leftPressed)
    {
        leftPressed = true;
        bool fired = chamber[currentChamber];
        currentChamber = (currentChamber + 1) % 6;

        if (fired)
        {
            gameMessage = player1Turn ? "P1 shot P2 - P1 Wins!" : "P2 shot P1 - P2 Wins!";
            std::cout << ">>> " << gameMessage << std::endl;
            gameOver = true;
        }
        else
        {
            std::cout << "Click! Empty chamber.\n";
            player1Turn = !player1Turn;
            gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
            printPlayerItems(player1Turn);
        }
        updateHUD();
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
        leftPressed = false;

    // Right Click: Shoot Self
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && !rightPressed)
    {
        rightPressed = true;
        bool fired = chamber[currentChamber];
        currentChamber = (currentChamber + 1) % 6;

        if (fired)
        {
            gameMessage = player1Turn ? "P1 shot self - P2 Wins!" : "P2 shot self - P1 Wins!";
            std::cout << ">>> " << gameMessage << std::endl;
            gameOver = true;
        }
        else
        {
            std::cout << "Click! You survived and found an item.\n";
            giveRandomItem(player1Turn);
            player1Turn = !player1Turn;
            gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
            printPlayerItems(player1Turn);
        }
        updateHUD();
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE)
        rightPressed = false;
}

void framebuffer_size_callback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow*, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }
    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    lastX = (float)xpos;
    lastY = (float)ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow*, double, double yoffset)
{
    camera.ProcessMouseScroll((float)yoffset);
}
