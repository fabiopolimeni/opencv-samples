#include <iostream>
#include <string>

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utility.hpp>

void help()
{
    std::cout << std::endl <<
        "This sample show how to load and show a simple image"
        << std::endl;
}

int main(int32_t argc, char* argv[])
{
    std::string options =
        "{help h usage| |}"
        "{@image|data/images/lena.jpg|Image to show|}";

    cv::CommandLineParser parser(argc, argv, options);

    if (parser.has("help"))
    {
        help();
        return EXIT_SUCCESS;
    }

    std::string imagename = parser.get<std::string>("@image");
    cv::Mat img = cv::imread(imagename); // the newer cvLoadImage alternative, MATLAB-style function
    if(img.empty())
    {
        std::cerr << "Cannot load image " << imagename << std::endl;
        return EXIT_FAILURE;
    }

    cv::imshow("Image with grain", img);
    cv::waitKey(0);

    return EXIT_SUCCESS;
}
