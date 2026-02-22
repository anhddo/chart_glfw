#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>
#include "ibkr_c_api.h"
#include <cjson/cJSON.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#define MAX_ITEMS 50

int scan(void) {
	printf("Connecting to TWS at 127.0.0.1:7495 ...\n");

	IbkrHandle h = ibkr_create("127.0.0.1", 7495, 0);
	ibkr_connect(h); /* waits ~1 s for the handshake */

	printf("Requesting scanner (TOP_PERC_GAIN, price > $5) ...\n");
	ibkr_start_scanner(h, 1, "TOP_PERC_GAIN", 5.0);

	/* Poll up to 30 s for results */
	CScannerItem items[MAX_ITEMS];
	int result = -1;
	int attempts = 0;
	while (result < 0 && attempts < 60) {
		SLEEP_MS(500);
		result = ibkr_poll_scanner(h, items, MAX_ITEMS);
		++attempts;
	}

	if (result < 0) {
		printf("No scanner results received.\n");
	} else {
		printf("\n=== Scanner Results (%d items) ===\n", result);
		printf("%-6s %-12s %-8s %-8s\n", "Rank", "Symbol", "Type", "Currency");
		printf("%-6s %-12s %-8s %-8s\n", "----", "------", "----", "--------");
		for (int i = 0; i < result; i++) {
			printf("%-6d %-12s %-8s %-8s\n",
				   items[i].rank,
				   items[i].symbol,
				   items[i].secType,
				   items[i].currency);
		}
	}

	ibkr_disconnect(h);
	SLEEP_MS(500);
	ibkr_destroy(h);
	return 0;
}




const unsigned int SCR_WIDTH = 2560;
const unsigned int SCR_HEIGHT = 1280;
int main(void) {
	scan();


	// Setup Platform/Renderer backends
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Chart GLFW", NULL, NULL);
	if (window == NULL)
	{
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	//glfwSetScrollCallback(window, Renderer::ScrollCallback);

	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		printf("failed to initialize GLAD\n");
		return -1;
	}

	ImGuiContext* ctx = igCreateContext(NULL);
	ImGuiIO* io = igGetIO_Nil();
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	igStyleColorsDark(igGetStyle());

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		igNewFrame();

		igShowDemoWindow(NULL);

		igRender();
		int w, h;
		glfwGetFramebufferSize(window, &w, &h);
		glViewport(0, 0, w, h);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	igDestroyContext(ctx);
	glfwTerminate();

	return 0;

}