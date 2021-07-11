#include "Sph.h"
#include "gui/objects/Camera.h"
#include <iostream>

using namespace Sph;

int main(int, char**) {
    CameraParams data;
    data.imageSize = Pixel(1000, 1000);
    data.target = Vector(0, 1, 1);
    data.position = Vector(0, 0, 0);
    FisheyeCamera camera(data);
    Optional<CameraRay> ray1 = camera.unproject(Coords(500, 500));
    std::cout << "Center = " << ray1->target - ray1->origin << std::endl;

    Optional<CameraRay> ray2 = camera.unproject(Coords(0, 500));
    std::cout << "Left = " << ray2->target - ray2->origin << std::endl;

    Optional<CameraRay> ray3 = camera.unproject(Coords(500, 0));
    std::cout << "Top = " << ray3->target - ray3->origin << std::endl;

    /*Optional<CameraRay> ray4 = camera.unproject(Coords(500, 250));
    std::cout << "Half Top = " << ray4->target - ray4->origin << std::endl;*/
    return 0;
}
