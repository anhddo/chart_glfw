#ifdef _WIN32
// This tells the linker to use main() even if we are in Windows Subsystem mode
#pragma comment(linker, "/entry:mainCRTStartup")
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // first

#include "renderer.h"
#include "data_api/ikbr/ibkr.h"
//#include "data_api/ikbr/MainClientDemo.h"

class AppController {
	public:
	AppController() {}
	~AppController() {}
	int option = 0;
	void run() {
		if (option == 1) {
			IbkrClient client;
			client.test();
		}
		else {
			Renderer renderer;
			renderer.draw();
		}
	}
};


int main() {
	AppController app;
	app.option = 1; // Set to 1 to run IBKR client test, 0 to run renderer
	app.run();

}