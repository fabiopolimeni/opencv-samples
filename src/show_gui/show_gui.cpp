#include <bx/bx.h>
#include <bx/uint32_t.h>
#include <bx/string.h>
#include <bgfx/bgfx.h>
#include <bx/crtimpl.h>
#include <imgui/imgui.h>

#include "entry/entry.h"
#include "entry/input.h"
#include "common.h"
#include "bgfx_utils.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/videoio.hpp>

#include <iostream>
#include <string>
#include <vector>

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
}

class ShowGUI : public entry::AppI {

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
		style.ChildWindowRounding = 0;	        // Radius of child window corners rounding. Set to 0.0f to have rectangular windows
		style.FramePadding = ImVec2(2,2); 		// Padding within a framed rectangle (used by most widgets)
    	style.ItemSpacing = ImVec2(1,1);  		// Horizontal and vertical spacing between widgets/lines
    	style.ItemInnerSpacing = ImVec2(3,3);   // Horizontal and vertical spacing between within elements of a composed widget (e.g. a slider and its label)
    	//style.IndentSpacing;              // Horizontal indentation when e.g. entering a tree node. Generally == (FontSize + FramePadding.x*2).
    
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
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.22f, 0.60f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.65f, 0.22f, 0.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.93f, 0.52f, 0.02f, 0.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.92f, 0.82f, 0.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.22f, 0.60f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.20f, 0.20f, 0.22f);
	}

	void initGUI(int _argc, char** _argv) {
		m_showVideoWindow = true;

		// Initialise GUI
		imguiCreate();
		setupGUIStyle();
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

	void init(int _argc, char** _argv) override	{
		initOpenCV(_argc, _argv);
		initBgfx(_argc, _argv);
		initGUI(_argc, _argv);

		// Create the textures to display
		m_texRGB = bgfx::createTexture2D(
			m_cameraInfo.frame_size.width,					// width
			m_cameraInfo.frame_size.height,					// height
			false, 											// no mip-maps
			1,												// number of layers
			bgfx::TextureFormat::Enum::RGB8,				// format
			BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP,	// flags
			nullptr											// mutable
		);

		m_timeOffset = bx::getHPCounter();
	}

	virtual int shutdown() override	{
		imguiDestroy();
		bgfx::shutdown();
		return EXIT_SUCCESS;
	}

	virtual bool update() override	{
		if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState) ) {
            int64_t now = bx::getHPCounter();
			static int64_t last = now;
			const int64_t frameTime = now - last;
			last = now;
			const double freq = double(bx::getHPFrequency() );
			const double toMs = 1000.0/freq;

			float time = (float)( (now-m_timeOffset)/double(bx::getHPFrequency() ) );

			// Set view 0 default viewport.
			bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height) );

			// This dummy draw call is here to make sure that view 0 is cleared
			// if no other draw calls are submitted to view 0.
			bgfx::touch(0);

			// Use debug font to print information about this example.
			bgfx::dbgTextClear();

			bgfx::dbgTextPrintf(0, 1, 0x4f, "Program: %s", m_progName.c_str());
			bgfx::dbgTextPrintf(0, 2, 0x6f, "Description: Rendering captured camera frames into different color spaces.");
			bgfx::dbgTextPrintf(0, 3, 0x8f, "Frame time: % 7.3f[ms]", double(frameTime)*toMs);	

			const bgfx::Stats* stats = bgfx::getStats();
			bgfx::dbgTextPrintf(0, 5, 0x0f, "Backbuffer %dW x %dH in pixels, debug text %dW x %dH in characters.",
					stats->width, stats->height, stats->textWidth, stats->textHeight);

			// Get the current camera frame and show it as a debug image
			cv::Mat cameraFrame;
			if (m_showVideoWindow && m_videoCapture.isOpened() && m_videoCapture.read(cameraFrame)) {
				auto imageFrameType = cameraFrame.type();

				bgfx::dbgTextPrintf(0, 6, 0x0f, "Camera frame %dx%d @%d fps (type: %s)",
					m_cameraInfo.frame_size.width, m_cameraInfo.frame_size.height,
					m_cameraInfo.fps, cvTypeToString(imageFrameType).c_str());

				{
					// Make sure we are in the right format and convert to UMat
					cv::UMat frameImage, rgbImage;
					cameraFrame.convertTo(frameImage, CV_8UC3);
					cv::cvtColor(frameImage, rgbImage, cv::COLOR_BGR2RGB);

					// TODO: Operate on frameImage (UMat)

					// Convert back to Mat
					cameraFrame = rgbImage.getMat(cv::ACCESS_READ).clone();
				}

				size_t imageSize = cameraFrame.total() * cameraFrame.elemSize();
				size_t imagePitch = cameraFrame.cols * cameraFrame.elemSize();

				m_imagePixels.resize(imageSize);

				// If the grabbed frame is continuous in memory,
				// then copying pixels is straight forward.
				if (cameraFrame.isContinuous()) {
					bx::memCopy(m_imagePixels.data(), cameraFrame.data, imageSize);
				} else {
					auto *p = m_imagePixels.data();
					for(int32_t i  = 0; i < cameraFrame.rows; i++){
						memcpy(p, cameraFrame.ptr(i), cameraFrame.cols*imagePitch);
						p += imagePitch;
					}
				}
				
				// Using makeRef to pass texture memory without copying.
				const bgfx::Memory* imageMem = bgfx::makeRef(m_imagePixels.data(), imageSize);

				// Copy camera frame into the texture
				bgfx::updateTexture2D(
					m_texRGB,			// texture handle
					0, 0, 				// mip, layer
					0, 0,				// start x, y
					cameraFrame.cols,	// width
					cameraFrame.rows,	// height
					imageMem,			// memory
					imagePitch			// pitch
				);

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

					ImGui::Begin("Camera", &m_showVideoWindow,
						ImVec2(0.0f, 0.0f),
						ImGuiWindowFlags_AlwaysAutoResize); {

							ImGui::Image((ImTextureID)(uintptr_t)m_texRGB.idx,
								ImVec2((float)cameraFrame.cols, (float)cameraFrame.rows));
							
						ImGui::End();
					}

					imguiEndFrame();
				}
			}		

			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
			bgfx::frame();


			// Quit
            if (inputGetKeyState(entry::Key::Esc)) {
                return false;
            }

			uint8_t keyModifiers = inputGetModifiersState();

			// Toggle windows
			if (keyModifiers & entry::Modifier::LeftShift) {
				if (inputGetKeyState(entry::Key::KeyD)) {
					m_showVideoWindow = !m_showVideoWindow;
				}
			}

			return true;
		}

		return false;
	}

    entry::MouseState 		m_mouseState;
	bgfx::TextureHandle		m_texRGB;
	std::vector<uint8_t>	m_imagePixels;
	std::string				m_progName;

	cv::VideoCapture		m_videoCapture;
	CameraInfo				m_cameraInfo;

	uint32_t    m_width;
	uint32_t    m_height;
	uint32_t    m_debug;
	uint32_t    m_reset;
    int64_t     m_timeOffset;

	bool		m_showVideoWindow;
};

ENTRY_IMPLEMENT_MAIN(ShowGUI);