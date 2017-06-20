#include <bx/bx.h>
#include <bx/uint32_t.h>
#include <bx/string.h>
#include <bgfx/bgfx.h>
#include <bx/crtimpl.h>

#include "entry/entry.h"
#include "entry/input.h"
#include "entry/cmd.h"
#include "imgui/imgui.h"
#include "common.h"
#include "bgfx_utils.h"
#include "imgui_ext.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/videoio.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <array>

namespace {

    template<class T>
    T base_name(T const & path, T const & delims = "/\\") {
        return path.substr(path.find_last_of(delims) + 1);
    }

    template<class T>
    T remove_extension(T const & filename) {
        typename T::size_type const p(filename.find_last_of('.'));
        return p > 0 && p != T::npos ? filename.substr(0, p) : filename;
    }

	struct CameraInfo {
		int32_t     id;
		cv::Size    frame_size;
		int32_t     fps;
	};

	std::vector<CameraInfo> enumerateCameras() {
		std::vector<CameraInfo> cameras;

		int32_t device_counts = 0;
		while (true) {
			cv::VideoCapture camera;
			if (!camera.open(device_counts)) {
				break;
			}

			cameras.push_back({
				device_counts,
				cv::Size(
					(int32_t)camera.get(CV_CAP_PROP_FRAME_WIDTH),
					(int32_t)camera.get(CV_CAP_PROP_FRAME_HEIGHT)),
				(int32_t)camera.get(CV_CAP_PROP_FPS)
			});
			
			++device_counts;
			camera.release();
		}

		return cameras;
	}

	void printCameraInfo(const CameraInfo& camera) {
		std::cout << std::endl
			<< "Camera id: " << camera.id
			<< " (" << camera.frame_size.width
			<< "x" << camera.frame_size.height
			<< "@" << camera.fps << ")"
			<< std::endl;
	}

	std::string cvTypeToString(int type) {
		std::string r;

		uchar depth = type & CV_MAT_DEPTH_MASK;
		uchar chans = 1 + (type >> CV_CN_SHIFT);

		switch ( depth ) {
			case CV_8U:  r = "8U"; break;
			case CV_8S:  r = "8S"; break;
			case CV_16U: r = "16U"; break;
			case CV_16S: r = "16S"; break;
			case CV_32S: r = "32S"; break;
			case CV_32F: r = "32F"; break;
			case CV_64F: r = "64F"; break;
			default:     r = "User"; break;
		}

		r += "C";
		r += (chans+'0');

		return r;
	}

	ImVec4 cvVec4bToImVec4f(const cv::Vec4b& color) {
		ImU32 u32Color = (color[0]) | (color[1] << 8) | (color[2] << 16) | (color[3] << 24);
		return ImGui::ColorConvertU32ToFloat4(u32Color);
	}

	ImVec4 cvVec3bToImVec4f(const cv::Vec3b& color, uint8_t alpha = 0xff) {
		ImU32 u32Color = (color[0]) | (color[1] << 8) | (color[2] << 16) | (alpha << 24);
		return ImGui::ColorConvertU32ToFloat4(u32Color);
	}

	// ImVec3 cvVec3bToImVec3f(const cv::Vec3b& color) {
	// 	ImU32 u32Color = (color[0]) | (color[1] << 8) | (color[2] << 16));
	// 	return ImGui::ColorConvertU32ToFloat4(u32Color);
	// }
}

class ShowGUI : public entry::AppI {

	void setupGUIStyle() {

		// Fonts
		//ImGuiIO& io = ImGui::GetIO();
		//io.Fonts->Clear();
		//io.Fonts->AddFontDefault();
		// io.Fonts->AddFontFromFileTTF("font/droidsans.ttf", 16);
		// io.Fonts->AddFontFromFileTTF("font/chp-fire.ttf", 16);
		// io.Fonts->AddFontFromFileTTF("font/roboto-regular.ttf", 16);
		// io.Fonts->AddFontFromFileTTF("font/ruritania.ttf", 16);
		// io.Fonts->AddFontFromFileTTF("font/signika-regular.ttf", 16);
		//io.Fonts->Build();
		//io.FontDefault = io.Fonts->Fonts[2];

		// Style
		ImGuiStyle& style = ImGui::GetStyle();
		style.GrabRounding = 0;					// Radius of grabs corners rounding. Set to 0.0f to have rectangular slider grabs.
		style.ScrollbarRounding = 2;			// Radius of grab corners for scrollbar
		style.FrameRounding = 2;				// Radius of frame corners rounding. Set to 0.0f to have rectangular frame (used by most widgets).
		style.WindowRounding = 2;				// Radius of window corners rounding. Set to 0.0f to have rectangular windows
		style.WindowPadding = ImVec2(2, 2);		// Padding within a window
		style.ChildWindowRounding = 0;	        // Radius of child window corners rounding. Set to 0.0f to have rectangular windows
		style.FramePadding = ImVec2(0,0); 		// Padding within a framed rectangle (used by most widgets)
    	style.ItemSpacing = ImVec2(1,1);  		// Horizontal and vertical spacing between widgets/lines
    	style.ItemInnerSpacing = ImVec2(3,3);   // Horizontal and vertical spacing between within elements of a composed widget (e.g. a slider and its label)
    	//style.IndentSpacing;              	// Horizontal indentation when e.g. entering a tree node. Generally == (FontSize + FramePadding.x*2).
    
		// Colors
		style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.84f, 0.84f, 0.84f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.59f, 0.59f, 0.59f, 0.90f);
		style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.10f, 0.90f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.70f, 0.70f, 0.70f, 0.65f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.15f, 0.15f, 0.15f, 0.09f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.78f, 0.80f, 0.80f, 0.30f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.37f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.60f, 0.82f, 0.00f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.88f, 0.88f, 0.88f, 0.45f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.82f, 0.82f, 0.82f, 0.90f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 0.91f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.53f, 0.53f, 0.53f, 0.67f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.53f, 0.53f, 0.53f, 0.82f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 0.00f, 0.00f, 0.15f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.82f, 0.82f, 0.82f, 0.67f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.22f, 0.60f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_ComboBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.99f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 0.50f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.22f, 0.60f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.82f, 0.82f, 0.82f, 0.67f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.81f, 0.82f, 0.82f, 0.77f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.60f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.22f, 0.60f, 0.82f, 0.50f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.60f, 0.82f, 0.70f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.60f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_Column] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_ColumnHovered] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.22f, 0.60f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
		style.Colors[ImGuiCol_CloseButton] = ImVec4(0.53f, 0.53f, 0.53f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.82f, 0.42f, 0.42f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.22f, 0.60f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.65f, 0.22f, 0.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.93f, 0.52f, 0.02f, 0.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.92f, 0.82f, 0.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.22f, 0.60f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.20f, 0.20f, 0.22f);
	}

	void initGUI(int _argc, char** _argv) {
		// Initialise GUI
		imguiCreate();
		setupGUIStyle();
	}
	
	void initOpenCV(int _argc, char** _argv) {
		try {
			std::string options =
				"{help h usage| |Program usage}"
				"{info i| |OpenCV build info}"
				"{enum e| |Enumerates available cameras|}"
				"{@camera|0|Camera to show|}"
				"{@width|640|Desired frame width|}"
				"{@height|360|Desired frame height|}"
				"{@fps|60|Desired frame-rate|}";

			cv::CommandLineParser parser(_argc, _argv, options);
			parser.about("Shows the view of the chosen camera");

			if (parser.has("help")) {
				parser.printMessage();
				std::exit(EXIT_SUCCESS);
			}

			if (parser.has("info")) {
				std::cout << cv::getBuildInformation() << std::endl;
				std::exit(EXIT_SUCCESS);
			}

			if (!parser.check()) {
				parser.printErrors();
				std::exit(EXIT_FAILURE);
			}

			// Users wants to know the cameras availables
			if (parser.has("e")) {
				auto cameras = enumerateCameras();

				std::cout << "Available cameras: " << std::endl;
				for (auto camera : cameras) {
					printCameraInfo(camera);
				}

				std::cout << std::flush;
				std::exit(EXIT_SUCCESS);
			}

			// Create a camera info for the given command line's arguments
			CameraInfo ci = {
				parser.get<int32_t>("@camera"),
				cv::Size(
					parser.get<int32_t>("@width"),
					parser.get<int32_t>("@height")),
				parser.get<int32_t>("@fps")
			};

			if (!m_videoCapture.open(ci.id)) {
				std::cerr << "Camera " << ci.id << " is not available!" << std::endl;
				parser.printMessage();
				std::exit(EXIT_FAILURE);
			}

			m_videoCapture.set(CV_CAP_PROP_FRAME_WIDTH, (double)ci.frame_size.width);
			m_videoCapture.set(CV_CAP_PROP_FRAME_HEIGHT, (double)ci.frame_size.height);
			m_videoCapture.set(CV_CAP_PROP_FPS, (double)ci.fps);

			m_cameraInfo = {
				ci.id,
				cv::Size(
					(int32_t)m_videoCapture.get(CV_CAP_PROP_FRAME_WIDTH),
					(int32_t)m_videoCapture.get(CV_CAP_PROP_FRAME_HEIGHT)),
				(int32_t)m_videoCapture.get(CV_CAP_PROP_FPS)
			};

			// Print caps of current camera
        	printCameraInfo(m_cameraInfo);

		} catch (cv::Exception& e) {
        	std::cerr << e.what() << std::endl;
        	std::exit(EXIT_FAILURE);
    	}
	}

	void initBgfx(int _argc, char** _argv) {
		Args args(_argc, _argv);
        m_progName = (bx::baseName(_argv[0]));

		m_width  = 1280;
		m_height = 720;
		m_debug  = BGFX_DEBUG_TEXT;
		m_reset  = BGFX_RESET_VSYNC;

		bgfx::init(args.m_type, args.m_pciId);
		bgfx::reset(m_width, m_height, m_reset);

		// Enable debug text.
		bgfx::setDebug(m_debug);

		// Set view 0 clear state.
		bgfx::setViewClear(0
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0x303030ff
			, 1.0f
			, 0
		);
	}

	static int cmdQuit(CmdContext* /*_context*/, void* _userData, int /*_argc*/, char const* const* /*_argv*/)
	{
		auto* _this = static_cast<ShowGUI*>(_userData);
		_this->quit();

		return EXIT_SUCCESS;
	}

	static int cmdShow(CmdContext* /*_context*/, void* _userData, int _argc, char const* const* _argv)
	{
		if (_argc > 1)
		{
			auto* _this = static_cast<ShowGUI*>(_userData);

			if (0 == bx::strCmp(_argv[1], "camera")) {
				_this->toggleState(SHOW_CAMERA);
				return EXIT_SUCCESS;
			}
			else if (0 == bx::strCmp(_argv[1], "lab")) {
				_this->removeState(COLOR_SPACE_ALL);
				_this->addState(COLOR_SPACE_Lab);
				return EXIT_SUCCESS;
			}
			else if (0 == bx::strCmp(_argv[1], "hsv")) {
				_this->removeState(COLOR_SPACE_ALL);
				_this->toggleState(COLOR_SPACE_HSV);
				return EXIT_SUCCESS;
			}
			else if (0 == bx::strCmp(_argv[1], "ycrcb")) {
				_this->removeState(COLOR_SPACE_ALL);
				_this->toggleState(COLOR_SPACE_YCrCb);
				return EXIT_SUCCESS;
			}
			else if (0 == bx::strCmp(_argv[1], "rgb")) {
				_this->removeState(COLOR_SPACE_ALL);
				_this->toggleState(COLOR_SPACE_RGB);
				return EXIT_SUCCESS;
			}
		}

		return EXIT_FAILURE;
	}

	virtual void init(int _argc, char** _argv) override	{
		initOpenCV(_argc, _argv);
		initBgfx(_argc, _argv);
		initGUI(_argc, _argv);

		// Create the texture to hold camera input image
		m_texRGBA = bgfx::createTexture2D(
			m_cameraInfo.frame_size.width,					// width
			m_cameraInfo.frame_size.height,					// height
			false, 											// no mip-maps
			1,												// number of layers
			bgfx::TextureFormat::Enum::RGBA8,				// format
			BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP,	// flags
			nullptr											// mutable
		);

		// Create textures for displaing the channels separately
		for (auto& tex : m_texChannels) {
			tex = bgfx::createTexture2D(
				m_cameraInfo.frame_size.width,					// width
				m_cameraInfo.frame_size.height,					// height
				false, 											// no mip-maps
				1,												// number of layers
				bgfx::TextureFormat::Enum::RGBA8,				// format
				BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP,	// flags
				nullptr											// mutable
			);
		}

		static const InputBinding bindings[] =
		{
			{ entry::Key::Esc, 	entry::Modifier::None,  		1, NULL, "quit" 		},
			{ entry::Key::KeyD,	entry::Modifier::LeftCtrl,  	1, NULL, "show camera" 	},
			{ entry::Key::KeyD, entry::Modifier::RightCtrl,		1, NULL, "show camera" 	},
			{ entry::Key::KeyR,	entry::Modifier::None,  		1, NULL, "show rgb" 	},
			{ entry::Key::KeyY,	entry::Modifier::None,  		1, NULL, "show ycrcb" 	},
			{ entry::Key::KeyH,	entry::Modifier::None,  		1, NULL, "show hsv" 	},
			{ entry::Key::KeyL,	entry::Modifier::None,  		1, NULL, "show lab"		},

			INPUT_BINDING_END
		};

		// Add bindings and commands
		cmdAdd("quit", cmdQuit, this);
		cmdAdd("show", cmdShow, this);

		inputAddBindings("showgui_bindings", bindings);

		// Set initial states
		m_states = NONE;
		addState(SHOW_CAMERA);

		bx::memSet(&m_selectedColor, 0x0, sizeof(m_selectedColor));
		m_timeOffset = bx::getHPCounter();
	}

	virtual int shutdown() override	{
		imguiDestroy();
		
		bgfx::destroyTexture(m_texRGBA);
		for (auto tex : m_texChannels) {
			bgfx::destroyTexture(tex);
		}

		bgfx::shutdown();
		return EXIT_SUCCESS;
	}

	static void updateImageToTexture(const cv::Mat& image, bgfx::TextureHandle texture) {
		size_t imageSize = image.total() * image.elemSize();
		size_t imagePitch = image.cols * image.elemSize();
	
		// !!ATTENTION!!: Not thread-safe!
		static std::vector<uint8_t> pixels;
		pixels.resize(imageSize);
		
		// If the grabbed frame is continuous in memory,
		// then copying pixels is straight forward.
		if (image.isContinuous()) {
			bx::memCopy(pixels.data(), image.data, imageSize);
		} else {
			// We need to copy one row at a time otherwise.
			auto *p = pixels.data();
			for(int32_t i  = 0; i < image.rows; i++){
				bx::memCopy(p, image.ptr(i), imagePitch);
				p += imagePitch;
			}
		}
		
		// Data will be free by bgfx once it has finished with it
		auto* imageMem = bgfx::copy(pixels.data(), uint32_t(imageSize));
		
		// Copy camera frame into the texture
		bgfx::updateTexture2D(
			texture,	// texture handle
			0, 0, 		// mip, layer
			0, 0,		// start x, y
			image.cols,	// width
			image.rows,	// height
			imageMem,	// memory
			imagePitch	// pitch
		);
	}

	virtual bool update() override	{
		//try {
			if (!hasState(EXIT_REQUEST) &&
				!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState) ) {

				int64_t now = bx::getHPCounter();
				static int64_t last = now;
				const int64_t frameTime = now - last;
				last = now;
				const double freq = double(bx::getHPFrequency());
				const double toMs = 1000.0/freq;

				float time = (float)((now-m_timeOffset)/double(bx::getHPFrequency()));

				// Set view 0 default viewport.
				bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height));

				// This dummy draw call is here to make sure that view 0 is cleared
				// if no other draw calls are submitted to view 0.
				bgfx::touch(0);

				// Use debug font to print information about this example.
				bgfx::dbgTextClear();

				bgfx::dbgTextPrintf(0, 1, 0x4f, "Program: Show Camera");
				bgfx::dbgTextPrintf(0, 2, 0x6f, "Description: Rendering captured camera frames into different color spaces.");
				bgfx::dbgTextPrintf(0, 3, 0x8f, "Frame time: % 7.3f[ms]", double(frameTime)*toMs);	

				const bgfx::Stats* stats = bgfx::getStats();
				bgfx::dbgTextPrintf(0, 5, 0x0f, "Backbuffer %dW x %dH in pixels, debug text %dW x %dH in characters.",
						stats->width, stats->height, stats->textWidth, stats->textHeight);

				// Get the current camera frame and show on the GUIs windows
				cv::Mat cameraFrame;
				if (hasState(SHOW_CAMERA) && m_videoCapture.isOpened() && m_videoCapture.read(cameraFrame)) {
					auto imageFrameType = cameraFrame.type();

					bgfx::dbgTextPrintf(0, 6, 0x0f, "Video Capture %dx%d @%d fps",
						m_cameraInfo.frame_size.width,
						m_cameraInfo.frame_size.height,
						m_cameraInfo.fps);

					bgfx::dbgTextPrintf(0, 7, 0x0f, "Camera Frame %dx%d (type: %s)",
						cameraFrame.cols, cameraFrame.rows,
						cvTypeToString(imageFrameType).c_str());

					cv::Mat colorSpaceFrame;
					cv::Mat frameChannels[3];

					std::string colorSpaceString = "RGB";
					int32_t colorSpaceCode = cv::COLOR_BGR2RGB;
					int32_t	bgrRevertCode = cv::COLOR_RGB2BGR;

					{
						// Pick the requested color space
						if (hasState(COLOR_SPACE_HSV)) {
							colorSpaceCode = cv::COLOR_BGR2HSV;
							bgrRevertCode = cv::COLOR_HSV2RGB;
							colorSpaceString = "HSV";
						}
						else if (hasState(COLOR_SPACE_YCrCb)) {
							colorSpaceCode = cv::COLOR_BGR2YCrCb;
							bgrRevertCode = cv::COLOR_YCrCb2RGB;
							colorSpaceString = "YCrCb";
						}
						else if (hasState(COLOR_SPACE_Lab)) {
							colorSpaceCode = cv::COLOR_BGR2Lab;
							bgrRevertCode = cv::COLOR_Lab2RGB;
							colorSpaceString = "Lab";
						}

						bgfx::dbgTextPrintf(0, 8, 0x0f, "Channels Color Space: %s",
							colorSpaceString.c_str());
						
						// Make sure we are in the right format and convert to UMat
						cv::UMat bgr, colorSpaceImage;
						cameraFrame.convertTo(bgr, CV_8UC3);

						// Convert camera input to the requested color space
						cv::cvtColor(bgr, colorSpaceImage, colorSpaceCode);

						// Separate the color space channels
						std::vector<cv::UMat> channels;
						cv::split(colorSpaceImage, channels);

						// Convert bgr to rgba and back to Mat
						// that is image data can be transferred to GPU
						cv::UMat rgba;
						cv::cvtColor(bgr, rgba, cv::COLOR_BGR2RGBA);
						cameraFrame = rgba.getMat(cv::ACCESS_READ).clone();

						for (auto i = 0; i < channels.size(); ++i) {
							// Convert single channel image into RGBA.
							// This is a required step because ImGUI is not capable
							// of showing only one channel as grayscale image, nor has
							// the ability to show an image with a custom shader.
							cv::UMat grayRGBA;
							cv::cvtColor(channels[i], grayRGBA, cv::COLOR_GRAY2BGRA);

							// Convert to Mat to access data from the CPU
							frameChannels[i] = grayRGBA.getMat(cv::ACCESS_READ).clone();
						}

						// Merge channels into a single image
						//cv::Mat alphaOne = cv::Mat::ones(cameraFrame.rows, cameraFrame.cols, CV_8UC1);
						cv::Mat grayChannels[] = {
							channels[0].getMat(cv::ACCESS_READ).clone(),
							channels[1].getMat(cv::ACCESS_READ).clone(),
							channels[2].getMat(cv::ACCESS_READ).clone()
						};
						
						cv::merge(grayChannels, 3, colorSpaceFrame);
					}

					// Show video
					{
						// Draw UI
						imguiBeginFrame(m_mouseState.m_mx
							, m_mouseState.m_my
							, (m_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
							| (m_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
							| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
							, m_mouseState.m_mz
							, uint16_t(m_width)
							, uint16_t(m_height)
						);
						
						// Store current GUI cursor position to compute where the
						// mouse pointer is, w.r.t. the displayed camera image.
						ImVec2 cursorPos = ImGui::GetCursorPos();
						bool showVideoWindow = hasState(SHOW_CAMERA);
						
						if (ImGui::Begin("Camera", &showVideoWindow,
							ImGuiWindowFlags_AlwaysAutoResize
							| ImGuiWindowFlags_NoScrollbar
							| ImGuiWindowFlags_NoResize)) {
							
							// Displayed image starts at current cursor
							// screen position + window's content padding.
							ImVec2 windowPos = ImGui::GetWindowPos();
							ImVec2 windowPad = ImGui::GetStyle().WindowPadding;
							
							cv::Point2i imageScreenStart(
								int32_t(windowPos.x + cursorPos.x),
								int32_t(windowPos.y + cursorPos.y + windowPad.y)
							);
							
							cv::Point2i currentMousePos(
								m_mouseState.m_mx, m_mouseState.m_my
							);
							
							// Mouse position in displayed frame' space
							cv::Point2i mouseAtPixel = currentMousePos - imageScreenStart;
							cv::Rect imageROI = cv::Rect(0, 0, cameraFrame.cols, cameraFrame.rows);
							
							if (imageROI.contains(mouseAtPixel)) {
								// Color at pixel
								cv::Vec4b pixelColor = cameraFrame.at<cv::Vec4b>(
									mouseAtPixel.y, mouseAtPixel.x);
							
								bgfx::dbgTextPrintf(0, 9, 0x0f, "Pixel at (%d,%d) RGB=[%d %d %d]",
									mouseAtPixel.x, mouseAtPixel.y,
									pixelColor[0], pixelColor[1], pixelColor[2]
								);

								int32_t tolerance = 40 + m_mouseState.m_mz;
								bgfx::dbgTextPrintf(0, 10, 0x0f, "Picking tolerance: %d", tolerance);
							
								// If mouse right button is pressed, the color
								// of this pixel is the one we want to filter.
								if (m_mouseState.m_buttons[entry::MouseButton::Right]) {
									
									m_selectedColor = cvVec4bToImVec4f(pixelColor);
									{
										// Create a mask, showing only those pixels of similar color
										// as long as the user keep the mouse button presses.
										cv::Vec4b colorSpacePixel = colorSpaceFrame.at<cv::Vec4b>(
											mouseAtPixel.y, mouseAtPixel.x);
										
										cv::Vec3b lowerColor(
											cv::saturate_cast<uchar>(colorSpacePixel[0] - tolerance),
											cv::saturate_cast<uchar>(colorSpacePixel[1] - tolerance),
											cv::saturate_cast<uchar>(colorSpacePixel[2] - tolerance)
										);
										
										cv::Vec3b uppderColor(
											cv::saturate_cast<uchar>(colorSpacePixel[0] + tolerance),
											cv::saturate_cast<uchar>(colorSpacePixel[1] + tolerance),
											cv::saturate_cast<uchar>(colorSpacePixel[2] + tolerance)
										);

										// To diplay the color correctly we need to convet
										// back to RGB from the picked color space pixel.
										{
											// Create a matrix image of one pixel only.
											cv::Mat3b lowerImage(lowerColor);
											cv::Mat3b upperImage(uppderColor);

											// Convert these 1x1 matrices to RGB space,
											// but we are already operating in RGB.
											if (colorSpaceCode != cv::COLOR_BGR2RGB) {
												cv::cvtColor(lowerImage, lowerImage, bgrRevertCode);
												cv::cvtColor(upperImage, upperImage, bgrRevertCode);
											}										

											// Read back the pixel in RGB
											m_minColor = cvVec3bToImVec4f(lowerImage(0, 0));
											m_maxColor = cvVec3bToImVec4f(upperImage(0, 0));

											// Extract the mask in color space
											cv::Mat maskImage, resultImage;
											cv::inRange(colorSpaceFrame, lowerColor, uppderColor, maskImage);
											
											// Apply the mask to the original camera frame in RGBA
											cv::bitwise_and(cameraFrame, cameraFrame, resultImage, maskImage);
											//cv::cvtColor(resultImage, cameraFrame, cv::COLOR_BGR2RGBA);
											cameraFrame = resultImage;
										}
									}
								}
							}
							
							// Upload image data to textures
							updateImageToTexture(cameraFrame, m_texRGBA);
							updateImageToTexture(frameChannels[0], m_texChannels[0]);
							updateImageToTexture(frameChannels[1], m_texChannels[1]);
							updateImageToTexture(frameChannels[2], m_texChannels[2]);

							// Displayed camera frame' size
							auto frameSize = ImVec2((float)cameraFrame.cols, (float)cameraFrame.rows);
							
							// Show the main frame
							ImGui::Image((ImTextureID)(uintptr_t)m_texRGBA.idx, frameSize);
							
							// Color picker
							ImGui::ColorEdit3("Picked Color", &m_selectedColor.x,
								ImGuiColorEditFlags_NoSliders
								| ImGuiColorEditFlags_NoPicker
								| ImGuiColorEditFlags_NoOptions);
							
							ImGui::SameLine();
							ImGui::ColorEdit3("Lower Bound", &m_minColor.x,
								ImGuiColorEditFlags_NoSliders
								| ImGuiColorEditFlags_NoPicker
								| ImGuiColorEditFlags_NoOptions);

							ImGui::SameLine();
							ImGui::ColorEdit3("Upper Bound", &m_maxColor.x,
								ImGuiColorEditFlags_NoSliders
								| ImGuiColorEditFlags_NoPicker
								| ImGuiColorEditFlags_NoOptions);

							ImGui::BeginGroup();
							{
								auto frameChannelSize = ImVec2(
									frameSize.x * .332f, frameSize.y * .332f);
										
								// Show frame's channels
								for (const auto& texChannel : m_texChannels) {										
									ImGui::Image(texChannel, frameChannelSize);
									ImGui::SameLine();
								}
								ImGui::EndGroup();
							}
						}

						// Camera window
						ImGui::End();
									
						if(!showVideoWindow) {
							removeState(SHOW_CAMERA);
						}

						imguiEndFrame();
					}
				}		

				// Advance to next frame. Rendering thread will be
				//  kicked to process submitted rendering primitives.
				bgfx::frame();
				return true;
			}
		// } catch (cv::Exception& e) {
        // 	std::cerr << e.what() << std::endl;
        // 	std::exit(EXIT_FAILURE);
    	// }

		return false;
	}

	enum State {
		NONE				= 0,
		EXIT_REQUEST		= (1<<0),
		SHOW_CAMERA			= (1<<1),
		COLOR_SPACE_Lab		= (1<<2),
		COLOR_SPACE_YCrCb	= (1<<3),
		COLOR_SPACE_HSV		= (1<<4),
		COLOR_SPACE_RGB		= (1<<5),
		COLOR_SPACE_ALL		=
			  COLOR_SPACE_Lab		
			| COLOR_SPACE_YCrCb
			| COLOR_SPACE_HSV		
			| COLOR_SPACE_RGB
	};

	void addState(State s) {
		m_states |= (uint32_t)s;
	}

	void removeState(State s) {
		m_states &= ~(uint32_t)s;
	}

	bool hasState(State s) {
		return (m_states & (uint32_t)s);
	}

	void toggleState(State s) {
		if (hasState(s)) {
			removeState(s);
		}
		else {
			addState(s);
		}
	}

	void quit() {
		addState(EXIT_REQUEST);
	}

    entry::MouseState 		m_mouseState;
	bgfx::TextureHandle		m_texRGBA;
	bgfx::TextureHandle		m_texChannels[3];
	std::string				m_progName;

	cv::VideoCapture		m_videoCapture;
	CameraInfo				m_cameraInfo;

	ImVec4					m_selectedColor;
	ImVec4					m_minColor;
	ImVec4					m_maxColor;

	uint32_t	m_states;
	uint32_t    m_width;
	uint32_t    m_height;
	uint32_t    m_debug;
	uint32_t    m_reset;
    int64_t     m_timeOffset;
};

ENTRY_IMPLEMENT_MAIN(ShowGUI);