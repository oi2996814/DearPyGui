﻿#include "Core/PythonUtilities/mvPythonModule.h"
#include "Core/PythonInterfaces/mvStdOutput.h"
#include "Core/mvWindow.h"
#include "Core/mvAppEditor.h"
#include "Platform/Windows/mvWindowsWindow.h"
#include <iostream>
#include <fstream>
#include "Core/PythonInterfaces/mvInterfaceRegistry.h"
#include "Core/PythonInterfaces/mvInterfaces.h"
#include <CLI11.hpp>
#include <iostream>

using namespace Marvel;

mvAppLog* mvAppLog::s_instance = nullptr;

class Application
{

public:

	Application(int argc, char* argv[]) : argc(argc), argv(argv)
	{

	}

	~Application()
	{
		Py_XDECREF(m);

		PyMem_RawFree(program);

		// shutdown the interpreter
		if (Py_FinalizeEx() < 0)
			exit(120);
	}

	void showConsole()
	{
		HWND hWnd = GetConsoleWindow();
		ShowWindow(hWnd, SW_SHOW);
	}

	void hideConsole()
	{
		HWND hWnd = GetConsoleWindow();
		ShowWindow(hWnd, SW_HIDE);
	}

	int parseCommandLine()
	{
		CLI::App app{ "Marvel Sandbox" };
		app.allow_windows_style_options();

		// options
		app.add_option("-a, --app", AppName, "Name of the python file (without extension)");
		app.add_option("-p, --path", PathName, "Path to app file (default is location of MarvelSandbox.exe)");

		// flags
		app.add_flag("-l, --logger", logger, "Show Logger");
		app.add_flag("-m, --metrics", metrics, "Show Metrics");
		app.add_flag("-s, --source", source, "Show Source");
		app.add_flag("-d, --documentation", documentation, "Show Documentation");
		app.add_flag("-e, --editor", editorMode, "Sets MarvelSandbox to Editor Mode");

		CLI11_PARSE(app, argc, argv);

		return 0;
		
	}

	void handlePaths()
	{
		PathName = PathName + ";python38.zip";

		program = Py_DecodeLocale(argv[0], NULL);
		if (program == NULL) {
			fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
			exit(1);
		}
		Py_SetProgramName(program);  /* optional but recommended */

		if (argc < 2) // ran from visual studio
		{
			ranFromVS = true;
			addedPath = std::string(MV_MAIN_DIR) + std::string("MarvelSandbox/");
			PathName = "python38.zip;../../MarvelSandbox";
		}
	}

	int initializePython()
	{
		// add our custom module
		PyImport_AppendInittab("sandboxout", &PyInit_embOut);

		// set path and start the interpreter
		wchar_t* deco = Py_DecodeLocale(PathName.c_str(), nullptr);
		Py_SetPath(deco);
		Py_NoSiteFlag = 1; // this must be set to 1

		Py_Initialize();
		if (!Py_IsInitialized())
		{
			printf("Error initializing Python interpreter\n");
			return 1;
		}
		PyEval_InitThreads();

		// import our custom module to capture stdout/stderr
		m = PyImport_ImportModule("sandboxout");
		PySys_SetObject("stdout", m);
		PySys_SetObject("stderr", m);

		return 0;
	}

	int run()
	{
		if (editorMode)
			return runEditorMode();

		if (errorMode)
			return runErrorMode();

		return runRegularMode();

	}

	int runEditorMode()
	{
		mvWindow* window = new mvWindowsWindow(mvAppEditor::GetAppEditor()->getWindowWidth(),
			mvAppEditor::GetAppEditor()->getWindowHeight(), true);
		window->show();
		window->run();

		return 0;
	}

	int runErrorMode()
	{
		PyErr_Print();
		mvApp::GetApp()->setOk(false);
		mvApp::GetApp()->showLogger();

		// create window
		mvWindow* window = new mvWindowsWindow(mvApp::GetApp()->getWindowWidth(), mvApp::GetApp()->getWindowHeight());
		window->show();
		window->run();
		delete window;

		return 0;
	}

	int runRegularMode()
	{
		// returns the dictionary object representing the module namespace
		PyObject* pDict = PyModule_GetDict(pModule); // borrowed reference
		mvApp::GetApp()->setModuleDict(pDict);
		std::string filename = addedPath + std::string(AppName) + ".py";
		mvApp::GetApp()->setFile(filename);
		PyEval_SaveThread(); // releases global lock
		mvApp::GetApp()->preRender();
		mvApp::GetApp()->setStarted();

		if (logger) mvApp::GetApp()->showLogger();
		if (source) mvApp::GetApp()->showSource();
		if (metrics) mvApp::GetApp()->showMetrics();
		if (documentation) mvApp::GetApp()->showDoc();

		// create window
		mvWindow* window = new mvWindowsWindow(mvApp::GetApp()->getWindowWidth(), mvApp::GetApp()->getWindowHeight());
		window->show();
		window->run();
		PyGILState_STATE gstate = PyGILState_Ensure();
		Py_XDECREF(pModule);
		delete window;

		return 0;
	}

	void logInformation()
	{
		// info
		mvAppLog::getLogger()->AddLog("[Sandbox Version] %0s\n", mvApp::getVersion());
		mvAppLog::getLogger()->AddLog("[Python Version] %0s\n", PY_VERSION);
		mvAppLog::getLogger()->AddLog("[ImGui Version] %0s\n", IMGUI_VERSION);
		mvAppLog::getLogger()->AddLog("[Compiler] MSVC version %0d\n", _MSC_VER);
	}

	void importModule()
	{
		// get module
		pModule = PyImport_ImportModule(AppName.c_str()); // new reference

		// check if error occurred
		if (!PyErr_Occurred() && pModule != nullptr)
			regularMode = true;

		else
			errorMode = true;
	}

public:

	int argc;
	char** argv;
	wchar_t* program;
	bool errorMode   = false;
	bool regularMode = false;
	PyObject* pModule = nullptr;
	PyObject* m = nullptr;

	bool ranFromVS = false;
	std::string addedPath;

	// options
	std::string AppName = "App";
	std::string PathName = "";

	// flags
	bool logger = false;
	bool metrics = false;
	bool source = false;
	bool documentation = false;
	bool editorMode = false;

};

int main(int argc, char* argv[])
{

	Application app(argc, argv);

	app.parseCommandLine();

#ifdef MV_RELEASE
	app.hideConsole();
#else
	app.showConsole();
#endif

	app.handlePaths();

	app.initializePython();

	app.logInformation();

	app.importModule();

	app.run();

	return 0;
}