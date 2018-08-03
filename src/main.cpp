
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/objdetect.hpp>

#include <boost/program_options.hpp>

#include <fstream>
#include <cstdlib>
#include <unistd.h>

#include <stdio.h>
#include <vector>
#include <iostream>

#include "pickerComms.hpp"
#include "pickerError.hpp"

namespace po = boost::program_options;

using namespace cv;
using namespace std;

using namespace terraclear;


//Find objects in a scene based on specific criteria
//Return contours for objects
vector<Rect> findObjectBoundingBoxes(Mat imgsrc)
{
    
    
    //ret vector
    vector<Rect> ret_vect;
    
    // hard coded color hue range for BLUE
    Scalar lowRange = Scalar(106,80,80);
    Scalar highRange = Scalar(116,220,220);
 
    Mat mat_filtered;
    
    //blur Image a bit first.
    cv::blur(imgsrc, mat_filtered, Size(20,20));

    /// Transform it to HSV color space
    cvtColor(mat_filtered, mat_filtered, COLOR_BGR2HSV);
    
    //find all objects in range
    cv::inRange(mat_filtered, lowRange, highRange, mat_filtered);
    
    //Vector for all contours.
    vector<vector<Point>> contours;
    findContours(mat_filtered, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
    
    //create bounding boxes from contours
    for (vector<vector<Point>>::iterator it = contours.begin(); it != contours.end(); it++)
    {
        vector<Point> contour = *it;
        ret_vect.push_back( boundingRect(Mat(contour)));
    }
    
    return ret_vect;
}

void mergeBoundingBoxes(vector<Rect> &object_boxes)
{
    //Group neighbouring rectangles
    vector<Rect> object_boxes_copy(object_boxes);
    object_boxes.insert(object_boxes.end(), object_boxes_copy.begin(), object_boxes_copy.end());
    groupRectangles(object_boxes, 1, 1);
 }

int main(int argc, const char * argv[])
{
    
    cout << "Starting Object Detection Scooper.\nPress any key to exit." << endl;
    
    //Config Options
    //Set Image Resolution..
    uint32_t cam_res_w = 1280;
    uint32_t cam_res_h = 960;

    // min square pixels off required object..
    uint32_t max_object_area = 500;

    //Threshold for percentage of object length to be on scoop
    double object_trigger_length = 0.60;

// Define supported command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("usb", po::value<string>(), "Set USB Serial Port device path. Default = /dev/ttyUSB0")
        ("nousb", po::value<bool>(), "Disable USB Check. Default = false")
        ("cam", po::value<uint16_t>(), "Set Video Camera Index. Default = 0")
        ("saveimg", po::value<bool>(), "Save Image From Camera on exit. Default = false")
        ("fullscr", po::value<bool>(), "Starts in full screen mode. Default = false");
        
    //variable map holding all supplied options..
    po::variables_map vm;
        
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
    }
    catch (std::exception& err)
    {
        cout << err.what() << endl << endl;
        cout << desc << endl;
        return -1;
    }
    
    po::notify(vm);    
    if (vm.count("help")) 
    {
        cout << desc << "\n";
        return 1;
    }
    
#ifdef __linux__
    string serialport = "/dev/ttyUSB0";
#else
    string serialport = "/dev/tty.usbserial-A5047JL0";
#endif

    //Get USB device path if specified as parameter
    if (vm.count("usb")) 
    {
       serialport = vm["usb"].as<string>();
    }  
    
    bool nousb = false;
    if (vm.count("nousb")) 
    {
       nousb = vm["nousb"].as<bool>();
    }        

    bool saveimg = false;
    if (vm.count("saveimg")) 
    {
       saveimg = vm["saveimg"].as<bool>();
    }        
    
    bool fullscr = false;
    if (vm.count("fullscr")) 
    {
       fullscr = vm["fullscr"].as<bool>();
    }        
    
    //setup serial port..
    pickerComms pcomms;
    try
    {
        //try to open serial port..
        pcomms.open(serialport, pickerBaud::BAUD_115200);
        
    } catch (pickerError err)
    {
        std::cout << "Error Opening Serial Port: " << serialport << endl;
        if (!nousb)
           return -1;
    }

    
    //Get Video Camera Index if specified as parameter
    uint16_t cam_id = 0;
    if (vm.count("cam")) 
    {
       cam_id = vm["cam"].as<uint16_t>();
    }    
    
    //Camera Matrix
    cv::Mat cam_mat;
    

    Scalar obj_box_color_detected = Scalar(0x00, 0xff, 0x00);//object bounding box color
    Scalar obj_detect_color_ready = Scalar(0x00, 0xff, 0xff);
    Scalar obj_detect_color_scoop1 = Scalar(0x00, 0x88, 0xff);
    Scalar obj_detect_color_scoop2 = Scalar(0x00, 0x00, 0xff);

    Scalar obj_detect_area_color1 = Scalar(0xff, 0x00, 0x00);//detection area bounding box color
    Scalar obj_detect_area_color2 = Scalar(0xff, 0x00, 0xff);//detection area bounding box color
    
      //detect Area 1 Ratios..
    uint32_t obj_detect_area1_x = 0.4063 * cam_res_w;
    uint32_t obj_detect_area1_y = 0.6354 * cam_res_h;
    uint32_t obj_detect_area1_w = 0.1563 * cam_res_w;
    uint32_t obj_detect_area1_h = 0.2 * cam_res_h;
  
    //detect Area 2 Ratios..
    uint32_t obj_detect_area2_x = 0.3906 * cam_res_w;
    uint32_t obj_detect_area2_y = 0.4167 * cam_res_h;
    uint32_t obj_detect_area2_w = 0.1875 * cam_res_w;
    uint32_t obj_detect_area2_h = 0.2083 * cam_res_h;

    Rect obj_detect_area1(Point(obj_detect_area1_x, obj_detect_area1_y), Point(obj_detect_area1_x + obj_detect_area1_w ,obj_detect_area1_y + obj_detect_area1_h)); //detection area 1
    Rect obj_detect_area2(Point(obj_detect_area2_x, obj_detect_area2_y), Point(obj_detect_area2_x + obj_detect_area2_w ,obj_detect_area2_y + obj_detect_area2_h)); //detection area 2

    //initialize Video Camera..
    VideoCapture cam_stream(cam_id);

    
     //check if video device has been initialised
    if (!cam_stream.isOpened())
    {
        std::cout << "Error Loading Camera: " << cam_id << endl;
        return -1;
    }
    
    //Set Camera Resolution
    cam_stream.set(3,cam_res_w);
    cam_stream.set(4,cam_res_h);
    
    //Go until key is pressed..
    while (true)
    {
        //TODO - move this to enum of pick state.
        bool pos_idle = true; //default position is idle..
        bool pos_pick = false;
        bool pos_scoop = false;
        

        //Read Camera Image
        cam_stream.read(cam_mat);
        if (!cam_mat.data)
            std::cout << "Camera could not load image...";
        else
        {

            //work with standard size image across platforms..
           // Size size(640,480);
           // resize(cam_mat,cam_mat,size);

            //Find all the Objects in captured frame..
            vector<Rect> object_boxes = findObjectBoundingBoxes(cam_mat);
            
            //Merge any overlapping boxes..
            mergeBoundingBoxes(object_boxes);
            
            
            //Top Left and Bottom Right coordinatres for detection area..
            Point obj_detect_area_tl1 = obj_detect_area1.tl();
            Point obj_detect_area_br1 = obj_detect_area1.br();
            
            Point obj_detect_area_tl2 = obj_detect_area2.tl();
            Point obj_detect_area_br2 = obj_detect_area2.br();

            //check objects location relative to areas of interrest..
            for (vector<Rect>::iterator it = object_boxes.begin(); it != object_boxes.end(); it++)
            {
                Rect obj_box = *it;
                //if box size is within required size
                if ((obj_box.height * obj_box.width) > max_object_area)
                {
                    //Top Left and Bottom Right coordinatres for box area..
                    Point obj_box_tl = obj_box.tl();
                    Point obj_box_br = obj_box.br();
                    
                    //is deteced object inside the areas of interrest
                    if ((obj_box & obj_detect_area1).area() > 0)
                    {
                        //Color to use, depending on where on scoop the object is at..
                        Scalar obj_scoop_color = obj_detect_color_scoop1;
                        
                        //check if at least X% of object lenth is on scoop..
                        uint32_t objlen_threshold =  (obj_box_br.y - obj_box_tl.y) * object_trigger_length;
                        uint32_t objlen_threshold_close = (obj_box_br.y - obj_box_tl.y) * object_trigger_length * 0.8;
                        
                        if ((obj_detect_area_tl1.y + objlen_threshold) < obj_box_br.y )
                        {               
                            obj_scoop_color = obj_detect_color_scoop2; //color2 on trigger vals..
                            pos_scoop = true;
                        }
                        else
                        {
                            //change to scoop color2 if its getting close to trigger values..
                            if ((obj_detect_area_tl1.y + objlen_threshold_close) < obj_box_br.y )
                                obj_scoop_color = obj_detect_color_scoop2;
                            
                            pos_pick = true;
                        }

                        rectangle(cam_mat, obj_box_tl, obj_box_br, obj_scoop_color, 2, 8, 0 );
                        
                   }
                    else if ((obj_box & obj_detect_area2).area() > 0)
                    {
                        rectangle(cam_mat, obj_box_tl, obj_box_br, obj_detect_color_ready, 2, 8, 0 );
                        pos_pick = true;
                    }
                    else
                    {
                        rectangle(cam_mat, obj_box_tl, obj_box_br, obj_box_color_detected, 2, 8, 0 );
                        pos_idle = true;
                    }
                        
                }
            }

            //draw detection areas
            rectangle(cam_mat, obj_detect_area_tl2, obj_detect_area_br2, obj_detect_area_color2, 2, 8, 0 );
            rectangle(cam_mat, obj_detect_area_tl1, obj_detect_area_br1, obj_detect_area_color1, 2, 8, 0 );

            
            //Show Camera Image..
            namedWindow("cam", WINDOW_NORMAL | WINDOW_FREERATIO);
            imshow("cam", cam_mat);
            if (fullscr)
                setWindowProperty("cam", WND_PROP_FULLSCREEN, WINDOW_FULLSCREEN);

            // ******* SERIAL ACTIONS START
            if (pos_scoop)
            {
                pcomms.sendMsg(MSG_SCOOP);
            } else if (pos_pick)
            {
                pcomms.sendMsg(MSG_PICK);
            }
            else
            {
                pcomms.sendMsg(MSG_IDLE);
            }
            // *** SERIAL ACTIONS END
            
        }
        if (waitKey(30) >= 0)
        {
            vector<int> compression_params;
            compression_params.push_back(IMWRITE_PNG_COMPRESSION);
            compression_params.push_back(9);
            
            //Save Last Image..

            if (saveimg)
            {
#ifdef __linux__ 
                imwrite("/home/koos/Desktop/inrange.png", cam_mat, compression_params);
#elif __APPLE__
                imwrite("/Users/koos/Desktop/inrange.png", cam_mat, compression_params);
#else 
                cout << "NO IMAGE SAVED FOR THIS O.S." << endl;
#endif
            }
            break;
        }
    }
    
    return 0;
}
