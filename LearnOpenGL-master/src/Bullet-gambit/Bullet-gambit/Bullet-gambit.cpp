#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>
#include <learnopengl/shader.h>
#include <learnopengl/model.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ctime>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

// --- State and UI Structures ---
enum GameState {
	STATE_MENU,
	STATE_GAME,
	STATE_CREDITS,
	STATE_SUBMENU_START
};
GameState currentGameState = STATE_MENU;

struct MenuButton {
	unsigned int textureID;
	glm::vec2 position;
	glm::vec2 size;
	GameState nextState;
	bool isGameAction = false;
	int actionCode = 0;
	glm::vec3 color;
};

// --- Global Variables ---
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;
const float MIN_TURN_DELAY = 0.5f;
// Duration for short effects (Click.png, Got.png)
const float STATUS_IMAGE_DURATION = 0.5f;
// Duration for Player Turn Indicator visibility before auto-skip
const float TURN_SKIP_DURATION = 2.0f;

// Textures
unsigned int menuBackgroundTex = 0;
unsigned int startButtonTex = 0, creditButtonTex = 0, quitButtonTex = 0;
unsigned int twoPlayerButtonTex = 0, botButtonTex = 0;
unsigned int foeButtonTex = 0, safeButtonTex = 0;
unsigned int player1Tex = 0, player2Tex = 0;
unsigned int player1GotTex = 0, player2GotTex = 0;
unsigned int player1WonTex = 0, player2WonTex = 0;
unsigned int clickTex = 0;

std::vector<MenuButton> activeButtons;
unsigned int quadVAO = 0;
Shader* menuShader = nullptr;
Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f, lastFrame = 0.0f;
float lastActionTime = 0.0f;
Model* gunModel = nullptr;
GLFWwindow* g_window = nullptr;

// Game Data
bool player1Turn = true;
bool gameOver = false;
bool chamber[6];
int currentChamber = 0;
std::string gameMessage = "Player 1's turn";
float statusImageTime = 0.0f;
unsigned int currentStatusTex = 0;
// Flag to manage immediate hiding of the turn indicator when clicked
bool turnIndicatorSkippedManually = false;

// Item System
enum ItemType { ITEM_NONE = 0, ITEM_ROLL = 1, ITEM_MOVE_BULLET = 2, ITEM_SKIP = 3 };
std::vector<ItemType> player1Items;
std::vector<ItemType> player2Items;

// --- Function Prototypes ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void processInput(GLFWwindow* window);
void renderQuad();
unsigned int loadTexture(const char* path);
void setupMainMenu();
void setupStartSubMenu();
void setupGameButtons();
void handleGameAction(int action);
void startGameInit();
void updateHUD();
void giveRandomItem(bool forPlayer1);
void useItem(bool forPlayer1, int slot);
int randomInt(int min, int max) { return min + rand() % (max - min + 1); }

// --- Helper for turn indicator status ---
bool isPlayerTurnIndicator(unsigned int texID) {
	return texID == player1Tex || texID == player2Tex;
}

// --- Item Helpers ---
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

// --- Game Logic Functions ---
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

	title += "| Action: Click Foe/Safe Buttons | 1-4: Use Item | R: Restart";
	glfwSetWindowTitle(g_window, title.c_str());
}

void giveRandomItem(bool forPlayer1)
{
	if (forPlayer1 && player1Items.size() >= 4) return;
	if (!forPlayer1 && player2Items.size() >= 4) return;

	int itemID = randomInt(1, 3);
	ItemType newItem = static_cast<ItemType>(itemID);

	if (forPlayer1) player1Items.push_back(newItem);
	else player2Items.push_back(newItem);

	std::cout << (forPlayer1 ? "Player 1" : "Player 2") << " got item: " << itemName(newItem) << std::endl;
	updateHUD();
}

void useItem(bool forPlayer1, int slot)
{
	auto& items = forPlayer1 ? player1Items : player2Items;
	if (slot < 0 || slot >= (int)items.size()) return;

	ItemType item = items[slot];
	items.erase(items.begin() + slot);

	lastActionTime = (float)glfwGetTime();

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
		std::cout << "Bullet moved forward one chamber (now at index " << currentChamber << ")." << std::endl;
		break;
	case ITEM_SKIP:
		player1Turn = !player1Turn;
		gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
		std::cout << (forPlayer1 ? "Player 1" : "Player 2") << " used SKIP. Turn immediately passes to the opponent." << std::endl;
		break;
	default: break;
	}

	// Set turn indicator if SKIP item was used
	if (item == ITEM_SKIP) {
		currentStatusTex = player1Turn ? player1Tex : player2Tex;
		statusImageTime = (float)glfwGetTime();
	}

	updateHUD();
}

void startGameInit()
{
	std::fill(std::begin(chamber), std::end(chamber), false);
	chamber[randomInt(0, 5)] = true;
	currentChamber = 0;
	player1Turn = true;
	gameOver = false;
	player1Items.clear();
	player2Items.clear();
	lastActionTime = (float)glfwGetTime();
	turnIndicatorSkippedManually = false; // Reset the flag

	giveRandomItem(true);
	giveRandomItem(false);

	gameMessage = "Player 1's turn";
	currentStatusTex = player1Tex; // Start with the click-to-proceed indicator
	statusImageTime = (float)glfwGetTime();
	updateHUD();
}

void handleGameAction(int action)
{
	if (gameOver) return;

	lastActionTime = (float)glfwGetTime();
	statusImageTime = (float)glfwGetTime();
	turnIndicatorSkippedManually = false; // Reset the flag

	bool fired = chamber[currentChamber];
	currentChamber = (currentChamber + 1) % 6;
	bool turnSwitched = false;

	if (action == 1) // Shoot Opponent (Foe button)
	{
		if (fired)
		{
			currentStatusTex = player1Turn ? player2GotTex : player1GotTex;
			gameMessage = player1Turn ? "P1 shot P2 - P1 Wins!" : "P2 shot P1 - P2 Wins!";
			gameOver = true;
		}
		else
		{
			currentStatusTex = clickTex; // Click.png feedback (0.5s)
			player1Turn = !player1Turn;
			gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
			turnSwitched = true;
		}
	}
	else if (action == 2) // Shoot Self (Safe button)
	{
		if (fired)
		{
			currentStatusTex = player1Turn ? player1GotTex : player2GotTex;
			gameMessage = player1Turn ? "P1 shot self - P2 Wins!" : "P2 shot self - P1 Wins!";
			gameOver = true;
		}
		else
		{
			currentStatusTex = clickTex; // Click.png feedback (0.5s)
			giveRandomItem(player1Turn);
			player1Turn = !player1Turn;
			gameMessage = player1Turn ? "Player 1's turn" : "Player 2's turn";
			turnSwitched = true;
		}
	}

	// After a successful action/turn switch (which means fired was false), set the next indicator 
	// to appear after the short Click.png effect finishes.
	if (turnSwitched) {
		// We rely on the Click.png to clear itself after STATUS_IMAGE_DURATION (0.5s).
		// The main loop will then check currentStatusTex == 0.
	}

	updateHUD();
}

// --- OpenGL/Utility Functions ---
unsigned int loadTexture(const char* path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	int width, height, nrComponents;

	std::string fullPath = FileSystem::getPath(path);
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &nrComponents, 0);

	if (data)
	{
		GLenum format = (nrComponents == 1) ? GL_RED : (nrComponents == 4) ? GL_RGBA : GL_RGB;
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load: " << fullPath << std::endl;
		unsigned char fallbackData[] = { 255, 255, 255, 255 };
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallbackData);
	}
	return textureID;
}

void renderQuad()
{
	if (quadVAO == 0)
	{
		float quadVertices[] = {
			-1.0f,	1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,	1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		unsigned int quadVBO;
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

// --- Menu Setup Functions ---
#define BUTTON_WIDTH_NORM (300.0f / SCR_WIDTH)
#define BUTTON_HEIGHT_NORM (100.0f / SCR_HEIGHT)
#define MARGIN_X (0.05f)
#define MARGIN_Y (0.05f)
#define BUTTON_SPACING (BUTTON_HEIGHT_NORM + 0.03f)

void setupMainMenu() {
	activeButtons.clear();
	activeButtons.push_back({ quitButtonTex, glm::vec2(MARGIN_X, MARGIN_Y), glm::vec2(BUTTON_WIDTH_NORM, BUTTON_HEIGHT_NORM), (GameState)-1, false, 0, glm::vec3(0.8f, 0.2f, 0.2f) });
	activeButtons.push_back({ creditButtonTex, glm::vec2(MARGIN_X, MARGIN_Y + BUTTON_SPACING), glm::vec2(BUTTON_WIDTH_NORM, BUTTON_HEIGHT_NORM), STATE_CREDITS, false, 0, glm::vec3(0.2f, 0.4f, 0.8f) });
	activeButtons.push_back({ startButtonTex, glm::vec2(MARGIN_X, MARGIN_Y + 2.0f * BUTTON_SPACING), glm::vec2(BUTTON_WIDTH_NORM, BUTTON_HEIGHT_NORM), STATE_SUBMENU_START, false, 0, glm::vec3(0.2f, 0.8f, 0.2f) });
}

void setupStartSubMenu() {
	setupMainMenu();
	float startButtonBaseY = MARGIN_Y + 2.0f * BUTTON_SPACING;
	float subButtonX = MARGIN_X + BUTTON_WIDTH_NORM + 0.02f;
	float subButtonWidth = BUTTON_WIDTH_NORM * 0.6f;
	float subButtonHeight = BUTTON_HEIGHT_NORM * 0.6f;
	float subButtonVOffset = (BUTTON_HEIGHT_NORM - subButtonHeight) / 2.0f;

	activeButtons.push_back({ twoPlayerButtonTex, glm::vec2(subButtonX, startButtonBaseY + subButtonVOffset), glm::vec2(subButtonWidth, subButtonHeight), STATE_GAME, false, 0, glm::vec3(0.2f, 0.7f, 0.7f) });
	activeButtons.push_back({ botButtonTex, glm::vec2(subButtonX + subButtonWidth + 0.01f, startButtonBaseY + subButtonVOffset), glm::vec2(subButtonWidth, subButtonHeight), STATE_SUBMENU_START, false, 0, glm::vec3(0.5f, 0.5f, 0.5f) });
}

void setupGameButtons() {
	activeButtons.clear();
	float btnW = BUTTON_WIDTH_NORM * 1.2f;
	float btnH = BUTTON_HEIGHT_NORM * 1.2f;
	float H_SPACING = 0.25f;
	float TotalWidth = btnW * 2.0f + H_SPACING;
	float startX = (1.0f - TotalWidth) / 2.0f;
	float btnY = MARGIN_Y + 0.1f;

	activeButtons.push_back({ foeButtonTex, glm::vec2(startX, btnY), glm::vec2(btnW, btnH), STATE_GAME, true, 1, glm::vec3(0.8f, 0.2f, 0.2f) });
	activeButtons.push_back({ safeButtonTex, glm::vec2(startX + btnW + H_SPACING, btnY), glm::vec2(btnW, btnH), STATE_GAME, true, 2, glm::vec3(0.2f, 0.8f, 0.2f) });
}

// ====================================================
// === MAIN FUNCTION ===
// ====================================================
int main()
{
	srand(static_cast<unsigned>(time(0)));

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	g_window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Bullet Gambit", NULL, NULL);
	glfwMakeContextCurrent(g_window);
	glfwSetFramebufferSizeCallback(g_window, framebuffer_size_callback);
	glfwSetCursorPosCallback(g_window, mouse_callback);
	glfwSetScrollCallback(g_window, scroll_callback);
	glfwSetMouseButtonCallback(g_window, mouse_button_callback);
	glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Shader ourShader("Bullet-gambit.vs", "Bullet-gambit.fs");
	Shader newMenuShader("menu_2d.vs", "menu_2d.fs");
	menuShader = &newMenuShader;

	try {
		gunModel = new Model(FileSystem::getPath("resources/objects/gun/fullgun.dae"));
	}
	catch (const std::exception& e) {
		std::cerr << "Error loading gun model: " << e.what() << std::endl;
		gunModel = nullptr;
	}

	// Load Textures
	menuBackgroundTex = loadTexture("resources/textures/menu/menu.png");
	startButtonTex = loadTexture("resources/textures/menu/Start button.png");
	creditButtonTex = loadTexture("resources/textures/menu/Credit button.png");
	quitButtonTex = loadTexture("resources/textures/menu/Quit button.png");
	twoPlayerButtonTex = loadTexture("resources/textures/menu/2 player button.png");
	botButtonTex = loadTexture("resources/textures/menu/bot button.png");
	safeButtonTex = loadTexture("resources/textures/menu/safe button.png");
	foeButtonTex = loadTexture("resources/textures/menu/foe button.png");

	player1Tex = loadTexture("resources/textures/menu/Player1.png");
	player2Tex = loadTexture("resources/textures/menu/Player2.png");
	player1GotTex = loadTexture("resources/textures/menu/Player1got.png");
	player2GotTex = loadTexture("resources/textures/menu/Player2got.png");
	player1WonTex = loadTexture("resources/textures/menu/Player1Won.png");
	player2WonTex = loadTexture("resources/textures/menu/Player2Won.png");
	clickTex = loadTexture("resources/textures/menu/Click.png");

	setupMainMenu();

	// --- Game Loop ---
	while (!glfwWindowShouldClose(g_window))
	{
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		processInput(g_window);

		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (currentGameState == STATE_MENU || currentGameState == STATE_SUBMENU_START)
		{
			// Menu Rendering (Unchanged)
			glDisable(GL_DEPTH_TEST);
			menuShader->use();
			menuShader->setFloat("alpha", 1.0f);

			glBindTexture(GL_TEXTURE_2D, menuBackgroundTex);
			menuShader->setVec2("offset", glm::vec2(0.0f, 0.0f));
			menuShader->setVec2("scale", glm::vec2(1.0f, 1.0f));
			menuShader->setVec3("color", glm::vec3(0.0f, 0.0f, 0.1f));
			renderQuad();

			for (const auto& button : activeButtons)
			{
				menuShader->setVec2("offset", button.position);
				menuShader->setVec2("scale", button.size);
				glBindTexture(GL_TEXTURE_2D, button.textureID);
				menuShader->setVec3("color", button.color);
				renderQuad();
			}
		}
		else if (currentGameState == STATE_CREDITS)
		{
			glDisable(GL_DEPTH_TEST);
			glClearColor(0.0f, 0.0f, 0.3f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		else if (currentGameState == STATE_GAME)
		{
			bool inCooldown = ((float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY);

			// 3D Rendering (Unchanged)
			glEnable(GL_DEPTH_TEST);
			ourShader.use();
			glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
			glm::mat4 view = camera.GetViewMatrix();
			ourShader.setMat4("projection", projection);
			ourShader.setMat4("view", view);

			if (gunModel)
			{
				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
				model = glm::rotate(model, glm::radians(135.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
				model = glm::rotate(model, (float)glfwGetTime() * glm::radians(20.0f), glm::vec3(0.0f, 0.0f, 1.0f));
				model = glm::scale(model, glm::vec3(0.5f));
				ourShader.setMat4("model", model);
				gunModel->Draw(ourShader);
			}

			// 2D UI Rendering
			glDisable(GL_DEPTH_TEST);
			menuShader->use();
			glActiveTexture(GL_TEXTURE0);
			menuShader->setInt("image", 0);

			// Status Image Logic (Turn indicator/Shot effect)
			float elapsed = (float)glfwGetTime() - statusImageTime;
			float alpha = 1.0f;
			bool showActionUI = true;

			// 1. Handle Game Over Transition: Shot -> Winner
			if (gameOver) {
				// If showing 'Got', transition to 'Won' after 0.5s
				if (elapsed > STATUS_IMAGE_DURATION && (currentStatusTex == player1GotTex || currentStatusTex == player2GotTex)) {
					currentStatusTex = (currentStatusTex == player1GotTex) ? player2WonTex : player1WonTex;
				}

				// Always hide action buttons when Game Over is active
				if (currentStatusTex != 0) showActionUI = false;
			}
			// 2. Handle Non-Game-Over Status Effects
			else if (currentStatusTex != 0) {

				// Player Turn Indicator (Player1.png/Player2.png): Hide on click OR after 2.0s
				if (isPlayerTurnIndicator(currentStatusTex)) {
					showActionUI = false; // Hide buttons if turn indicator is showing

					// Auto-skip logic after 2.0 seconds
					if (elapsed > TURN_SKIP_DURATION) {
						currentStatusTex = 0; // Clear it after 2.0s, allows buttons to show
					}
				}
				// Click feedback (Click.png): Hide after duration (0.5s)
				else if (currentStatusTex == clickTex) {
					if (elapsed > STATUS_IMAGE_DURATION) {
						currentStatusTex = 0; // Hide image after 0.5s

						// FIX: When Click.png clears naturally, it means a turn just switched.
						// We MUST set the next turn indicator here, otherwise the action buttons 
						// will flash briefly before the indicator is supposed to come up.
						currentStatusTex = player1Turn ? player1Tex : player2Tex;
						statusImageTime = (float)glfwGetTime();
						showActionUI = false; // The next indicator is now showing.

					}
					else {
						showActionUI = false; // Image showing, hide buttons
					}
				}
			}

			// 3. Manual Skip Handling (Removed the auto-restore logic)
			// The only logic here is to ensure action buttons are displayed after a manual skip.
			if (currentStatusTex == 0 && !gameOver) {

				if (turnIndicatorSkippedManually) {
					// Scenario: Player just clicked to dismiss PlayerX.png.
					// Reset the flag and ensure action buttons are visible. DO NOT set currentStatusTex.
					turnIndicatorSkippedManually = false;
					showActionUI = true;
				}
				// If currentStatusTex is 0, and not a manual skip, buttons are already set to show.
			}


			// Draw Status Image
			if (currentStatusTex != 0)
			{
				// Click effect fade out
				if (currentStatusTex == clickTex && elapsed > 0.0f) {
					alpha = 1.0f - std::min(1.0f, elapsed / STATUS_IMAGE_DURATION);
				}

				menuShader->setVec2("offset", glm::vec2(0.0f, 0.0f));
				menuShader->setVec2("scale", glm::vec2(1.0f, 1.0f));
				glBindTexture(GL_TEXTURE_2D, currentStatusTex);
				menuShader->setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));
				menuShader->setFloat("alpha", alpha);
				renderQuad();
			}

			// Draw Foe/Safe Action Buttons
			menuShader->setFloat("alpha", 1.0f);

			if (showActionUI && !gameOver)
			{
				for (const auto& button : activeButtons)
				{
					if (button.isGameAction) {
						glm::vec3 finalColor = button.color;
						if (inCooldown) {
							finalColor = finalColor * 0.5f + glm::vec3(0.3f, 0.3f, 0.3f) * 0.5f;
						}

						menuShader->setVec2("offset", button.position);
						menuShader->setVec2("scale", button.size);
						glBindTexture(GL_TEXTURE_2D, button.textureID);
						menuShader->setVec3("color", finalColor);
						renderQuad();
					}
				}
			}
			glEnable(GL_DEPTH_TEST);
		}

		glfwSwapBuffers(g_window);
		glfwPollEvents();
	}

	if (gunModel) delete gunModel;
	glfwTerminate();
	return 0;
}

// ====================================================
// === Callbacks ===
// ====================================================

void processInput(GLFWwindow* window)
{
	// ESC input handling
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		if (currentGameState == STATE_GAME) {
			currentGameState = STATE_MENU;
			setupMainMenu();
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		else if (currentGameState == STATE_CREDITS || currentGameState == STATE_SUBMENU_START) {
			currentGameState = STATE_MENU;
			setupMainMenu();
		}
		else if (currentGameState == STATE_MENU) {
			glfwSetWindowShouldClose(window, true);
		}
	}

	if (currentGameState != STATE_GAME) return;

	if (gameOver)
	{
		// Press 'R' to reset game and go back to P1 turn indicator
		if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) startGameInit();
		return;
	}

	// Item use input
	if ((float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY) return;

	for (int i = 0; i < 4; ++i)
		if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS)
			useItem(player1Turn, i);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		float normX = (float)xpos / SCR_WIDTH;
		float normY = 1.0f - ((float)ypos / SCR_HEIGHT);

		bool buttonClicked = false;

		for (const auto& btn : activeButtons)
		{
			float minX = btn.position.x;
			float maxX = btn.position.x + btn.size.x;
			float minY = btn.position.y;
			float maxY = btn.position.y + btn.size.y;

			if (normX >= minX && normX <= maxX && normY >= minY && normY <= maxY)
			{
				buttonClicked = true;

				if (btn.isGameAction) {
					if (isPlayerTurnIndicator(currentStatusTex) && currentGameState == STATE_GAME) {
						// The click was on a button, but the indicator is up. Ignore the action/click.
						return;
					}

					if (currentGameState == STATE_GAME && (float)glfwGetTime() - lastActionTime < MIN_TURN_DELAY) {
						return;
					}
					if (!gameOver) handleGameAction(btn.actionCode);
					return;
				}

				// Menu button handling (Unchanged)
				if (btn.nextState == (GameState)-1) {
					glfwSetWindowShouldClose(window, true);
					return;
				}

				currentGameState = btn.nextState;

				if (currentGameState == STATE_SUBMENU_START) {
					setupStartSubMenu();
				}
				else if (currentGameState == STATE_MENU) {
					setupMainMenu();
				}
				else if (currentGameState == STATE_GAME) {
					startGameInit();
					setupGameButtons();
				}

				return;
			}
		}

		// ** Click to Skip Player Turn Indicator **
		if (!buttonClicked && currentGameState == STATE_GAME)
		{
			if (isPlayerTurnIndicator(currentStatusTex))
			{
				currentStatusTex = 0;
				// Set the skip flag, telling the main loop NOT to immediately restore PlayerX.png
				turnIndicatorSkippedManually = true;
			}
		}
	}
}

void framebuffer_size_callback(GLFWwindow*, int width, int height)
{
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow*, double xpos, double ypos)
{
	if (currentGameState == STATE_GAME)
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
	}
	else
	{
		lastX = (float)xpos;
		lastY = (float)ypos;
		if (firstMouse) firstMouse = false;
	}
}

void scroll_callback(GLFWwindow*, double, double yoffset)
{
	if (currentGameState == STATE_GAME) camera.ProcessMouseScroll((float)yoffset);
}