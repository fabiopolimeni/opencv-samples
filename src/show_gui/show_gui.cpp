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
#include <opencv2/core/ocl.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <algorithm>

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

	template<typename T>
	T clamp(const T& v, const T& lo, const T& hi) {
		return (v < lo) ? lo : (hi < v) ? hi : v;
	}

	struct CameraInfo {
		int32_t     id;
		cv::Size    frameSize;
		int32_t     fps;
	};

	struct OCLDevice {
		int32_t		id;
		std::string	name;
		std::string	version;
		bool		available;
		bool		imageSupport;
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
			<< " (" << camera.frameSize.width
			<< "x" << camera.frameSize.height
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

	void printOpenCLDevice(const OCLDevice& device) {
		std::cout << "OpenCL Device: " << device.name << std::endl;
		std::cout << " - id:            " << device.id << std::endl;
		std::cout << " - available:     " << std::boolalpha << device.available << std::endl;
		std::cout << " - imageSupport:  " << std::boolalpha << device.imageSupport << std::endl;
		std::cout << " - version:       " << device.version << std::endl;
		std::cout << std::endl;
	}

	std::vector<OCLDevice> enumerateOpenCLDevices() {
		std::vector<OCLDevice> devices;

		cv::ocl::setUseOpenCL(true);
		if (!cv::ocl::haveOpenCL())
		{
			return devices;
		}

		cv::ocl::Context context;
		if (!context.create(cv::ocl::Device::TYPE_GPU))
		{
			std::cout << "Failed creating the context..." << std::endl;
			return devices;
		}

		for (int32_t i = 0; i < context.ndevices(); ++i)
		{
			cv::ocl::Device device = context.device(i);
			OCLDevice clDevice = {
				i,
				device.name(),
				device.OpenCLVersion(),
				device.available(),
				device.imageSupport()
			};

			devices.push_back(clDevice);
		}

		return devices;
	}

	ImVec4 cvVec4bToImVec4f(const cv::Vec4b& color) {
		ImU32 u32Color = (color[0]) | (color[1] << 8) | (color[2] << 16) | (color[3] << 24);
		return ImGui::ColorConvertU32ToFloat4(u32Color);
	}

	ImVec4 cvVec3bToImVec4f(const cv::Vec3b& color, uint8_t alpha = 0xff) {
		ImU32 u32Color = (color[0]) | (color[1] << 8) | (color[2] << 16) | (alpha << 24);
		return ImGui::ColorConvertU32ToFloat4(u32Color);
	}
}

class FrameOptions {

public:

	bool printUsage;
	bool cvInfo;
	bool enumCameras;
	bool enumOCLDevices;
	bool useMultiThreading;

	int32_t clDevice;
	int32_t numOfFrames;
	int32_t frameOffset;

	int32_t cameraId;
	int32_t frameWidth;
	int32_t frameHeight;
	int32_t requestedFPS;

	// Parse command line arguments and set relevant properties.
	// Return false if any argument is invalid, true otherwise.
	bool init(int _argc, char** _argv) {
		std::string options =
			"{help usage h| |Program usage}"
			"{opencv-info v| |OpenCV build info}"
			"{enumerate-cameras c| |Enumerates available cameras}"
			"{enumerate-ocl-devices l| |Enumerates OpenCL devices}"
			"{opencl-device d|-1|Whether to use OpenCL device}"
			"{frames-buffer f|2|Number of frames to hold in the buffer}"
			"{frame-offset o|-1|Offset into the frame's buffer}"
			"{multi-threaded m| |Enable multi-threading}"
			"{@camera|0|Camera to show}"
			"{@width|640|Desired frame width}"
			"{@height|360|Desired frame height}"
			"{@fps|60|Desired capture frame-rate}";

		m_parser = new cv::CommandLineParser(_argc, _argv, options);
		m_parser->about("Shows the view of the chosen camera");

		if (!m_parser->check()) {
			m_parser->printErrors();
			return false;
		}

		// Flags
		printUsage = m_parser->has("usage");
		cvInfo = m_parser->has("opencv-info");
		enumCameras = m_parser->has("enumerate-cameras");
		enumOCLDevices = m_parser->has("enumerate-ocl-devices");
		useMultiThreading = m_parser->has("multi-threaded");

		// OpenCL device to use. -1 means no OpenCL process
		clDevice = m_parser->get<int32_t>("opencl-device");

		// Get the maximum number of frames we want to store into the frame's buffer
		numOfFrames = clamp(m_parser->get<int32_t>("frames-buffer"), 1, 64);
		frameOffset = clamp(m_parser->get<int32_t>("frame-offset"), -(numOfFrames -1), 0);

		// Camera's frame properties
		cameraId = m_parser->get<int32_t>("@camera");
		frameWidth = m_parser->get<int32_t>("@width");
		frameHeight = m_parser->get<int32_t>("@height");
		requestedFPS = m_parser->get<int32_t>("@fps");

		return true;
	}

	void printUsageMessage() {
		m_parser->printMessage();
	}

	FrameOptions() : m_parser(nullptr) {

	}

	virtual ~FrameOptions() {
		delete m_parser;
	}

private:

	cv::CommandLineParser* m_parser;
};

class FrameProcessor {

public:

	bool init(int32_t _gpuDeviceId = -1) {
		return false;
	}

};

class FrameProvider {

public:

	bool init(int32_t _cameraId, int32_t _frameWidth, int32_t _frameHeight, int32_t _fps,
		int32_t _frames, int32_t _offset, bool _isMultiThreaded) {
		// Get the maximum number of frames we want to store into the frame's buffer
		m_numOfFrames = clamp(_frames, 1, 64);
		m_cameraFrames = new Frame[m_numOfFrames];

		m_frameOffset = clamp(_offset, -(m_numOfFrames -1), 0);

		// Create a camera info for the given command line's arguments
		if (!m_videoCapture.open(_cameraId)) {
			std::cerr << "Requested camera " << _cameraId << " is not available!" << std::endl;
			return false;
		}

		m_videoCapture.set(CV_CAP_PROP_FRAME_WIDTH, (double)_frameWidth);
		m_videoCapture.set(CV_CAP_PROP_FRAME_HEIGHT, (double)_frameHeight);
		m_videoCapture.set(CV_CAP_PROP_FPS, (double)_fps);
		
		m_cameraInfo = {
			_cameraId,
			cv::Size(
				(int32_t)m_videoCapture.get(CV_CAP_PROP_FRAME_WIDTH),
				(int32_t)m_videoCapture.get(CV_CAP_PROP_FRAME_HEIGHT)),
			(int32_t)m_videoCapture.get(CV_CAP_PROP_FPS)
		};

		m_process.test_and_set(std::memory_order::memory_order_acq_rel);
		m_capture.store(false, std::memory_order::memory_order_relaxed);
		m_indexCounter.store(0, std::memory_order::memory_order_release);

		// If multi-threading is enabled, create a
		// thread and execute here the tick funciton.
		m_isMultiThreaded = _isMultiThreaded;
		if (m_isMultiThreaded) {
			m_captureThread = std::thread([this]{
				while(m_process.test_and_set(std::memory_order::memory_order_acq_rel)) {
					this->tick();
				}
			});
		}

		return true;
	}

	// Retuns whether a new image has been added into the buffer.
	bool tick() {
		if (m_capture.load(std::memory_order::memory_order_relaxed)) {
			
			cv::Mat cameraFrame;
			if (m_videoCapture.isOpened() && m_videoCapture.read(cameraFrame)) {
				// Write into the back buffer, which in our case
				// technically is the following available frame in the buffer
				auto backIndex = computeBufferIndex(m_indexCounter + 1);
				auto& frame = m_cameraFrames[backIndex];
				frame.write(cameraFrame);

				// The following operation will not necessarly make
				// variables visible and consistent to the rest of
				// the threads, e.g ARM architecture, if it wasn't
				// for the release memory order.
				// Being the index an atomic object it guarantees
				// that two threads, querying the buffer before the
				// next write is issued, will see the same result,
				// and therefore, they will process the same image.
				m_indexCounter.fetch_add(1, std::memory_order::memory_order_release);
				return true;
			}
		}

		return false;
	}

	void shutdown() {
		m_process.clear(std::memory_order::memory_order_release);
		if (m_captureThread.joinable()) {
			m_captureThread.join();
		}

		delete[] m_cameraFrames;
	}

	void capture(bool _onOff) {
		m_capture.store(_onOff, std::memory_order::memory_order_relaxed);
		if (!m_isMultiThreaded) {
			tick();
		}
	}

	// Return a copy of the current front-buffer camera's capture
	cv::Mat getCameraFrame(int32_t _offset = 1) const {
		// An offset greater than 0 indicates that we want to use
		// the value passed as argument to the command line.
		if (_offset > 0) {
			_offset = m_frameOffset;
		}

		auto steps = clamp(_offset, -(m_numOfFrames -1), 0);
		auto index = getBufferIndexByOffset(steps);
		return m_cameraFrames[index].read();
	}

	const CameraInfo& getCameraInfo() const {
		return m_cameraInfo;
	}

	bool isMultiThreaded() const {
		return m_isMultiThreaded;
	}

	int32_t getNumberOfFramesInBuffer() const {
		return m_numOfFrames;
	}

	FrameProvider() : m_cameraFrames(nullptr) {

	}

	virtual ~FrameProvider() {

	}

private:
	
	class Frame {
		cv::Mat				m_imageBGR;
		std::shared_mutex	m_rwMutex;

	public:
		cv::Mat read() {
			m_rwMutex.lock_shared();
			auto image = m_imageBGR.clone();
			m_rwMutex.unlock_shared();
			return image;
		}

		void write(const cv::Mat& _image) {
			m_rwMutex.lock();
			m_imageBGR = _image.clone();
			m_rwMutex.unlock();
		}
	};

	cv::VideoCapture		m_videoCapture;
	
	Frame*					m_cameraFrames;
	CameraInfo				m_cameraInfo;

	std::thread				m_captureThread;

	std::atomic_flag		m_process;
	std::atomic<bool>		m_capture;

	std::atomic<int32_t>	m_indexCounter;
	int32_t					m_numOfFrames;
	int32_t					m_frameOffset;
	bool					m_isMultiThreaded;

	int32_t computeBufferIndex(int32_t _value) const {
		return _value % m_numOfFrames;
	}

	int32_t getBufferIndexByOffset(int32_t _offset) const {
		auto counter = m_indexCounter.load(std::memory_order::memory_order_acquire);
		auto index = counter + _offset;
		if (index >= 0) {
			return computeBufferIndex(index);
		}
		else {
			return m_numOfFrames + index;
		}
	}
};

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
		setState(NONE);

		if (!m_frameOptions.init(_argc, _argv)) {
			addState(EXIT_REQUEST);
		}

		if (!m_frameProvider.init(
			m_frameOptions.cameraId,
			m_frameOptions.frameWidth,
			m_frameOptions.frameHeight,
			m_frameOptions.requestedFPS,
			m_frameOptions.numOfFrames,
			m_frameOptions.frameOffset,
			m_frameOptions.useMultiThreading
		)) {
			addState(EXIT_REQUEST);
		}

		initBgfx(_argc, _argv);
		initGUI(_argc, _argv);

		auto cameraInfo = m_frameProvider.getCameraInfo();

		// Create the texture to hold camera input image
		m_texRGBA = bgfx::createTexture2D(
			cameraInfo.frameSize.width,						// width
			cameraInfo.frameSize.height,					// height
			false, 											// no mip-maps
			1,												// number of layers
			bgfx::TextureFormat::Enum::RGBA8,				// format
			BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP,	// flags
			nullptr											// mutable
		);

		// Create textures for displaing the channels separately
		for (auto& tex : m_texChannels) {
			tex = bgfx::createTexture2D(
				cameraInfo.frameSize.width,					// width
				cameraInfo.frameSize.height,					// height
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

		// Parse options
		{
			if (m_frameOptions.printUsage) {
				m_frameOptions.printUsageMessage();
				addState(EXIT_REQUEST);
			}

			if  (m_frameOptions.cvInfo) {
				std::cout << cv::getBuildInformation() << std::endl;
				addState(EXIT_REQUEST);
			}

			if (m_frameOptions.enumCameras) {
				auto cameras = enumerateCameras();

				if (!cameras.empty()) {
					std::cout << "-- Available cameras --" << std::endl;
					for (auto camera : cameras) {
						printCameraInfo(camera);
					}
				}
				else {
					std::cout << "!! No camera available !!" << std::endl;
				}

				std::cout << std::flush;
				addState(EXIT_REQUEST);
			}

			if (m_frameOptions.enumOCLDevices) {
				auto devices = enumerateOpenCLDevices();

				if (!devices.empty()) {
					std::cout << "-- Available OpenCL devices --" << std::endl;
					for (auto device : devices) {
						printOpenCLDevice(device);
					}
				}
				else {
					std::cout << "!! No OpenCL device available !!" << std::endl;
				}

				std::cout << std::flush;
				addState(EXIT_REQUEST);
			}
		}

		// Set initial states
		addState(SHOW_CAMERA);
		addState(COLOR_SPACE_RGB);

		bx::memSet(&m_selectedColor, 0x0, sizeof(m_selectedColor));
		m_timeOffset = bx::getHPCounter();
	}

	virtual int shutdown() override	{
		m_frameProvider.shutdown();
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

			// This dummy draw call is here to make sure that view 0 is
			// cleared if no other draw calls are submitted to view 0.
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
			bool showGUI = hasState(SHOW_CAMERA);
			m_frameProvider.capture(showGUI);
			if (showGUI) {
				cv::Mat cameraFrame = m_frameProvider.getCameraFrame();
				if (!cameraFrame.empty()) {
				
					auto imageFrameType = cameraFrame.type();
					auto cameraInfo = m_frameProvider.getCameraInfo();
					
					bgfx::dbgTextPrintf(0, 6, 0x0f, "Video Capture %dx%d @%d fps (%s)",
						cameraInfo.frameSize.width, cameraInfo.frameSize.height, cameraInfo.fps,
						m_frameProvider.isMultiThreaded() ? "multi-threaded" : "single-thread");
					
					bgfx::dbgTextPrintf(0, 7, 0x0f, "Camera Frame %dx%d (type: %s frames: %d)",
						cameraFrame.cols, cameraFrame.rows,
						cvTypeToString(imageFrameType).c_str(),
						m_frameProvider.getNumberOfFramesInBuffer());
					
					cv::Mat3b colorSpaceFrame;
					cv::Mat frameChannels[3];
					
					int32_t colorSpaceCode = cv::COLOR_BGR2RGB;
					int32_t	rgbToColorSpace = 0;
					std::string colorSpaceString = "RGB";
					{
						// Pick the requested color space
						if (hasState(COLOR_SPACE_HSV)) {
							colorSpaceCode = cv::COLOR_BGR2HSV;
							rgbToColorSpace = cv::COLOR_HSV2RGB;
							colorSpaceString = "HSV";
						}
						else if (hasState(COLOR_SPACE_YCrCb)) {
							colorSpaceCode = cv::COLOR_BGR2YCrCb;
							rgbToColorSpace = cv::COLOR_YCrCb2RGB;
							colorSpaceString = "YCrCb";
						}
						else if (hasState(COLOR_SPACE_Lab)) {
							colorSpaceCode = cv::COLOR_BGR2Lab;
							rgbToColorSpace = cv::COLOR_Lab2RGB;
							colorSpaceString = "Lab";
						}
						
						bgfx::dbgTextPrintf(0, 8, 0x0f, "Channels Color Space: %s",
							colorSpaceString.c_str());
						
						// Make sure we are in the right format and convert to Mat
						cv::Mat bgr, colorSpaceImage;
						cameraFrame.convertTo(bgr, CV_8UC3);
						
						// Convert camera input to the requested color space
						cv::cvtColor(bgr, colorSpaceImage, colorSpaceCode);
						
						// Separate the color space channels
						std::vector<cv::Mat> channels;
						cv::split(colorSpaceImage, channels);
						
						// Convert bgr to rgba and back to Mat
						// that is image data can be transferred to GPU
						cv::Mat rgba;
						cv::cvtColor(bgr, rgba, cv::COLOR_BGR2RGBA);
						
						cameraFrame = rgba;//.getMat(cv::ACCESS_READ).clone();
						for (auto i = 0; i < channels.size(); ++i) {
							// Convert single channel image into RGBA.
							// This is a required step because ImGUI is not capable
							// of showing only one channel as grayscale image, nor has
							// the ability to show an image with a custom shader.
							cv::Mat grayRGBA;
							cv::cvtColor(channels[i], grayRGBA, cv::COLOR_GRAY2BGRA);
							
							// Convert to Mat to access data from the CPU
							frameChannels[i] = grayRGBA;//.getMat(cv::ACCESS_READ).clone();
						}
						
						// Merge channels into a single image
						//cv::Mat alphaOne = cv::Mat::ones(cameraFrame.rows, cameraFrame.cols, CV_8UC1);
						cv::Mat grayChannels[] = {
							channels[0],//.getMat(cv::ACCESS_READ).clone(),
							channels[1],//.getMat(cv::ACCESS_READ).clone(),
							channels[2],//.getMat(cv::ACCESS_READ).clone()
						};
						
						cv::merge(grayChannels, 3, colorSpaceFrame);
					}
					
					// Show camera capture on the GUI
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
								// RGB pixel color at requested image coordinates
								cv::Vec4b pixelColor = cameraFrame.at<cv::Vec4b>(
									mouseAtPixel.y, mouseAtPixel.x);
								cv::Vec3b pixelSpace = colorSpaceFrame.at<cv::Vec3b>(
									mouseAtPixel.y, mouseAtPixel.x);
							
								bgfx::dbgTextPrintf(0, 9, 0x0f, "Pixel at (%d,%d) RGB=[%d %d %d] %s=[%d %d %d]",
									mouseAtPixel.x, mouseAtPixel.y,
									pixelColor[0], pixelColor[1], pixelColor[2],
									colorSpaceString.c_str(),
									pixelSpace[0], pixelSpace[1], pixelSpace[2]
								);
								
								int32_t tolerance = 40 + m_mouseState.m_mz;
								bgfx::dbgTextPrintf(0, 10, 0x0f, "Picking tolerance: %d", tolerance);
							
								// If mouse right button is pressed, the color
								// of this pixel is the one we want to filter.
								if (m_mouseState.m_buttons[entry::MouseButton::Right]) {
									m_selectedColor = cvVec4bToImVec4f(pixelColor);
									{										
										cv::Vec3b lowerColor(
											cv::saturate_cast<uchar>(pixelSpace[0] - tolerance),
											cv::saturate_cast<uchar>(pixelSpace[1] - tolerance),
											cv::saturate_cast<uchar>(pixelSpace[2] - tolerance)
										);
										
										cv::Vec3b upperColor(
											cv::saturate_cast<uchar>(pixelSpace[0] + tolerance),
											cv::saturate_cast<uchar>(pixelSpace[1] + tolerance),
											cv::saturate_cast<uchar>(pixelSpace[2] + tolerance)
										);
										
										// To diplay the color correctly we need to convet
										// back to RGB from the picked color space pixel.
										if (!hasState(COLOR_SPACE_RGB)) {
											// Create a matrix image of one pixel only.
											cv::Mat3b lowerImage(lowerColor);
											cv::Mat3b upperImage(upperColor);
											// Convert these 1x1 matrices to RGB space,
											// but we are already operating in RGB.
											cv::cvtColor(lowerImage, lowerImage, rgbToColorSpace);
											cv::cvtColor(upperImage, upperImage, rgbToColorSpace);
											// Read back the pixel in RGB for readibility purpose
											m_minColor = cvVec3bToImVec4f(lowerImage(0, 0));
											m_maxColor = cvVec3bToImVec4f(upperImage(0, 0));
										}
										else {
											// Read back the pixel in RGB for readibility purpose
											m_minColor = cvVec3bToImVec4f(lowerColor);
											m_maxColor = cvVec3bToImVec4f(upperColor);
										}
										
										// Extract the mask in requested color space
										cv::Mat maskImage, resultImage;
										cv::inRange(colorSpaceFrame, lowerColor, upperColor, maskImage);
										
										// Apply the mask to the original camera frame in RGBA
										cv::bitwise_and(cameraFrame, cameraFrame, resultImage, maskImage);
										//cv::cvtColor(maskImage, cameraFrame, cv::COLOR_GRAY2RGBA);
										cameraFrame = resultImage;										
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
			}		
			
			// Advance to next frame. Rendering thread will be
			// kicked to process submitted rendering primitives.
			bgfx::frame();
			return true;
		}
		
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

	void setState(State s) {
		m_states = (uint32_t)s;
	}

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

	FrameOptions			m_frameOptions;
	FrameProcessor			m_frameProcessor;
	FrameProvider			m_frameProvider;

    entry::MouseState 		m_mouseState;
	bgfx::TextureHandle		m_texRGBA;
	bgfx::TextureHandle		m_texChannels[3];
	std::string				m_progName;

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