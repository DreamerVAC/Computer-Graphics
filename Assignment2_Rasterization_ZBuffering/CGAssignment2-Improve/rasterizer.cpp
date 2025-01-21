#include <algorithm>
#include <vector>
#include "rasterizer.hpp"
#include <opencv2/opencv.hpp>
#include <math.h>


rst::pos_buf_id rst::rasterizer::load_positions(const std::vector<Eigen::Vector3f> &positions)
{
    auto id = get_next_id();
    pos_buf.emplace(id, positions);

    return {id};
}

rst::ind_buf_id rst::rasterizer::load_indices(const std::vector<Eigen::Vector3i> &indices)
{
    auto id = get_next_id();
    ind_buf.emplace(id, indices);

    return {id};
}

rst::col_buf_id rst::rasterizer::load_colors(const std::vector<Eigen::Vector3f> &cols)
{
    auto id = get_next_id();
    col_buf.emplace(id, cols);

    return {id};
}

auto to_vec4(const Eigen::Vector3f& v3, float w = 1.0f)
{
    return Vector4f(v3.x(), v3.y(), v3.z(), w);
}


static bool insideTriangle(int x, int y, const Vector3f* _v)
{   
    Vector3f Q = {float(x), float(y), 0};

    Vector3f p0p1 = _v[1] - _v[0];
    Vector3f p0Q = Q - _v[0];

    Vector3f p1p2 = _v[2] - _v[1];
    Vector3f p1Q = Q - _v[1];

    Vector3f p2p0 = _v[0] - _v[2];
    Vector3f p2Q = Q - _v[2];

    //The class definition is already counterclockwise, so we only have to consider the same positive case
    return p0p1.cross(p0Q).z() > 0 && p1p2.cross(p1Q).z() > 0 && p2p0.cross(p2Q).z()>0;
    
}

static std::tuple<float, float, float> computeBarycentric2D(float x, float y, const Vector3f* v)
{
    float c1 = (x*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*y + v[1].x()*v[2].y() - v[2].x()*v[1].y()) / (v[0].x()*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*v[0].y() + v[1].x()*v[2].y() - v[2].x()*v[1].y());
    float c2 = (x*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*y + v[2].x()*v[0].y() - v[0].x()*v[2].y()) / (v[1].x()*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*v[1].y() + v[2].x()*v[0].y() - v[0].x()*v[2].y());
    float c3 = (x*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*y + v[0].x()*v[1].y() - v[1].x()*v[0].y()) / (v[2].x()*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*v[2].y() + v[0].x()*v[1].y() - v[1].x()*v[0].y());
    return {c1,c2,c3};
}

void rst::rasterizer::draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer, col_buf_id col_buffer, Primitive type)
{
    auto& buf = pos_buf[pos_buffer.pos_id];
    auto& ind = ind_buf[ind_buffer.ind_id];
    auto& col = col_buf[col_buffer.col_id];

    float f1 = (50 - 0.1) / 2.0;
    float f2 = (50 + 0.1) / 2.0;

    Eigen::Matrix4f mvp = projection * view * model;
    for (auto& i : ind)
    {
        Triangle t;
        Eigen::Vector4f v[] = {
                mvp * to_vec4(buf[i[0]], 1.0f),
                mvp * to_vec4(buf[i[1]], 1.0f),
                mvp * to_vec4(buf[i[2]], 1.0f)
        };
        //Homogeneous division
        for (auto& vec : v) {
            vec /= vec.w();
        }
        //Viewport transformation
        for (auto & vert : v)
        {
            vert.x() = 0.5*width*(vert.x()+1.0);
            vert.y() = 0.5*height*(vert.y()+1.0);
            vert.z() = -vert.z() * f1 + f2;
        }

        for (int i = 0; i < 3; ++i)
        {
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
        }

        auto col_x = col[i[0]];
        auto col_y = col[i[1]];
        auto col_z = col[i[2]];

        t.setColor(0, col_x[0], col_x[1], col_x[2]);
        t.setColor(1, col_y[0], col_y[1], col_y[2]);
        t.setColor(2, col_z[0], col_z[1], col_z[2]);

        rasterize_triangle(t);
    }
}





//Screen space rasterization
void rst::rasterizer::rasterize_triangle(const Triangle& t) {
    auto v = t.toVector4();
    
    // Task_improve: Find out the bounding box of current triangle.
    // iterate through the pixel and find if the current pixel is inside the triangle

    // If so, use the following code to get the interpolated z value.

    //auto[alpha, beta, gamma] = computeBarycentric2D(x, y, t.v);
    //float w_reciprocal = 1.0/(alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
    //float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
    //z_interpolated *= w_reciprocal;

    // TODO : set the current pixel (use the set_pixel function) to the color of the triangle (use getColor function) if it should be painted.

    int super_sample_rate = 2;

    float min_x = std::min({ v[0].x(), v[1].x(), v[2].x() });
    float max_x = std::max({ v[0].x(), v[1].x(), v[2].x() });
    float min_y = std::min({ v[0].y(), v[1].y(), v[2].y() });
    float max_y = std::max({ v[0].y(), v[1].y(), v[2].y() });

    int x_min = static_cast<int>(std::floor(min_x));
    int x_max = static_cast<int>(std::ceil(max_x));
    int y_min = static_cast<int>(std::floor(min_y));
    int y_max = static_cast<int>(std::ceil(max_y));
    
    for (int x = x_min; x <= x_max; x++) {
        for (int y = y_min; y <= y_max; y++) {
            std::vector<float> sample_depths;
            std::vector<Eigen::Vector3f> sample_colors;
            for (int sx = 0; sx < super_sample_rate; sx++) {
                for (int sy = 0; sy < super_sample_rate; sy++) {
                    float sub_x = x + (sx + 0.5) / super_sample_rate;
                    float sub_y = y + (sy + 0.5) / super_sample_rate;
                    if (!insideTriangle(sub_x, sub_y, t.v)) {
                        continue;
                    }
                    auto [alpha, beta, gamma] = computeBarycentric2D(sub_x, sub_y, t.v);
                    if (alpha >= 0 && beta >= 0 && gamma >= 0) {
                        float w_reciprocal = 1.0f / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
                        float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
                        z_interpolated *= w_reciprocal;
                        int mind = get_index(x, y) + sx * 2 + sy;
                        if (my_depth_buf[mind] > z_interpolated) {
                            depth_buf[mind] = z_interpolated;
                            my_col_buf[mind] = t.getColor() / (super_sample_rate * super_sample_rate);
                            my_depth_buf[mind] = z_interpolated;
                        }
                    }
                }
            }
            int index = get_index(x, y);
            Vector3f inserted_color = { 0,0,0 };
            for (int i = 0; i < super_sample_rate * super_sample_rate; i++) {
                inserted_color += my_col_buf[index + i];
            }
            Eigen::Vector3f point = Eigen::Vector3f(x, y, 1.0f);
            set_pixel(point,inserted_color);
        }
    }



}

void rst::rasterizer::set_model(const Eigen::Matrix4f& m)
{
    model = m;
}

void rst::rasterizer::set_view(const Eigen::Matrix4f& v)
{
    view = v;
}

void rst::rasterizer::set_projection(const Eigen::Matrix4f& p)
{
    projection = p;
}

void rst::rasterizer::clear(rst::Buffers buff)
{
    if ((buff & rst::Buffers::Color) == rst::Buffers::Color)
    {
        std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f{0, 0, 0});
        std::fill(my_col_buf.begin(), my_col_buf.end(), Eigen::Vector3f{ 0,0,0 });
    }
    if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth)
    {
        std::fill(depth_buf.begin(), depth_buf.end(), std::numeric_limits<float>::infinity());
        std::fill(my_depth_buf.begin(), my_depth_buf.end(), std::numeric_limits<float>::infinity());
    }
}

rst::rasterizer::rasterizer(int w, int h) : width(w), height(h)
{
    frame_buf.resize(w * h);
    depth_buf.resize(w * h);
    my_col_buf.resize(w * h * 2);
    my_depth_buf.resize(w * h * 2);
}

int rst::rasterizer::get_index(int x, int y)
{
    return (height-1-y)*width + x;
}

int rst::rasterizer::get_super_index(int x, int y)
{
    return (height*2 - 1 - y) * width*2 + x;
}



void rst::rasterizer::set_pixel(const Eigen::Vector3f& point, const Eigen::Vector3f& color)
{
    //old index: auto ind = point.y() + point.x() * width;
    auto ind = (height-1-point.y())*width + point.x();
    frame_buf[ind] = color;

}

// clang-format on