#include "gui/renderers/FrameBuffer.h"
#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

Bitmap<Rgba> FrameBuffer::motionBlur() const {
    Bitmap<Rgba> blurred(values.size());
    Bitmap<float> depthBuffer(values.size());
    blurred.fill(Rgba::black());
    depthBuffer.fill(LARGE);
    /*ThreadPool& pool = *ThreadPool::getGlobalInstance();
    parallelFor(pool, 0, values.size().y, [this, &blurred](const int y) {*/
    for (int y = 0; y < values.size().y; ++y) {
        for (int x = 0; x < values.size().x; ++x) {
            // Rgba acc(0.f);
            /// \todo this VELOCITY probably needs to be DIFFERENCE IN POSITION BETWEEN FRAMES, there may be
            /// huge spikes in velocity (shockwave) that are immediately damped and the particles does not
            /// move that much
            BasicVector<float> velocity = values[Pixel(x, y)].velocity; // / values[Pixel(x, y)].depth * 1e8;
            /*const float norm = sqrt(dot(velocity, velocity));
            if (norm > 80) {
                velocity *= 80 / norm;
            }*/
            Pixel blur = Pixel(int(velocity[X]), -int(velocity[Y])); /// \todo subpixel?
            if (blur == Pixel(0, 0)) {
                if (values[Pixel(x, y)].depth < depthBuffer[Pixel(x, y)]) {
                    blurred[Pixel(x, y)] = values[Pixel(x, y)].color;
                    depthBuffer[Pixel(x, y)] = values[Pixel(x, y)].depth;
                }
            }
            if (abs(blur.x) > abs(blur.y)) {
                const int dt = blur.x > 0 ? 1 : -1;
                for (int t = 0; t != blur.x; t += dt) {
                    const int x1 = x + t;
                    const int y1 = y + float(t) * blur.y / blur.x;
                    if (x1 >= 0 && y1 >= 0 && x1 < values.size().x && y1 < values.size().y) {
                        // acc += values[Pixel(x1, y1)].color;
                        if (values[Pixel(x1, y1)].depth < depthBuffer[Pixel(x1, y1)]) {
                            blurred[Pixel(x1, y1)] = values[Pixel(x, y)].color; // / abs(blur.x);
                            depthBuffer[Pixel(x1, y1)] = values[Pixel(x1, y1)].depth;
                        }
                    }
                }
            } else {
                const int dt = blur.y > 0 ? 1 : -1;
                for (int t = 0; t != blur.y; t += dt) {
                    const int y1 = y + t;
                    const int x1 = x + float(t) * blur.x / blur.y;
                    if (x1 >= 0 && y1 >= 0 && x1 < values.size().x && y1 < values.size().y) {
                        // acc += values[Pixel(x1, y1)].color;
                        if (values[Pixel(x1, y1)].depth < depthBuffer[Pixel(x1, y1)]) {
                            blurred[Pixel(x1, y1)] = values[Pixel(x, y)].color; // / abs(blur.y);
                            depthBuffer[Pixel(x1, y1)] = values[Pixel(x1, y1)].depth;
                        }
                    }
                }
                // blurred[Pixel(x, y)] = acc / abs(blur.y);
            }
        }
    }

    return blurred;
}

NAMESPACE_SPH_END
