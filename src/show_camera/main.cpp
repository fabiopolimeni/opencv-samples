#include <iostream>
#include <string>
#include <vector>

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/videoio.hpp>

template<class T>
T base_name(T const & path, T const & delims = "/\\")
{
  return path.substr(path.find_last_of(delims) + 1);
}
template<class T>
T remove_extension(T const & filename)
{
  typename T::size_type const p(filename.find_last_of('.'));
  return p > 0 && p != T::npos ? filename.substr(0, p) : filename;
}

void help(const char* path) {

    std::string progname = remove_extension(base_name<std::string>(path));

    std::cout << std::endl
        << "Shows the view of the chosen camera" << std::endl
        << "usage: " << progname << " [options]" << std::endl
        << "usage: " << progname << " <camera-id> <width> <height>" << std::endl
        << "\toptions:" << std::endl
        << "\t -e: enumerates the cameras in the system" << std::endl;
}

struct camera_info {
    int32_t       id;
    cv::Size    frame_size;
};

std::vector<camera_info> enum_available_cameras() {
    std::vector<camera_info> cameras;

    int32_t device_counts = 0;
    while (true) {
        cv::VideoCapture camera;
        if (!camera.open(device_counts)) {
            break;
        }

        cameras.push_back({
            device_counts,
            cv::Size(
                (int32_t)camera.get(cv::CAP_PROP_FRAME_WIDTH),
                (int32_t)camera.get(cv::CAP_PROP_FRAME_HEIGHT))
        });
        
        ++device_counts;
        camera.release();
    }

    return cameras;
}

int main(int32_t argc, char* argv[]) {

    try {
        std::string options =
            "{help h usage| |}"
            "{enum e| |Enumerate available cameras|}"
            "{@camera|0|Camera to show|}"
            "{@width|1280|Desired frame width|}"
            "{@height|720|desired frame height|}";

        cv::CommandLineParser parser(argc, argv, options);

        if (parser.has("help"))
        {
            help(argv[0]);
            return EXIT_SUCCESS;
        }

        // Users wants to know the cameras availables
        if (parser.has("e")) {
            auto cameras = enum_available_cameras();
            return EXIT_SUCCESS;
        }

        // Create a camera info for the given command line's arguments
        camera_info ci = {
            parser.get<int32_t>("@camera"),
            cv::Size(
                parser.get<int32_t>("@width"),
                parser.get<int32_t>("@height"))
        };

        cv::VideoCapture vc;
        if (!vc.open(ci.id)) {
            std::cerr << "Camera " << ci.id << " is not available!" << std::endl;
            help(argv[0]);
            return EXIT_FAILURE;
        }

        const std::string window_name("Camera Show");
        cv::namedWindow(window_name);

        cv::Mat frame;
        while (vc.grab() && vc.retrieve(frame)) {
            cv::imshow(window_name, frame);
            auto key = cv::waitKey(10);
            if (key == 27) // ESCAPE
                break;
        }

    } catch (cv::Exception& cv_exc) {
        std::cerr << cv_exc.msg << std::endl;
        help(argv[0]);
        std::exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}